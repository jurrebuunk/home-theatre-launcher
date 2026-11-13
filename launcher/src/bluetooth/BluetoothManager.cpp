#include "BluetoothManager.h"

#include "BluezAgent.h"
#include "Config.h"
#include "dbus/DBusTypes.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBt, "htl.bluetooth")

namespace {
const QString kBluez = QStringLiteral("org.bluez");
const QString kAdapter = QStringLiteral("org.bluez.Adapter1");
const QString kDevice = QStringLiteral("org.bluez.Device1");
const QString kBattery = QStringLiteral("org.bluez.Battery1");
const QString kAgentPath = QStringLiteral("/nl/jurrebuunk/HomeTheatre/agent");

QDBusConnection bus() { return QDBusConnection::systemBus(); }
} // namespace

BluetoothManager::BluetoothManager(const Config *config, bool demo, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_demo(demo)
{
    DBusTypes::registerAll();

    const QVariantMap bt = config->bluetooth();
    m_scanDuration = bt.value("discoveryTimeout").toInt();
    m_pairingTimeout = bt.value("pairingTimeout").toInt();
    m_capability = bt.value("agentCapability").toString();
    m_preferredAdapter = bt.value("adapter").toString();

    auto lookup = [this](const QString &p) { return m_devices.value(p); };
    m_paired = new DeviceModel(lookup, this);
    m_nearby = new DeviceModel(lookup, this);

    m_scanTimer.setInterval(1000);
    connect(&m_scanTimer, &QTimer::timeout, this, [this] {
        if (--m_scanRemaining <= 0)
            stopScan();
        Q_EMIT scanRemainingChanged();
    });

    if (demo) {
        populateDemo();
        return;
    }
    if (!bt.value("enable").toBool())
        return;

    m_agentObject = new QObject(this);
    new BluezAgentAdaptor(
        m_agentObject,
        [this](const QString &type, const QString &device, const QVariant &value, const QDBusMessage &msg) {
            handleAgent(type, device, value, msg);
        },
        [this] { cancelAgent(); });
    if (!bus().registerObject(kAgentPath, m_agentObject))
        qCWarning(lcBt) << "cannot register agent object";

    m_watcher = new QDBusServiceWatcher(kBluez, bus(), QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) {
                if (owner.isEmpty()) {
                    m_serviceAvailable = false;
                    m_adapters.clear();
                    m_devices.clear();
                    m_adapterProps.clear();
                    rebuild();
                } else {
                    loadObjects();
                }
            });

    bus().connect(kBluez, QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                  QStringLiteral("InterfacesAdded"), this,
                  SLOT(onInterfacesAdded(QDBusObjectPath, QMap<QString, QVariantMap>)));
    bus().connect(kBluez, QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                  QStringLiteral("InterfacesRemoved"), this, SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
    bus().connect(kBluez, QString(), QStringLiteral("org.freedesktop.DBus.Properties"),
                  QStringLiteral("PropertiesChanged"), this,
                  SLOT(onPropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));
    loadObjects();
}

// ---------------------------------------------------------------- state

QString BluetoothManager::currentAdapter() const
{
    return m_adapters.value(m_adapterIndex);
}

QStringList BluetoothManager::adapterNames() const
{
    QStringList out;
    for (const QString &p : m_adapters)
        out << p.section(QLatin1Char('/'), -1);
    return out;
}

QString BluetoothManager::adapterName() const
{
    return currentAdapter().section(QLatin1Char('/'), -1);
}

bool BluetoothManager::powered() const
{
    return m_adapterProps.value(currentAdapter()).value(QStringLiteral("Powered")).toBool();
}

bool BluetoothManager::discovering() const
{
    return m_adapterProps.value(currentAdapter()).value(QStringLiteral("Discovering")).toBool();
}

int BluetoothManager::connectedCount() const
{
    int n = 0;
    for (const QVariantMap &d : m_devices)
        if (d.value(QStringLiteral("Connected")).toBool())
            ++n;
    return n;
}

QString BluetoothManager::deviceName(const QString &path) const
{
    const QVariantMap d = m_devices.value(path);
    return d.value(QStringLiteral("Alias"), d.value(QStringLiteral("Name"))).toString();
}

void BluetoothManager::loadObjects()
{
    if (m_demo)
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(kBluez, QStringLiteral("/"),
                                                      QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                                      QStringLiteral("GetManagedObjects"));
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg, 5000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<ObjectMap> reply = *w;
        m_adapters.clear();
        m_adapterProps.clear();
        m_devices.clear();
        m_serviceAvailable = !reply.isError();
        if (reply.isError()) {
            qCWarning(lcBt) << "BlueZ unavailable:" << reply.error().message();
        } else {
            const ObjectMap objects = reply.value();
            for (auto it = objects.cbegin(); it != objects.cend(); ++it)
                onInterfacesAdded(it.key(), it.value());
            registerAgent();
        }
        rebuild();
    });
}

