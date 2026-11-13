#include "ControlService.h"

#include "lifecycle/LifecycleController.h"

#include <QDBusConnection>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcControl, "htl.control")

ControlService::ControlService(LifecycleController *life, QObject *parent)
    : QObject(parent)
    , m_life(life)
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
    m_life->launch(id);
}

void ControlService::Home()
{
    m_life->home();
}

void ControlService::OpenBluetooth()
{
    Q_EMIT openBluetoothRequested();
}
