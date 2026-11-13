#include "SystemdBackend.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

Q_LOGGING_CATEGORY(lcSystemd, "htl.systemd")

namespace {

const QString kService = QStringLiteral("org.freedesktop.systemd1");
const QString kPath = QStringLiteral("/org/freedesktop/systemd1");
const QString kManager = QStringLiteral("org.freedesktop.systemd1.Manager");
const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");

struct SdProperty {
    QString name;
    QDBusVariant value;
};
struct SdAux {
    QString name;
    QList<SdProperty> properties;
};
struct SdExec {
    QString path;
    QStringList argv;
    bool ignoreFailure = false;
};
struct SdUnitInfo {
    QString name, description, loadState, activeState, subState, following;
    QDBusObjectPath path;
    uint jobId = 0;
    QString jobType;
    QDBusObjectPath jobPath;
};

} // namespace

Q_DECLARE_METATYPE(SdProperty)
Q_DECLARE_METATYPE(SdAux)
Q_DECLARE_METATYPE(SdExec)
Q_DECLARE_METATYPE(SdUnitInfo)

QDBusArgument &operator<<(QDBusArgument &a, const SdProperty &p)
{
    a.beginStructure();
    a << p.name << p.value;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, SdProperty &p)
{
    a.beginStructure();
    a >> p.name >> p.value;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const SdAux &x)
{
    a.beginStructure();
    a << x.name << x.properties;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, SdAux &x)
{
    a.beginStructure();
    a >> x.name >> x.properties;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const SdExec &e)
{
    a.beginStructure();
    a << e.path << e.argv << e.ignoreFailure;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, SdExec &e)
{
    a.beginStructure();
    a >> e.path >> e.argv >> e.ignoreFailure;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const SdUnitInfo &u)
{
    a.beginStructure();
    a << u.name << u.description << u.loadState << u.activeState << u.subState << u.following
      << u.path << u.jobId << u.jobType << u.jobPath;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, SdUnitInfo &u)
{
    a.beginStructure();
    a >> u.name >> u.description >> u.loadState >> u.activeState >> u.subState >> u.following
      >> u.path >> u.jobId >> u.jobType >> u.jobPath;
    a.endStructure();
    return a;
}

QString UnitBackend::UnitState::describe() const
{
    if (execCode == 1)
        return QStringLiteral("exited with status %1 (%2)").arg(execStatus).arg(result);
    if (execCode == 2 || execCode == 3)
        return QStringLiteral("killed by signal %1 (%2)").arg(execStatus).arg(result);
    return QStringLiteral("%1/%2 (%3)").arg(active, sub, result);
}

SystemdBackend::SystemdBackend(QObject *parent)
    : UnitBackend(parent)
{
    qDBusRegisterMetaType<SdProperty>();
    qDBusRegisterMetaType<QList<SdProperty>>();
    qDBusRegisterMetaType<SdAux>();
    qDBusRegisterMetaType<QList<SdAux>>();
    qDBusRegisterMetaType<SdExec>();
    qDBusRegisterMetaType<QList<SdExec>>();
    qDBusRegisterMetaType<SdUnitInfo>();
    qDBusRegisterMetaType<QList<SdUnitInfo>>();

    for (Bus bus : {User, System}) {
        QDBusConnection c = connection(bus);
        if (!c.isConnected()) {
            qCWarning(lcSystemd) << "no" << (bus == User ? "session" : "system") << "bus connection";
            continue;
        }
        c.connect(kService, kPath, kManager, QStringLiteral("JobRemoved"), this,
                  bus == User ? SLOT(onUserJobRemoved(uint, QDBusObjectPath, QString, QString))
                              : SLOT(onSystemJobRemoved(uint, QDBusObjectPath, QString, QString)));
        c.asyncCall(QDBusMessage::createMethodCall(kService, kPath, kManager, QStringLiteral("Subscribe")));
    }
}

QDBusConnection SystemdBackend::connection(Bus bus) const
{
    return bus == User ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
}

QDBusMessage SystemdBackend::managerCall(const QString &method) const
{
    return QDBusMessage::createMethodCall(kService, kPath, kManager, method);
}

QString SystemdBackend::unitPath(const QString &name)
{
    // sd_bus_path_encode(): every byte outside [A-Za-z0-9] (and a leading digit) becomes _xx.
    QString out = QStringLiteral("/org/freedesktop/systemd1/unit/");
    const QByteArray bytes = name.toUtf8();
    for (int i = 0; i < bytes.size(); ++i) {
        const char ch = bytes.at(i);
        const bool alnum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        if (alnum && !(i == 0 && ch >= '0' && ch <= '9'))
            out += QLatin1Char(ch);
        else
            out += QStringLiteral("_%1").arg(quint8(ch), 2, 16, QLatin1Char('0'));
    }
    return out;
}