void BluetoothManager::registerAgent()
{
    call(QStringLiteral("/org/bluez"), QStringLiteral("org.bluez.AgentManager1"), QStringLiteral("RegisterAgent"),
         {QVariant::fromValue(QDBusObjectPath(kAgentPath)), m_capability}, 5000, [this](const QDBusMessage &) {
             call(QStringLiteral("/org/bluez"), QStringLiteral("org.bluez.AgentManager1"),
                  QStringLiteral("RequestDefaultAgent"), {QVariant::fromValue(QDBusObjectPath(kAgentPath))}, 5000,
                  [](const QDBusMessage &) { qCInfo(lcBt) << "pairing agent registered"; });
         });
}

void BluetoothManager::onInterfacesAdded(const QDBusObjectPath &path, const InterfaceMap &interfaces)
{
    const QString p = path.path();
    if (interfaces.contains(kAdapter)) {
        m_adapterProps[p].insert(interfaces.value(kAdapter));
        if (!m_adapters.contains(p)) {
            m_adapters << p;
            m_adapters.sort();
            const int preferred = int(m_adapters.indexOf(QStringLiteral("/org/bluez/") + m_preferredAdapter));
            if (preferred >= 0)
                m_adapterIndex = preferred;
        }
    }
    if (interfaces.contains(kDevice))
        m_devices[p].insert(interfaces.value(kDevice));
    if (interfaces.contains(kBattery)) {
        m_devices[p].insert(QStringLiteral("Battery"), interfaces.value(kBattery).value(QStringLiteral("Percentage")).toInt());
        Q_EMIT batteryChanged(p, m_devices[p].value(QStringLiteral("Battery")).toInt());
    }
    rebuild();
}

void BluetoothManager::onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces)
{
    const QString p = path.path();
    if (interfaces.contains(kAdapter)) {
        m_adapters.removeAll(p);
        m_adapterProps.remove(p);
        m_adapterIndex = qBound(0, m_adapterIndex, qMax(0, int(m_adapters.size()) - 1));
    }
    if (interfaces.contains(kDevice))
        m_devices.remove(p);
    else if (interfaces.contains(kBattery) && m_devices.contains(p))
        m_devices[p].remove(QStringLiteral("Battery"));
    rebuild();
}

void BluetoothManager::onPropertiesChanged(const QString &iface, const QVariantMap &changed,
                                           const QStringList &invalidated, const QDBusMessage &msg)
{
    const QString p = msg.path();
    if (iface == kBattery && m_devices.contains(p) && changed.contains(QStringLiteral("Percentage"))) {
        const int level = changed.value(QStringLiteral("Percentage")).toInt();
        if (m_devices[p].value(QStringLiteral("Battery")) != level) {
            m_devices[p].insert(QStringLiteral("Battery"), level);
            Q_EMIT batteryChanged(p, level);
            rebuild();
        }
        return;
    }
    if (iface == kDevice && m_devices.contains(p) && changed.contains(QStringLiteral("Connected"))
        && changed.value(QStringLiteral("Connected")).toBool() != m_devices[p].value(QStringLiteral("Connected")).toBool()) {
        const bool connected = changed.value(QStringLiteral("Connected")).toBool();
        m_devices[p].insert(QStringLiteral("Connected"), connected);
        Q_EMIT connectionChanged(p, connected);
    }
    QVariantMap *target = nullptr;
    if (iface == kAdapter && m_adapterProps.contains(p))
        target = &m_adapterProps[p];
    else if (iface == kDevice && m_devices.contains(p))
        target = &m_devices[p];
    if (!target)
        return;
    // RSSI/ManufacturerData churn during scans is irrelevant for the UI.
    bool relevant = false;
    for (auto it = changed.cbegin(); it != changed.cend(); ++it) {
        if (target->value(it.key()) != it.value()) {
            target->insert(it.key(), it.value());
            if (it.key() != QLatin1String("RSSI") && it.key() != QLatin1String("ManufacturerData")
                && it.key() != QLatin1String("ServiceData") && it.key() != QLatin1String("TxPower"))
                relevant = true;
        }
    }
    for (const QString &k : invalidated)
        target->remove(k);
    if (iface == kAdapter && changed.contains(QStringLiteral("Discovering"))
        && !changed.value(QStringLiteral("Discovering")).toBool() && m_scanTimer.isActive()) {
        m_scanTimer.stop();
        m_scanRemaining = 0;
        Q_EMIT scanRemainingChanged();
    }
    if (relevant || !invalidated.isEmpty())
        rebuild();
}

