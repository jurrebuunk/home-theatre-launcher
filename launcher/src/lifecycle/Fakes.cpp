#include "Fakes.h"

#include <QRegularExpression>
#include <QTimer>

void FakeUnitBackend::startTransient(const TransientSpec &spec, Done done)
{
    log << QStringLiteral("startTransient %1 %2").arg(spec.name, spec.argv.join(QLatin1Char(' ')));
    QTimer::singleShot(0, this, [this, spec, done] {
        if (failToStart.contains(spec.name)) {
            setState(User, spec.name, QStringLiteral("failed"), 1, 1);
            done(false, QStringLiteral("failed"));
            return;
        }
        ++startedCount;
        setState(User, spec.name, QStringLiteral("active"));
        done(true, QString());
    });
}

void FakeUnitBackend::startUnit(Bus bus, const QString &name, Done done)
{
    log << QStringLiteral("start %1").arg(name);
    QTimer::singleShot(0, this, [this, bus, name, done] {
        if (failToStart.contains(name)) {
            done(false, QStringLiteral("failed"));
            return;
        }
        setState(bus, name, QStringLiteral("active"));
        done(true, QString());
    });
}

void FakeUnitBackend::stopUnit(Bus bus, const QString &name, Done done)
{
    log << QStringLiteral("stop %1").arg(name);
    QTimer::singleShot(0, this, [this, bus, name, done] {
        if (!ignoreStop.contains(name) && units[bus].contains(name))
            setState(bus, name, QStringLiteral("inactive"));
        done(true, QString());
    });
}

void FakeUnitBackend::killUnit(Bus bus, const QString &name, int signal)
{
    log << QStringLiteral("kill %1 %2").arg(name).arg(signal);
    ignoreStop.remove(name);
    setState(bus, name, QStringLiteral("failed"), 2, signal);
}

void FakeUnitBackend::unitState(Bus bus, const QString &name, StateCb cb)
{
    const UnitState st = units[bus].value(name);
    QTimer::singleShot(0, this, [st, cb] { cb(st); });
}

void FakeUnitBackend::listActiveUnits(Bus bus, const QString &pattern, ListCb cb)
{
    const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern));
    QStringList out;
    for (auto it = units[bus].cbegin(); it != units[bus].cend(); ++it)
        if (it.value().isRunning() && re.match(it.key()).hasMatch())
            out << it.key();
    QTimer::singleShot(0, this, [out, cb] { cb(out); });
}

void FakeUnitBackend::runCommand(const QStringList &argv, const QStringList &, int, CommandCb cb)
{
    log << QStringLiteral("run %1").arg(argv.join(QLatin1Char(' ')));
    const bool ok = argv.isEmpty() || !failingCommands.contains(argv.first());
    QTimer::singleShot(0, this, [ok, cb] { cb(ok, ok ? QString() : QStringLiteral("exit status 1")); });
}

void FakeUnitBackend::setState(Bus bus, const QString &name, const QString &active, int execCode, int execStatus)
{
    UnitState &st = units[bus][name];
    st.loaded = true;
    st.active = active;
    st.sub = active == QLatin1String("active") ? QStringLiteral("running") : QStringLiteral("dead");
    st.result = active == QLatin1String("failed") ? QStringLiteral("exit-code") : QStringLiteral("success");
    st.execCode = execCode;
    st.execStatus = execStatus;
    Q_EMIT unitChanged(bus, name);
}

QString FakeUnitBackend::activeState(Bus bus, const QString &name) const
{
    return units[bus].value(name).active;
}

quint32 FakeWindowSource::add(const QString &appId, const QString &title)
{
    const quint32 id = m_next++;
    m_windows << WindowInfo{id, appId, title};
    Q_EMIT windowsChanged();
    return id;
}

void FakeWindowSource::remove(quint32 id)
{
    m_windows.removeIf([id](const WindowInfo &w) { return w.id == id; });
    Q_EMIT windowsChanged();
}

void FakeNameSource::set(const QString &name, bool present)
{
    if (present)
        names.insert(name);
    else
        names.remove(name);
    Q_EMIT namesChanged();
}
