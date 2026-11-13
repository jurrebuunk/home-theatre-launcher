#include "ControlService.h"

#include "idle/IdleMonitor.h"
#include "lifecycle/LifecycleController.h"
#include "network/NetworkSettings.h"

#include <QDBusConnection>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcControl, "htl.control")

ControlService::ControlService(LifecycleController *life, NetworkSettings *network, IdleMonitor *idle, QObject *parent)
    : QObject(parent)
    , m_life(life)
    , m_network(network)
    , m_idle(idle)
{
    connect(m_life, &LifecycleController::changed, this, [this] { Q_EMIT StateChanged(state()); });
}

bool ControlService::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/nl/jurrebuunk/HomeTheatre"), this,
                            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
                                | QDBusConnection::ExportScriptableProperties)) {
        qCWarning(lcControl) << "cannot register control object";
        return false;
    }
    if (!bus.registerService(QStringLiteral("nl.jurrebuunk.HomeTheatre"))) {
        qCWarning(lcControl) << "cannot own nl.jurrebuunk.HomeTheatre (another launcher running?)";
        return false;
    }
    return true;
}

QString ControlService::state() const
{
    return m_life->stateName();
}

QString ControlService::appId() const
{
    return m_life->appId();
}

void ControlService::Launch(const QString &id)
{
    m_idle->activity();
    m_life->launch(id);
}

void ControlService::Home()
{
    m_life->home();
}

void ControlService::OpenBluetooth()
{
    m_idle->activity();
    Q_EMIT openBluetoothRequested();
}

void ControlService::OpenNetwork()
{
    m_idle->activity();
    Q_EMIT openNetworkRequested();
}

bool ControlService::idle() const
{
    return m_idle->idle();
}

void ControlService::EnterIdle()
{
    m_idle->enter();
}

void ControlService::Wake()
{
    m_idle->activity();
}

QStringList ControlService::NetworkInterfaces()
{
    QStringList out;
    for (const QVariant &v : m_network->interfaces())
        out << v.toMap().value("name").toString() + QLatin1Char(' ') + v.toMap().value("kind").toString();
    return out;
}

QStringList ControlService::WifiNetworks()
{
    QStringList out;
    for (const WifiRow &r : m_network->networks()->rows())
        out << r.ssid;
    return out;
}

void ControlService::WifiScan()
{
    if (!m_network->scanning())
        m_network->toggleScan();
}

QString ControlService::WifiConnect(const QString &ssid, const QString &password)
{
    return m_network->connectNetwork(ssid, password);
}

QString ControlService::WifiConnectStatus()
{
    const QVariantMap c = m_network->connecting();
    const QString stage = c.value("stage").toString();
    return stage == QLatin1String("error") ? QStringLiteral("error: ") + c.value("message").toString() : stage;
}

QString ControlService::SetIpv4(const QString &name, const QString &method, const QString &address,
                                const QString &gateway, const QString &dns)
{
    return m_network->setIpv4(name, method, address, gateway, dns);
}