QVariantMap BluetoothManager::deviceInfo(const QString &path) const
{
    const QVariantMap d = m_devices.value(path);
    const QString name = deviceName(path);
    return {{"name", name}, {"kind", DeviceModel::kindForIcon(d.value(QStringLiteral("Icon")).toString(), name)},
            {"battery", d.value(QStringLiteral("Battery"), -1)}, {"connected", d.value(QStringLiteral("Connected")).toBool()}};
}

int BluetoothManager::batteryForAddress(const QString &address) const
{
    for (const QVariantMap &d : m_devices)
        if (d.value(QStringLiteral("Address")).toString().compare(address, Qt::CaseInsensitive) == 0)
            return d.value(QStringLiteral("Battery"), -1).toInt();
    return -1;
}

void BluetoothManager::rebuild()
{
    const QString adapter = currentAdapter();
    QStringList paired, nearby;
    QStringList keys = m_devices.keys();
    std::sort(keys.begin(), keys.end(), [this](const QString &a, const QString &b) {
        return deviceName(a).localeAwareCompare(deviceName(b)) < 0;
    });
    for (const QString &p : keys) {
        const QVariantMap d = m_devices.value(p);
        if (d.value(QStringLiteral("Adapter")).value<QDBusObjectPath>().path() != adapter
            && d.value(QStringLiteral("Adapter")).toString() != adapter)
            continue;
        if (d.value(QStringLiteral("Paired")).toBool() || d.value(QStringLiteral("Bonded")).toBool())
            paired << p;
        else if (d.contains(QStringLiteral("Name")))
            nearby << p;
    }
    m_paired->sync(paired);
    m_nearby->sync(nearby);
    Q_EMIT changed();
}

// ---------------------------------------------------------------- D-Bus helpers

void BluetoothManager::call(const QString &path, const QString &iface, const QString &method,
                            const QVariantList &args, int timeoutMs, std::function<void(const QDBusMessage &)> ok)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kBluez, path, iface, method);
    msg.setArguments(args);
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg, timeoutMs), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, path, ok](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusMessage reply = w->reply();
        setBusy(path, false);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            reportError(path, reply);
            return;
        }
        ok(reply);
    });
}

void BluetoothManager::setProperty(const QString &path, const QString &iface, const QString &name,
                                   const QVariant &value)
{
    if (m_demo) {
        if (iface == kAdapter)
            m_adapterProps[path].insert(name, value);
        else
            m_devices[path].insert(name, value);
        rebuild();
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(kBluez, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Set"));
    msg << iface << name << QVariant::fromValue(QDBusVariant(value));
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg, 10000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError())
            reportError(path, w->reply());
    });
}

void BluetoothManager::setBusy(const QString &path, bool busy)
{
    if (!m_devices.contains(path))
        return;
    m_devices[path].insert(QStringLiteral("_busy"), busy);
    m_paired->refresh(path);
    m_nearby->refresh(path);
}

