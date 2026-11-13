#pragma once

#include "Sources.h"
#include "UnitBackend.h"

#include <QHash>
#include <QSet>

// In-memory stand-ins used by --demo and the unit tests.
class FakeUnitBackend : public UnitBackend
{
    Q_OBJECT
public:
    explicit FakeUnitBackend(QObject *parent = nullptr) : UnitBackend(parent) {}

    void startTransient(const TransientSpec &spec, Done done) override;
    void startUnit(Bus bus, const QString &name, Done done) override;
    void stopUnit(Bus bus, const QString &name, Done done) override;
    void killUnit(Bus bus, const QString &name, int signal) override;
    void resetFailed(Bus, const QString &) override {}
    void unitState(Bus bus, const QString &name, StateCb cb) override;
    void listActiveUnits(Bus bus, const QString &pattern, ListCb cb) override;
    void watch(Bus, const QString &) override {}
    void runCommand(const QStringList &argv, const QStringList &environment, int timeoutSec,
                    CommandCb cb) override;

    // Test controls
    void setState(Bus bus, const QString &name, const QString &active, int execCode = 0, int execStatus = 0);
    QString activeState(Bus bus, const QString &name) const;

    QHash<QString, UnitState> units[2];
    QSet<QString> failToStart;     // startTransient/startUnit fails
    QSet<QString> ignoreStop;      // stop job completes but unit stays active
    QSet<QString> failingCommands; // argv[0] values that fail
    QStringList log;
    int startedCount = 0;
};

class FakeWindowSource : public WindowSource
{
    Q_OBJECT
public:
    explicit FakeWindowSource(QObject *parent = nullptr) : WindowSource(parent) {}
    bool available() const override { return true; }
    QList<WindowInfo> windows() const override { return m_windows; }
    void activate(quint32 id) override { activated << id; }

    quint32 add(const QString &appId, const QString &title = {});
    void remove(quint32 id);

    QList<quint32> activated;

private:
    QList<WindowInfo> m_windows;
    quint32 m_next = 1;
};

class FakeNameSource : public NameSource
{
    Q_OBJECT
public:
    explicit FakeNameSource(QObject *parent = nullptr) : NameSource(parent) {}
    void watchName(const QString &) override {}
    bool hasName(const QString &name) const override { return names.contains(name); }
    void set(const QString &name, bool present);

    QSet<QString> names;
};