void SystemdBackend::callJob(Bus bus, const QDBusMessage &msg, Done done, bool missingIsOk)
{
    auto *w = new QDBusPendingCallWatcher(connection(bus).asyncCall(msg, 25000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, bus, done, missingIsOk](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            const QString err = reply.error().name();
            if (missingIsOk && (err.endsWith(QLatin1String("NoSuchUnit")) || err.endsWith(QLatin1String("LoadFailed")))) {
                done(true, QString());
                return;
            }
            done(false, reply.error().message());
            return;
        }
        const QString job = reply.value().path();
        if (m_pendingResults[bus].contains(job)) {
            const QString result = m_pendingResults[bus].take(job);
            done(result == QLatin1String("done"), result);
            return;
        }
        m_jobs[bus].insert(job, done);
    });
}

void SystemdBackend::startTransient(const TransientSpec &spec, Done done)
{
    QList<SdProperty> props;
    auto add = [&props](const QString &n, const QVariant &v) { props << SdProperty{n, QDBusVariant(v)}; };
    add(QStringLiteral("Description"), spec.description);
    add(QStringLiteral("Type"), QStringLiteral("exec"));
    add(QStringLiteral("ExecStart"), QVariant::fromValue(QList<SdExec>{SdExec{spec.argv.first(), spec.argv, false}}));
    if (!spec.environment.isEmpty())
        add(QStringLiteral("Environment"), spec.environment);
    if (!spec.workingDirectory.isEmpty())
        add(QStringLiteral("WorkingDirectory"), spec.workingDirectory);
    add(QStringLiteral("KillMode"), QStringLiteral("control-group"));
    add(QStringLiteral("SendSIGKILL"), true);
    add(QStringLiteral("TimeoutStopUSec"), QVariant::fromValue<quint64>(quint64(spec.stopTimeoutSec) * 1000000ULL));
    add(QStringLiteral("Restart"), QStringLiteral("no"));

    QDBusMessage msg = managerCall(QStringLiteral("StartTransientUnit"));
    msg << spec.name << QStringLiteral("fail") << QVariant::fromValue(props)
        << QVariant::fromValue(QList<SdAux>{});
    watch(User, spec.name);
    callJob(User, msg, std::move(done), false);
}

void SystemdBackend::startUnit(Bus bus, const QString &name, Done done)
{
    QDBusMessage msg = managerCall(QStringLiteral("StartUnit"));
    msg << name << QStringLiteral("replace");
    callJob(bus, msg, std::move(done), false);
}

void SystemdBackend::stopUnit(Bus bus, const QString &name, Done done)
{
    QDBusMessage msg = managerCall(QStringLiteral("StopUnit"));
    msg << name << QStringLiteral("replace");
    callJob(bus, msg, std::move(done), true);
}

void SystemdBackend::killUnit(Bus bus, const QString &name, int signal)
{
    QDBusMessage msg = managerCall(QStringLiteral("KillUnit"));
    msg << name << QStringLiteral("all") << signal;
    connection(bus).asyncCall(msg);
}

void SystemdBackend::resetFailed(Bus bus, const QString &name)
{
    QDBusMessage msg = managerCall(QStringLiteral("ResetFailedUnit"));
    msg << name;
    // Synchronous on purpose: must complete before a transient unit of the same name is created.
    connection(bus).call(msg, QDBus::Block, 5000);
}

void SystemdBackend::unitState(Bus bus, const QString &name, StateCb cb)
{
    const QString path = unitPath(name);
    QDBusMessage unitMsg = QDBusMessage::createMethodCall(kService, path, kProps, QStringLiteral("GetAll"));
    unitMsg << QStringLiteral("org.freedesktop.systemd1.Unit");
    auto *w = new QDBusPendingCallWatcher(connection(bus).asyncCall(unitMsg, 5000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, bus, path, cb](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *w;
        UnitState st;
        if (reply.isError()) {
            cb(st);
            return;
        }
        const QVariantMap u = reply.value();
        st.loaded = u.value(QStringLiteral("LoadState")).toString() == QLatin1String("loaded");
        st.active = u.value(QStringLiteral("ActiveState")).toString();
        st.sub = u.value(QStringLiteral("SubState")).toString();
        if (!st.loaded) {
            cb(st);
            return;
        }
        QDBusMessage svcMsg = QDBusMessage::createMethodCall(kService, path, kProps, QStringLiteral("GetAll"));
        svcMsg << QStringLiteral("org.freedesktop.systemd1.Service");
        auto *w2 = new QDBusPendingCallWatcher(connection(bus).asyncCall(svcMsg, 5000), this);
        connect(w2, &QDBusPendingCallWatcher::finished, this, [st, cb](QDBusPendingCallWatcher *w2) mutable {
            w2->deleteLater();
            QDBusPendingReply<QVariantMap> r = *w2;
            if (!r.isError()) {
                const QVariantMap s = r.value();
                st.result = s.value(QStringLiteral("Result")).toString();
                st.execCode = s.value(QStringLiteral("ExecMainCode")).toInt();
                st.execStatus = s.value(QStringLiteral("ExecMainStatus")).toInt();
            }
            cb(st);
        });
    });
}