QString BluetoothManager::friendlyError(const QString &name, const QString &message)
{
    static const QHash<QString, QString> map{
        {"org.bluez.Error.AuthenticationFailed", "Authentication failed"},
        {"org.bluez.Error.AuthenticationCanceled", "Pairing was cancelled"},
        {"org.bluez.Error.AuthenticationRejected", "Pairing was rejected"},
        {"org.bluez.Error.AuthenticationTimeout", "Pairing timed out"},
        {"org.bluez.Error.ConnectionAttemptFailed", "Couldn’t reach the device"},
        {"org.bluez.Error.InProgress", "Already in progress"},
        {"org.bluez.Error.NotReady", "Bluetooth is off"},
        {"org.bluez.Error.NotAvailable", "Not available"},
        {"org.bluez.Error.DoesNotExist", "The device is gone"},
        {"org.freedesktop.DBus.Error.NoReply", "The device didn’t respond"},
        {"org.freedesktop.DBus.Error.ServiceUnknown", "Bluetooth service unavailable"},
    };
    if (map.contains(name))
        return map.value(name);
    return message.isEmpty() ? name : message;
}

void BluetoothManager::reportError(const QString &path, const QDBusMessage &reply)
{
    m_lastError = friendlyError(reply.errorName(), reply.errorMessage());
    m_errorPath = path;
    qCWarning(lcBt) << path << reply.errorName() << reply.errorMessage();
    Q_EMIT lastErrorChanged();
}

void BluetoothManager::clearError()
{
    if (m_lastError.isEmpty())
        return;
    m_lastError.clear();
    m_errorPath.clear();
    Q_EMIT lastErrorChanged();
}

// ---------------------------------------------------------------- actions

void BluetoothManager::retry()
{
    if (m_demo)
        return;
    loadObjects();
}

void BluetoothManager::selectAdapter(int delta)
{
    if (m_adapters.size() < 2)
        return;
    stopScan();
    m_adapterIndex = (m_adapterIndex + delta + int(m_adapters.size())) % int(m_adapters.size());
    rebuild();
}

void BluetoothManager::setPowered(bool on)
{
    if (currentAdapter().isEmpty())
        return;
    if (!on)
        stopScan();
    setProperty(currentAdapter(), kAdapter, QStringLiteral("Powered"), on);
}

void BluetoothManager::toggleScan()
{
    if (m_scanTimer.isActive() || discovering()) {
        stopScan();
        return;
    }
    if (currentAdapter().isEmpty() || !powered())
        return;
    clearError();
    m_scanRemaining = m_scanDuration;
    m_scanTimer.start();
    Q_EMIT scanRemainingChanged();
    if (m_demo) {
        m_adapterProps[currentAdapter()].insert(QStringLiteral("Discovering"), true);
        rebuild();
        return;
    }
    call(currentAdapter(), kAdapter, QStringLiteral("StartDiscovery"), {}, 10000, [](const QDBusMessage &) {});
}

