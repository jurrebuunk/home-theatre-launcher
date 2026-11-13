#pragma once

#include <QObject>
#include <QStringList>
#include <functional>

// Abstraction over systemd (user + system manager) and short helper commands,
// so the lifecycle state machine can be tested with a fake.
class UnitBackend : public QObject
{
    Q_OBJECT
public:
    enum Bus { User = 0, System = 1 };
    Q_ENUM(Bus)

    struct UnitState {
        bool loaded = false;
        QString active = QStringLiteral("inactive");
        QString sub;
        QString result;
        int execCode = 0;    // CLD_* code (1 exited, 2 killed, 3 dumped)
        int execStatus = 0;  // exit status or signal number

        bool isRunning() const
        {
            return active == QLatin1String("active") || active == QLatin1String("activating")
                || active == QLatin1String("reloading") || active == QLatin1String("deactivating");
        }
        bool isStopped() const { return !isRunning(); }
        QString describe() const;
    };

    struct TransientSpec {
        QString name;
        QString description;
        QStringList argv;
        QStringList environment;
        QString workingDirectory;
        int stopTimeoutSec = 10;
    };

    using Done = std::function<void(bool ok, const QString &error)>;
    using StateCb = std::function<void(const UnitState &)>;
    using ListCb = std::function<void(const QStringList &)>;
    using CommandCb = std::function<void(bool ok, const QString &output)>;

    explicit UnitBackend(QObject *parent = nullptr) : QObject(parent) {}

    virtual void startTransient(const TransientSpec &spec, Done done) = 0;
    virtual void startUnit(Bus bus, const QString &name, Done done) = 0;
    virtual void stopUnit(Bus bus, const QString &name, Done done) = 0;
    virtual void killUnit(Bus bus, const QString &name, int signal) = 0;
    virtual void resetFailed(Bus bus, const QString &name) = 0;
    virtual void unitState(Bus bus, const QString &name, StateCb cb) = 0;
    // Names of loaded units matching a glob that are not inactive.
    virtual void listActiveUnits(Bus bus, const QString &pattern, ListCb cb) = 0;
    // Emit unitChanged() whenever this unit's properties change.
    virtual void watch(Bus bus, const QString &name) = 0;
    // Run argv directly (no shell), killed after timeoutSec.
    virtual void runCommand(const QStringList &argv, const QStringList &environment, int timeoutSec,
                            CommandCb cb) = 0;

Q_SIGNALS:
    void unitChanged(int bus, const QString &name);
};