void SystemdBackend::listActiveUnits(Bus bus, const QString &pattern, ListCb cb)
{
    QDBusMessage msg = managerCall(QStringLiteral("ListUnitsByPatterns"));
    msg << QStringList{} << QStringList{pattern};
    auto *w = new QDBusPendingCallWatcher(connection(bus).asyncCall(msg, 5000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [cb](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QList<SdUnitInfo>> reply = *w;
        QStringList out;
        if (!reply.isError())
            for (const auto &u : reply.value())
                if (u.activeState != QLatin1String("inactive") && u.activeState != QLatin1String("failed"))
                    out << u.name;
        cb(out);
    });
}

void SystemdBackend::watch(Bus bus, const QString &name)
{
    const QString path = unitPath(name);
    if (m_watched[bus].contains(path))
        return;
    m_watched[bus].insert(path, name);
    connection(bus).connect(kService, path, kProps, QStringLiteral("PropertiesChanged"), this,
                            bus == User ? SLOT(onUserPropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage))
                                        : SLOT(onSystemPropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
}

void SystemdBackend::onUserPropertiesChanged(const QString &, const QVariantMap &, const QStringList &,
                                             const QDBusMessage &msg)
{
    if (const QString name = m_watched[User].value(msg.path()); !name.isEmpty())
        Q_EMIT unitChanged(User, name);
}

void SystemdBackend::onSystemPropertiesChanged(const QString &, const QVariantMap &, const QStringList &,
                                               const QDBusMessage &msg)
{
    if (const QString name = m_watched[System].value(msg.path()); !name.isEmpty())
        Q_EMIT unitChanged(System, name);
}

void SystemdBackend::onUserJobRemoved(uint, const QDBusObjectPath &job, const QString &, const QString &result)
{
    jobRemoved(User, job.path(), result);
}

void SystemdBackend::onSystemJobRemoved(uint, const QDBusObjectPath &job, const QString &, const QString &result)
{
    jobRemoved(System, job.path(), result);
}

void SystemdBackend::jobRemoved(Bus bus, const QString &job, const QString &result)
{
    if (auto it = m_jobs[bus].find(job); it != m_jobs[bus].end()) {
        Done done = it.value();
        m_jobs[bus].erase(it);
        done(result == QLatin1String("done"), result);
        return;
    }
    // Reply may still be in flight; remember a bounded number of results.
    if (m_pendingResults[bus].size() > 64)
        m_pendingResults[bus].clear();
    m_pendingResults[bus].insert(job, result);
}

void SystemdBackend::runCommand(const QStringList &argv, const QStringList &environment, int timeoutSec,
                                CommandCb cb)
{
    if (argv.isEmpty()) {
        cb(true, QString());
        return;
    }
    auto *proc = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &kv : environment) {
        const int eq = kv.indexOf(QLatin1Char('='));
        if (eq > 0)
            env.insert(kv.left(eq), kv.mid(eq + 1));
    }
    proc->setProcessEnvironment(env);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    auto *timer = new QTimer(proc);
    timer->setSingleShot(true);
    auto finished = std::make_shared<bool>(false);
    auto finish = [proc, cb, finished](bool ok, const QString &extra) {
        if (*finished)
            return;
        *finished = true;
        QString out = QString::fromUtf8(proc->readAll()).right(4096);
        if (!extra.isEmpty())
            out += (out.isEmpty() ? QString() : QStringLiteral("\n")) + extra;
        cb(ok, out);
        proc->deleteLater();
    };
    connect(timer, &QTimer::timeout, proc, [proc, finish, timeoutSec] {
        proc->kill();
        finish(false, QStringLiteral("timed out after %1 s").arg(timeoutSec));
    });
    connect(proc, &QProcess::finished, proc, [finish](int code, QProcess::ExitStatus status) {
        if (status == QProcess::CrashExit)
            finish(false, QStringLiteral("crashed"));
        else
            finish(code == 0, code == 0 ? QString() : QStringLiteral("exit status %1").arg(code));
    });
    connect(proc, &QProcess::errorOccurred, proc, [finish, proc](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(false, proc->errorString());
    });
    timer->start(timeoutSec * 1000);
    proc->start(argv.first(), argv.mid(1));
}
