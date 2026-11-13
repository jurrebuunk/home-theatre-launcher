#pragma once

#include "UnitBackend.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QSet>

class SystemdBackend : public UnitBackend
{
    Q_OBJECT
public:
    explicit SystemdBackend(QObject *parent = nullptr);

    void startTransient(const TransientSpec &spec, Done done) override;
    void startUnit(Bus bus, const QString &name, Done done) override;
    void stopUnit(Bus bus, const QString &name, Done done) override;
    void killUnit(Bus bus, const QString &name, int signal) override;
    void resetFailed(Bus bus, const QString &name) override;
    void unitState(Bus bus, const QString &name, StateCb cb) override;
    void listActiveUnits(Bus bus, const QString &pattern, ListCb cb) override;
    void watch(Bus bus, const QString &name) override;
    void runCommand(const QStringList &argv, const QStringList &environment, int timeoutSec,
                    CommandCb cb) override;

    static QString unitPath(const QString &name);

private Q_SLOTS:
    void onUserJobRemoved(uint id, const QDBusObjectPath &job, const QString &unit, const QString &result);
    void onSystemJobRemoved(uint id, const QDBusObjectPath &job, const QString &unit, const QString &result);
    void onUserPropertiesChanged(const QString &iface, const QVariantMap &changed,
                                 const QStringList &invalidated, const QDBusMessage &msg);
    void onSystemPropertiesChanged(const QString &iface, const QVariantMap &changed,
                                   const QStringList &invalidated, const QDBusMessage &msg);

private:
    QDBusConnection connection(Bus bus) const;
    QDBusMessage managerCall(const QString &method) const;
    void callJob(Bus bus, const QDBusMessage &msg, Done done, bool missingIsOk);
    void jobRemoved(Bus bus, const QString &job, const QString &result);

    QHash<QString, Done> m_jobs[2];
    QHash<QString, QString> m_pendingResults[2]; // JobRemoved seen before we stored the job
    QHash<QString, QString> m_watched[2]; // object path -> unit name
};
