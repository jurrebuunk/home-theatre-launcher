#include "BusNameWatcher.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

BusNameWatcher::BusNameWatcher(QObject *parent)
    : NameSource(parent)
{
    m_watcher.setConnection(QDBusConnection::sessionBus());
    m_watcher.setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &name, const QString &, const QString &newOwner) {
                if (newOwner.isEmpty())
                    m_present.remove(name);
                else
                    m_present.insert(name);
                Q_EMIT namesChanged();
            });
}

void BusNameWatcher::watchName(const QString &name)
{
    if (name.isEmpty() || m_watcher.watchedServices().contains(name))
        return;
    m_watcher.addWatchedService(name);
    if (auto *iface = QDBusConnection::sessionBus().interface(); iface && iface->isServiceRegistered(name))
        m_present.insert(name);
}
