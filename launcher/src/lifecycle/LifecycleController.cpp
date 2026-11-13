#include "LifecycleController.h"

#include <QDateTime>

#include <QLoggingCategory>
#include <QRegularExpression>
#include <QStandardPaths>

#include <csignal>

Q_LOGGING_CATEGORY(lcLife, "htl.lifecycle")

LifecycleController::LifecycleController(const Config *config, UnitBackend *backend, WindowSource *windows,
                                         NameSource *names, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_backend(backend)
    , m_windows(windows)
    , m_names(names)
{
    m_startTimer.setSingleShot(true);
    m_stopTimer.setSingleShot(true);
    connect(&m_startTimer, &QTimer::timeout, this, [this] {
        if (m_state == State::Launching)
            failStart(QStringLiteral("%1 did not become ready within %2 s").arg(m_app.label).arg(m_app.startTimeout));
    });
    connect(&m_stopTimer, &QTimer::timeout, this, [this] { onStopDeadline(m_op); });

    connect(m_backend, &UnitBackend::unitChanged, this, [this](int bus, const QString &name) {
        if (bus == UnitBackend::User && name == m_app.unitName())
            refreshUnitState();
    });
    connect(m_windows, &WindowSource::windowsChanged, this, &LifecycleController::evaluate);
    connect(m_names, &NameSource::namesChanged, this, &LifecycleController::evaluate);
}

QString LifecycleController::stateName() const
{
    switch (m_state) {
    case State::Launcher: return QStringLiteral("launcher");
    case State::Launching: return QStringLiteral("launching");
    case State::Running: return QStringLiteral("running");
    case State::Closing: return QStringLiteral("closing");
    case State::Error: return QStringLiteral("error");
    }
    return {};
}

QString LifecycleController::errorKind() const
{
    switch (m_errorKind) {
    case ErrorKind::StartFailed: return QStringLiteral("start");
    case ErrorKind::StopIncomplete: return QStringLiteral("stop");
    case ErrorKind::None: break;
    }
    return {};
}

QString LifecycleController::errorTitle() const
{
    switch (m_errorKind) {
    case ErrorKind::StartFailed: return QStringLiteral("%1 didn’t start").arg(m_app.label);
    case ErrorKind::StopIncomplete: return QStringLiteral("%1 is still closing").arg(m_app.label);
    case ErrorKind::None: break;
    }
    return {};
}

void LifecycleController::setState(State s)
{
    if (s == m_state)
        return;
    qCInfo(lcLife) << "state" << stateName() << "->" << [s] {
        switch (s) {
        case State::Launcher: return "launcher";
        case State::Launching: return "launching";
        case State::Running: return "running";
        case State::Closing: return "closing";
        case State::Error: return "error";
        }
        return "";
    }() << m_app.id;
    m_state = s;
    if (s == State::Running)
        m_runningSince = QDateTime::currentMSecsSinceEpoch();
    else if (s != State::Closing)
        m_runningSince = 0;
    Q_EMIT changed();
}

// ---------------------------------------------------------------- startup

void LifecycleController::startup()
{
    m_backend->listActiveUnits(UnitBackend::User, QStringLiteral("htl-app-*.service"), [this](const QStringList &units) {
        QString adopt;
        for (const QString &unit : units) {
            QString id = unit;
            id.remove(QStringLiteral("htl-app-")).remove(QStringLiteral(".service"));
            if (adopt.isEmpty() && m_config->app(id)) {
                adopt = id;
                continue;
            }
            qCInfo(lcLife) << "stopping stray unit" << unit;
            m_backend->stopUnit(UnitBackend::User, unit, [](bool, const QString &) {});
        }
        if (adopt.isEmpty()) {
            stopStraySystemUnits();
            return;
        }
        qCInfo(lcLife) << "adopting running app" << adopt;
        m_app = *m_config->app(adopt);
        for (int i = 0; i < m_config->apps().size(); ++i)
            if (m_config->apps().at(i).id == adopt)
                m_lastIndex = i;
        ++m_op;
        m_unitStarted = true;
        m_unitKnown = false;
        m_baseline.clear();
        m_sawWindow = matchingWindow().has_value();
        m_names->watchName(m_app.readyDbusName);
        m_sawName = !m_app.readyDbusName.isEmpty() && m_names->hasName(m_app.readyDbusName);
        m_backend->watch(UnitBackend::User, m_app.unitName());
        setState(State::Running);
        refreshUnitState();
    });
}

void LifecycleController::stopStraySystemUnits()
{
    // Nothing runs: no system unit belonging to any app may stay active
    // (e.g. no Android container behind the home screen).
    for (const AppSpec &a : m_config->apps()) {
        for (const QString &u : a.systemUnits) {
            m_backend->unitState(UnitBackend::System, u, [this, u](const UnitBackend::UnitState &st) {
                if (st.isRunning() && m_state == State::Launcher) {
                    qCInfo(lcLife) << "stopping idle system unit" << u;
                    m_backend->stopUnit(UnitBackend::System, u, [](bool, const QString &) {});
                }
            });
        }
    }
}

// ---------------------------------------------------------------- launching

void LifecycleController::launch(const QString &id)
{
    if (m_state != State::Launcher)
        return;
    const AppSpec *spec = m_config->app(id);
    if (!spec)
        return;
    for (int i = 0; i < m_config->apps().size(); ++i)
        if (m_config->apps().at(i).id == id)
            m_lastIndex = i;

    if (m_cleanupIncomplete) {
        m_app = m_blockedApp;
        m_errorKind = ErrorKind::StopIncomplete;
        setState(State::Error);
        Q_EMIT changed();
        return;
    }

    m_app = *spec;
    m_errorKind = ErrorKind::None;
    m_pendingError = ErrorKind::None;
    m_errorReason.clear();
    m_errorDetails.clear();
    m_unitStarted = false;
    m_unitKnown = false;
    m_unit = {};
    m_sawWindow = false;
    m_sawName = false;
    m_baseline.clear();
    for (const WindowInfo &w : m_windows->windows())
        m_baseline.insert(w.id);
    m_names->watchName(m_app.readyDbusName);

    const int op = ++m_op;
    setState(State::Launching);
    m_startTimer.start(m_app.startTimeout * 1000);

    runHook(m_app.preStart, op, [this, op](bool ok, const QString &out) {
        if (!ok) {
            failStart(QStringLiteral("preStart hook failed: %1").arg(out));
            return;
        }
        startSystemUnits(0, op);
    });
}

void LifecycleController::runHook(const QStringList &argv, int op, std::function<void(bool, const QString &)> next)
{
    if (argv.isEmpty()) {
        next(true, QString());
        return;
    }
    m_backend->runCommand(argv, m_app.environment, m_app.hookTimeout, [this, op, next](bool ok, const QString &out) {
        if (alive(op))
            next(ok, out);
    });
}

void LifecycleController::startSystemUnits(int index, int op)
{
    if (index >= m_app.systemUnits.size()) {
        startApp(op);
        return;
    }
    const QString unit = m_app.systemUnits.at(index);
    m_backend->watch(UnitBackend::System, unit);
    m_backend->startUnit(UnitBackend::System, unit, [this, op, index, unit](bool ok, const QString &err) {
        if (!alive(op))
            return;
        if (!ok) {
            failStart(QStringLiteral("could not start %1: %2").arg(unit, err));
            return;
        }
        startSystemUnits(index + 1, op);
    });
}

void LifecycleController::startApp(int op)
{
    m_backend->resetFailed(UnitBackend::User, m_app.unitName());
    UnitBackend::TransientSpec spec;
    spec.name = m_app.unitName();
    spec.description = QStringLiteral("Home Theatre: %1").arg(m_app.label);
    spec.argv = m_app.command;
    spec.environment = m_app.environment;
    spec.workingDirectory = m_app.workingDirectory;
    spec.stopTimeoutSec = m_app.stopTimeout;
    m_backend->startTransient(spec, [this, op](bool ok, const QString &err) {
        if (!alive(op))
            return;
        if (!ok) {
            m_unitStarted = true;
            failStart(QStringLiteral("could not start %1: %2").arg(m_app.label, err));
            return;
        }
        m_unitStarted = true;
        refreshUnitState();
    });
}

void LifecycleController::refreshUnitState()
{
    if (m_app.id.isEmpty())
        return;
    const int op = m_op;
    m_backend->unitState(UnitBackend::User, m_app.unitName(), [this, op](const UnitBackend::UnitState &st) {
        if (!alive(op))
            return;
        m_unit = st;
        m_unitKnown = true;
        evaluate();
    });
}

bool LifecycleController::windowMatches(const WindowInfo &w) const
{
    if (m_app.readyAnyWindow && !m_baseline.contains(w.id))
        return true;
    if (!m_app.readyAppId.isEmpty() && w.appId == m_app.readyAppId)
        return true;
    if (!m_app.readyAppIdRegex.isEmpty() && QRegularExpression(m_app.readyAppIdRegex).match(w.appId).hasMatch())
        return true;
    if (!m_app.readyTitleRegex.isEmpty() && QRegularExpression(m_app.readyTitleRegex).match(w.title).hasMatch())
        return true;
    return false;
}

std::optional<WindowInfo> LifecycleController::matchingWindow() const
{
    for (const WindowInfo &w : m_windows->windows())
        if (windowMatches(w))
            return w;
    return std::nullopt;
}

void LifecycleController::evaluate()
{
    if (m_state == State::Launching) {
        if (!m_unitStarted || !m_unitKnown)
            return;
        if (m_unit.isStopped() && m_app.exitOnUnitExit) {
            failStart(QStringLiteral("%1 %2").arg(m_app.label, m_unit.describe()));
            return;
        }
        const bool unitOk = m_unit.active == QLatin1String("active") || !m_app.exitOnUnitExit;
        const std::optional<WindowInfo> win = m_app.hasWindowRule() ? matchingWindow() : std::nullopt;
        const bool windowOk = !m_app.hasWindowRule() || !m_windows->available() || win.has_value();
        const bool nameOk = m_app.readyDbusName.isEmpty() || m_names->hasName(m_app.readyDbusName);
        if (unitOk && windowOk && nameOk) {
            m_startTimer.stop();
            m_sawWindow = win.has_value();
            m_sawName = !m_app.readyDbusName.isEmpty();
            setState(State::Running);
            if (win)
                m_windows->activate(win->id);
        }
        return;
    }

    if (m_state == State::Running) {
        if (m_unitKnown && m_unit.isStopped() && m_app.exitOnUnitExit) {
            qCInfo(lcLife) << m_app.id << "exited:" << m_unit.describe();
            beginClose();
            return;
        }
        if (m_app.exitOnWindowClosed && m_app.hasWindowRule() && m_windows->available()) {
            if (matchingWindow())
                m_sawWindow = true;
            else if (m_sawWindow) {
                qCInfo(lcLife) << m_app.id << "window closed";
                beginClose();
                return;
            }
        }
        if (m_app.exitOnDbusNameLost && !m_app.readyDbusName.isEmpty()) {
            if (m_names->hasName(m_app.readyDbusName))
                m_sawName = true;
            else if (m_sawName) {
                qCInfo(lcLife) << m_app.id << "lost bus name" << m_app.readyDbusName;
                beginClose();
                return;
            }
        }
    }
}

void LifecycleController::failStart(const QString &reason)
{
    qCWarning(lcLife) << "start failed:" << reason;
    m_pendingError = ErrorKind::StartFailed;
    m_errorReason = reason;
    m_errorDetails = reason;
    fetchJournal();
    beginClose();
}

void LifecycleController::fetchJournal()
{
    const QString journalctl = QStandardPaths::findExecutable(QStringLiteral("journalctl"));
    if (journalctl.isEmpty() || !m_unitStarted)
        return;
    const QString unit = m_app.unitName();
    m_backend->runCommand({journalctl, QStringLiteral("--user"), QStringLiteral("-u"), unit, QStringLiteral("-n"),
                           QStringLiteral("15"), QStringLiteral("-o"), QStringLiteral("cat"),
                           QStringLiteral("--no-pager")},
                          {}, 3, [this, unit](bool, const QString &out) {
                              if (m_app.unitName() != unit || out.trimmed().isEmpty())
                                  return;
                              m_errorDetails += QStringLiteral("\n\n") + out.trimmed();
                              Q_EMIT changed();
                          });
}

// ---------------------------------------------------------------- closing

void LifecycleController::home()
{
    switch (m_state) {
    case State::Launching:
    case State::Running:
        beginClose();
        break;
    case State::Launcher:
        Q_EMIT homeAtLauncher();
        break;
    case State::Error:
        dismiss();
        break;
    case State::Closing:
        break;
    }
}

void LifecycleController::beginClose()
{
    if (m_state == State::Closing)
        return;
    const int op = ++m_op;
    m_startTimer.stop();
    setState(State::Closing);
    // Budget: stop command + unit stop (systemd escalates to SIGKILL) + grace.
    m_stopTimer.start((m_app.stopTimeout * 2 + 5) * 1000);

    auto stopUnit = [this, op] {
        m_backend->stopUnit(UnitBackend::User, m_app.unitName(), [this, op](bool ok, const QString &err) {
            if (!alive(op))
                return;
            if (!ok)
                qCWarning(lcLife) << "stop job for" << m_app.unitName() << "ended:" << err;
            stopSystemUnits(m_app.systemUnits.size() - 1, op);
        });
    };
    if (m_app.stopCommand.isEmpty()) {
        stopUnit();
        return;
    }
    m_backend->runCommand(m_app.stopCommand, m_app.environment, m_app.stopTimeout,
                          [this, op, stopUnit](bool ok, const QString &out) {
                              if (!alive(op))
                                  return;
                              if (!ok)
                                  qCWarning(lcLife) << "stopCommand failed:" << out;
                              stopUnit();
                          });
}

void LifecycleController::stopSystemUnits(int index, int op)
{
    if (index < 0) {
        verifyStopped(op);
        return;
    }
    const QString unit = m_app.systemUnits.at(index);
    m_backend->stopUnit(UnitBackend::System, unit, [this, op, index, unit](bool ok, const QString &err) {
        if (!alive(op))
            return;
        if (!ok)
            qCWarning(lcLife) << "stop job for" << unit << "ended:" << err;
        stopSystemUnits(index - 1, op);
    });
}

void LifecycleController::verifyStopped(int op)
{
    // Collect the state of the app unit and every system unit, then decide.
    struct Check {
        int pending = 0;
        QStringList stillRunning;
    };
    auto check = std::make_shared<Check>();
    const auto units = QList<QPair<UnitBackend::Bus, QString>>{{UnitBackend::User, m_app.unitName()}}
        + [this] {
              QList<QPair<UnitBackend::Bus, QString>> l;
              for (const QString &u : m_app.systemUnits)
                  l << qMakePair(UnitBackend::System, u);
              return l;
          }();
    check->pending = units.size();
    auto decide = [this, op, check] {
        if (!alive(op))
            return;
        if (!m_app.readyDbusName.isEmpty() && m_names->hasName(m_app.readyDbusName))
            check->stillRunning << m_app.readyDbusName;
        if (check->stillRunning.isEmpty()) {
            finishClose(op);
            return;
        }
        qCInfo(lcLife) << "still running:" << check->stillRunning;
        m_errorDetails = QStringLiteral("Still running: %1").arg(check->stillRunning.join(QStringLiteral(", ")));
        // Re-check shortly; bounded by the stop deadline.
        QTimer::singleShot(500, this, [this, op] {
            if (alive(op))
                verifyStopped(op);
        });
    };
    for (const auto &[bus, unit] : units) {
        m_backend->unitState(bus, unit, [check, decide, unit](const UnitBackend::UnitState &st) {
            if (st.isRunning())
                check->stillRunning << unit;
            if (--check->pending == 0)
                decide();
        });
    }
}

void LifecycleController::finishClose(int op)
{
    m_stopTimer.stop();
    runHook(m_app.postStop, op, [this](bool ok, const QString &out) {
        if (!ok) {
            qCWarning(lcLife) << "postStop hook failed:" << out;
            m_errorDetails += QStringLiteral("\npostStop hook failed: %1").arg(out);
        }
        m_cleanupIncomplete = false;
        m_unitStarted = false;
        if (m_pendingError == ErrorKind::StartFailed) {
            m_errorKind = ErrorKind::StartFailed;
            m_pendingError = ErrorKind::None;
            setState(State::Error);
        } else {
            m_errorKind = ErrorKind::None;
            setState(State::Launcher);
            stopStraySystemUnits();
        }
        Q_EMIT changed();
    });
}

void LifecycleController::onStopDeadline(int op)
{
    if (!alive(op) || m_state != State::Closing)
        return;
    ++m_op; // cancel the running stop chain
    qCWarning(lcLife) << m_app.id << "did not stop in time";
    m_cleanupIncomplete = true;
    m_blockedApp = m_app;
    m_errorKind = ErrorKind::StopIncomplete;
    m_pendingError = ErrorKind::None;
    if (m_errorDetails.isEmpty())
        m_errorDetails = QStringLiteral("%1 did not stop within %2 s").arg(m_app.label).arg(m_app.stopTimeout * 2 + 5);
    setState(State::Error);
    Q_EMIT changed();
}

// ---------------------------------------------------------------- error actions

void LifecycleController::retry()
{
    if (m_state != State::Error || m_errorKind != ErrorKind::StartFailed)
        return;
    const QString id = m_app.id;
    setState(State::Launcher);
    launch(id);
}

void LifecycleController::stopAgain()
{
    if (m_state != State::Error || m_errorKind != ErrorKind::StopIncomplete)
        return;
    m_backend->killUnit(UnitBackend::User, m_app.unitName(), SIGKILL);
    m_errorDetails.clear();
    beginClose();
}

void LifecycleController::dismiss()
{
    if (m_state != State::Error)
        return;
    setState(State::Launcher);
}

void LifecycleController::debugShow(const QString &state, const QString &appId, const QString &errorKind)
{
    if (const AppSpec *a = m_config->app(appId))
        m_app = *a;
    m_errorKind = errorKind == QLatin1String("start") ? ErrorKind::StartFailed
        : errorKind == QLatin1String("stop")          ? ErrorKind::StopIncomplete
                                                      : ErrorKind::None;
    m_errorDetails = m_errorKind == ErrorKind::StartFailed
        ? QStringLiteral("%1 exited with status 1 (exit-code)").arg(m_app.label)
        : QStringLiteral("Still running: htl-app-%1.service").arg(m_app.id);
    static const QHash<QString, State> states{{"launcher", State::Launcher}, {"launching", State::Launching},
                                              {"running", State::Running}, {"closing", State::Closing},
                                              {"error", State::Error}};
    m_state = states.value(state, State::Launcher);
    m_runningSince = m_state == State::Running ? QDateTime::currentMSecsSinceEpoch() - 84 * 60 * 1000 : 0;
    Q_EMIT changed();
}