void BluetoothManager::stopScan()
{
    const bool wasScanning = m_scanTimer.isActive() || discovering();
    m_scanTimer.stop();
    m_scanRemaining = 0;
    Q_EMIT scanRemainingChanged();
    if (!wasScanning || currentAdapter().isEmpty())
        return;
    if (m_demo) {
        m_adapterProps[currentAdapter()].insert(QStringLiteral("Discovering"), false);
        rebuild();
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(kBluez, currentAdapter(), kAdapter, QStringLiteral("StopDiscovery"));
    bus().asyncCall(msg, 5000); // NotReady/"No discovery started" errors are harmless here
}

void BluetoothManager::connectDevice(const QString &path)
{
    clearError();
    if (m_demo) {
        setProperty(path, kDevice, QStringLiteral("Connected"), true);
        return;
    }
    setBusy(path, true);
    m_connectingPath = path;
    call(path, kDevice, QStringLiteral("Connect"), {}, 30000, [this](const QDBusMessage &) { m_connectingPath.clear(); });
}

void BluetoothManager::disconnectDevice(const QString &path)
{
    clearError();
    if (m_demo) {
        setProperty(path, kDevice, QStringLiteral("Connected"), false);
        return;
    }
    setBusy(path, true);
    call(path, kDevice, QStringLiteral("Disconnect"), {}, 15000, [](const QDBusMessage &) {});
}

void BluetoothManager::setTrusted(const QString &path, bool trusted)
{
    clearError();
    setProperty(path, kDevice, QStringLiteral("Trusted"), trusted);
}

void BluetoothManager::forget(const QString &path)
{
    clearError();
    const QString adapter = m_devices.value(path).value(QStringLiteral("Adapter")).value<QDBusObjectPath>().path();
    if (m_demo) {
        m_devices.remove(path);
        rebuild();
        return;
    }
    call(adapter.isEmpty() ? currentAdapter() : adapter, kAdapter, QStringLiteral("RemoveDevice"),
         {QVariant::fromValue(QDBusObjectPath(path))}, 10000, [](const QDBusMessage &) {});
}

void BluetoothManager::pair(const QString &path)
{
    if (!m_pairing.isEmpty())
        return;
    clearError();
    stopScan(); // discovery makes pairing slower and less reliable
    m_trustOnPair = false;
    m_pairing = {{"path", path}, {"name", deviceName(path)},
                 {"kind", DeviceModel::kindForIcon(m_devices.value(path).value("Icon").toString(), deviceName(path))},
                 {"stage", "working"}, {"message", QString()}};
    Q_EMIT pairingChanged();
    if (m_demo)
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(kBluez, path, kDevice, QStringLiteral("Pair"));
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg, (m_pairingTimeout + 10) * 1000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusMessage reply = w->reply();
        if (m_pairing.value("path").toString() != path)
            return;
        if (reply.type() == QDBusMessage::ErrorMessage && reply.errorName() != QLatin1String("org.bluez.Error.AlreadyExists")) {
            cancelAgent();
            m_pairing.insert("stage", "error");
            m_pairing.insert("message", friendlyError(reply.errorName(), reply.errorMessage()));
            Q_EMIT pairingChanged();
            return;
        }
        finishPairing(true);
    });
}

void BluetoothManager::finishPairing(bool ok)
{
    const QString path = m_pairing.value("path").toString();
    m_pairing.clear();
    Q_EMIT pairingChanged();
    if (!ok || path.isEmpty())
        return;
    // Trust only ever follows the explicit tick in the pairing dialog.
    if (m_trustOnPair)
        setTrusted(path, true);
    connectDevice(path);
}

void BluetoothManager::cancelPairing()
{
    const QString path = m_pairing.value("path").toString();
    if (!m_request.isEmpty())
        rejectRequest();
    m_pairing.clear();
    Q_EMIT pairingChanged();
    if (m_demo || path.isEmpty())
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(kBluez, path, kDevice, QStringLiteral("CancelPairing"));
    bus().asyncCall(msg, 5000);
}

// ---------------------------------------------------------------- agent

void BluetoothManager::handleAgent(const QString &type, const QString &device, const QVariant &value,
                                   const QDBusMessage &msg)
{
    // A service authorisation for the device the user is pairing/connecting right
    // now is part of that explicit action; anything else is asked on screen.
    if (type == QLatin1String("authorizeService")
        && (device == m_pairing.value("path").toString() || device == m_connectingPath)) {
        bus().send(msg.createReply());
        return;
    }
    if (!m_request.isEmpty())
        rejectRequest(); // only one prompt at a time

    QString text;
    if (type == QLatin1String("confirm") || type == QLatin1String("displayPasskey"))
        text = QStringLiteral("%1").arg(value.toUInt(), 6, 10, QLatin1Char('0'));
    else if (type == QLatin1String("displayPin"))
        text = value.toString();

    m_request = {{"type", type}, {"path", device}, {"name", deviceName(device)},
                 {"kind", DeviceModel::kindForIcon(m_devices.value(device).value("Icon").toString(), deviceName(device))},
                 {"code", text}, {"needsReply", msg.isDelayedReply()}};
    m_pendingAgentMsg = msg;
    if (m_pairing.isEmpty() && type != QLatin1String("authorizeService")) {
        // Device-initiated pairing: show the same dialog.
        m_pairing = {{"path", device}, {"name", deviceName(device)}, {"kind", m_request.value("kind")},
                     {"stage", "working"}, {"message", QString()}};
        Q_EMIT pairingChanged();
    }
    Q_EMIT requestChanged();
}

void BluetoothManager::acceptRequest(const QString &value)
{
    if (m_request.isEmpty())
        return;
    const QString type = m_request.value("type").toString();
    if (m_pendingAgentMsg.isDelayedReply()) {
        QDBusMessage reply;
        if (type == QLatin1String("requestPin"))
            reply = m_pendingAgentMsg.createReply(value);
        else if (type == QLatin1String("requestPasskey"))
            reply = m_pendingAgentMsg.createReply(QVariant::fromValue(value.toUInt()));
        else
            reply = m_pendingAgentMsg.createReply();
        bus().send(reply);
    }
    m_request.clear();
    m_pendingAgentMsg = QDBusMessage();
    Q_EMIT requestChanged();
    if (m_demo && !m_pairing.isEmpty()) {
        m_devices[m_pairing.value("path").toString()].insert(QStringLiteral("Paired"), true);
        finishPairing(true);
    }
}

void BluetoothManager::rejectRequest()
{
    if (m_request.isEmpty())
        return;
    if (m_pendingAgentMsg.isDelayedReply())
        bus().send(m_pendingAgentMsg.createErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                                                      QStringLiteral("Rejected by user")));
    const bool wasAuthorization = m_request.value("type").toString().startsWith(QLatin1String("authorize"));
    m_request.clear();
    m_pendingAgentMsg = QDBusMessage();
    Q_EMIT requestChanged();
    if (wasAuthorization && m_pairing.value("stage") == QLatin1String("working")
        && m_pairing.value("path") != m_connectingPath) {
        m_pairing.clear();
        Q_EMIT pairingChanged();
    }
}

void BluetoothManager::cancelAgent()
{
    // BlueZ cancelled (timeout or remote side): the pending message must not be answered.
    if (m_request.isEmpty())
        return;
    m_request.clear();
    m_pendingAgentMsg = QDBusMessage();
    Q_EMIT requestChanged();
}

void BluetoothManager::close()
{
    stopScan();
    if (!m_pairing.isEmpty())
        cancelPairing();
    else if (!m_request.isEmpty())
        rejectRequest();
    clearError();
}

// ---------------------------------------------------------------- demo

void BluetoothManager::populateDemo()
{
    m_serviceAvailable = true;
    const QString hci = QStringLiteral("/org/bluez/hci0");
    m_adapters = {hci};
    m_adapterProps[hci] = {{"Powered", true}, {"Discovering", true}};
    auto add = [&](const QString &id, const QString &name, const QString &icon, bool paired, bool connected,
                   bool trusted, int battery = -1) {
        QVariantMap d{{"Name", name},           {"Alias", name},     {"Icon", icon},       {"Paired", paired},
                      {"Connected", connected}, {"Trusted", trusted}, {"Adapter", hci},
                      {"Address", QStringLiteral("00:11:22:33:44:%1").arg(id)}};
        if (battery >= 0)
            d.insert("Battery", battery);
        m_devices[hci + QStringLiteral("/dev_") + id] = d;
    };
    add("01", "Xbox Wireless Controller", "input-gaming", true, true, true, 80);
    add("02", "DualSense Wireless Controller", "input-gaming", true, true, true, 15);
    add("03", "Media Remote", "input-keyboard", true, true, true);
    add("04", "Living Room Soundbar", "audio-card", true, true, true);
    add("05", "Over-ear Headphones", "audio-headphones", false, false, false, 64);
    add("06", "Compact Keyboard", "input-keyboard", false, false, false);
    // Keep the design's order rather than alphabetical for the demo.
    m_paired->sync({hci + "/dev_01", hci + "/dev_02", hci + "/dev_03", hci + "/dev_04"});
    m_nearby->sync({hci + "/dev_05", hci + "/dev_06"});
    m_scanRemaining = 23;
}

void BluetoothManager::demoShowPairing()
{
    const QString path = QStringLiteral("/org/bluez/hci0/dev_05");
    m_pairing = {{"path", path}, {"name", deviceName(path)}, {"kind", "headphones"}, {"stage", "working"},
                 {"message", QString()}};
    m_request = {{"type", "confirm"}, {"path", path}, {"name", deviceName(path)}, {"kind", "headphones"},
                 {"code", "482913"}, {"needsReply", false}};
    Q_EMIT pairingChanged();
    Q_EMIT requestChanged();
}
