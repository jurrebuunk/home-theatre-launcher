#include "NetworkSettings.h"

#include "Config.h"
#include "dbus/DBusTypes.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QtEndian>

Q_LOGGING_CATEGORY(lcNetCfg, "htl.network.settings")

namespace {
const QString kNm = QStringLiteral("org.freedesktop.NetworkManager");
const QString kNmPath = QStringLiteral("/org/freedesktop/NetworkManager");
const QString kDevice = QStringLiteral("org.freedesktop.NetworkManager.Device");
const QString kWired = QStringLiteral("org.freedesktop.NetworkManager.Device.Wired");
const QString kWireless = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
const QString kAp = QStringLiteral("org.freedesktop.NetworkManager.AccessPoint");
const QString kIp4 = QStringLiteral("org.freedesktop.NetworkManager.IP4Config");
const QString kActive = QStringLiteral("org.freedesktop.NetworkManager.Connection.Active");
const QString kSettings = QStringLiteral("org.freedesktop.NetworkManager.Settings");
const QString kConnection = QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection");
const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");

QDBusConnection bus() { return QDBusConnection::systemBus(); }

QString readTrimmed(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll().trimmed()) : QString();
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QString deviceState(uint s)
{
    // NMDeviceState
    if (s >= 100 && s < 110) return QStringLiteral("connected");
    if (s >= 40 && s < 100) return QStringLiteral("connecting");
    if (s == 30 || s == 110 || s == 120) return QStringLiteral("disconnected");
    if (s == 10) return QStringLiteral("unmanaged");
    return QStringLiteral("unavailable");
}
} // namespace

NetworkSettings::NetworkSettings(const Config *config, bool demo, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_demo(demo)
    , m_networks(new WifiModel(this))
{
    DBusTypes::registerAll();
    m_scanDuration = config->network().value("scanDuration").toInt();

    m_scanTimer.setInterval(1000);
    connect(&m_scanTimer, &QTimer::timeout, this, [this] {
        if (--m_scanRemaining <= 0) {
            m_scanRemaining = 0;
            m_scanTimer.stop();
        } else if (m_scanRemaining == m_scanDuration / 2) {
            requestScan(); // a second pass catches slow access points
        }
        Q_EMIT scanChanged();
    });
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(250);
    connect(&m_reloadTimer, &QTimer::timeout, this, &NetworkSettings::reload);
    m_fallbackTimer.setInterval(5000);
    connect(&m_fallbackTimer, &QTimer::timeout, this, &NetworkSettings::reloadFallback);
    m_connectTimer.setSingleShot(true);
    m_connectTimer.setInterval(45000);
    connect(&m_connectTimer, &QTimer::timeout, this, [this] { finishConnect(false, QStringLiteral("Timed out")); });

    if (demo) {
        populateDemo();
        return;
    }

    m_watcher = new QDBusServiceWatcher(kNm, bus(), QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) {
                m_managed = !owner.isEmpty();
                m_saved.clear();
                scheduleReload();
            });
    for (const char *iface : {"org.freedesktop.DBus.Properties", "org.freedesktop.DBus.ObjectManager",
                              "org.freedesktop.NetworkManager.Settings.Connection"}) {
        bus().connect(kNm, QString(), QString::fromLatin1(iface), QString(), this, SLOT(onSignal(QDBusMessage)));
    }
    bus().connect(kNm, QString(), kActive, QStringLiteral("StateChanged"), this,
                  SLOT(onActiveStateChanged(uint, uint, QDBusMessage)));
    bus().connect(kNm, QString(), kDevice, QStringLiteral("StateChanged"), this,
                  SLOT(onDeviceStateChanged(uint, uint, uint, QDBusMessage)));

    const auto *daemon = bus().interface();
    m_managed = daemon && daemon->isServiceRegistered(kNm);
    reload();
}

// ---------------------------------------------------------------- state

QVariantList NetworkSettings::interfaces() const
{
    QVariantList out;
    int ethernet = 0, wifi = 0;
    for (const Iface &i : m_ifaces) {
        const bool w = i.kind == QLatin1String("wifi");
        const int n = w ? ++wifi : ++ethernet;
        QString label = w ? QStringLiteral("Wi-Fi") : QStringLiteral("Ethernet");
        if (n > 1)
            label += QLatin1Char(' ') + QString::number(n);
        out << QVariantMap{{"name", i.name}, {"kind", i.kind}, {"label", label}};
    }
    return out;
}

const NetworkSettings::Iface *NetworkSettings::iface(const QString &name) const
{
    for (const Iface &i : m_ifaces)
        if (i.name == name)
            return &i;
    return nullptr;
}

const NetworkSettings::Iface *NetworkSettings::wifiIface() const
{
    if (const Iface *s = iface(m_selected); s && s->kind == QLatin1String("wifi"))
        return s;
    for (const Iface &i : m_ifaces)
        if (i.kind == QLatin1String("wifi"))
            return &i;
    return nullptr;
}

QVariantMap NetworkSettings::details() const
{
    const Iface *i = iface(m_selected);
    if (!i)
        return {};
    QString ssid;
    if (i->kind == QLatin1String("wifi"))
        for (const WifiRow &r : m_rows)
            if (r.active)
                ssid = r.ssid;
    return {
        {"name", i->name}, {"kind", i->kind}, {"carrier", i->carrier}, {"speed", speedText(i->speed)},
        {"state", i->state}, {"address", i->address}, {"gateway", i->gateway}, {"dns", i->dns.join(QStringLiteral(", "))},
        {"method", i->method}, {"editable", i->editable && m_managed}, {"ssid", ssid},
    };
}

bool NetworkSettings::needsPassword(const QString &ssid) const
{
    const WifiRow *r = m_networks->find(ssid);
    return r && r->secured() && !r->saved;
}

QStringList NetworkSettings::savedFor(const QString &ssid) const
{
    QStringList out;
    for (auto it = m_saved.cbegin(); it != m_saved.cend(); ++it)
        if (it->type == QLatin1String("802-11-wireless") && it->ssid == ssid)
            out << it.key();
    out.sort();
    return out;
}

void NetworkSettings::setLastError(const QString &message)
{
    if (message == m_lastError)
        return;
    m_lastError = message;
    Q_EMIT lastErrorChanged();
}

void NetworkSettings::clearError() { setLastError({}); }

void NetworkSettings::setConnecting(const QVariantMap &c)
{
    m_connecting = c;
    Q_EMIT connectingChanged();
}

void NetworkSettings::clearConnecting()
{
    if (m_connecting.value("stage") == QLatin1String("working"))
        return; // still running; the result will replace it
    setConnecting({});
}

// ---------------------------------------------------------------- view

void NetworkSettings::open()
{
    m_open = true;
    m_resortNext = true;
    if (m_demo)
        return;
    if (m_managed)
        reload();
    else {
        reloadFallback();
        m_fallbackTimer.start();
    }
    if (const Iface *w = wifiIface(); w && iface(m_selected) == w && m_wifiEnabled && !scanning())
        toggleScan();
}

void NetworkSettings::close()
{
    m_open = false;
    m_fallbackTimer.stop();
    m_scanTimer.stop();
    if (m_scanRemaining) {
        m_scanRemaining = 0;
        Q_EMIT scanChanged();
    }
    clearConnecting();
    clearError();
}

void NetworkSettings::select(const QString &name)
{
    if (name == m_selected || !iface(name))
        return;
    m_selected = name;
    m_resortNext = true;
    Q_EMIT changed();
    if (m_open && iface(name)->kind == QLatin1String("wifi") && m_wifiEnabled && !scanning())
        toggleScan();
}

void NetworkSettings::toggleScan()
{
    if (scanning()) {
        m_scanRemaining = 0;
        m_scanTimer.stop();
        Q_EMIT scanChanged();
        return;
    }
    if (!wifiIface() || !m_wifiEnabled)
        return;
    m_resortNext = true;
    m_scanRemaining = m_scanDuration;
    m_scanTimer.start();
    Q_EMIT scanChanged();
    requestScan();
}

void NetworkSettings::requestScan()
{
    const Iface *w = wifiIface();
    if (m_demo || !m_managed || !w || w->path.isEmpty())
        return;
    // "Scanning not allowed immediately following previous scan" is harmless.
    call(w->path, kWireless, QStringLiteral("RequestScan"), {QVariantMap()}, [](const QDBusMessage &) {},
         [](const QDBusMessage &) {});
}

void NetworkSettings::setWifiEnabled(bool on)
{
    if (m_demo) {
        m_wifiEnabled = on;
        if (!on)
            m_rows.clear();
        else
            populateDemo();
        m_networks->sync(m_rows, true);
        Q_EMIT changed();
        return;
    }
    if (!m_managed)
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(kNm, kNmPath, kProps, QStringLiteral("Set"));
    msg << kNm << QStringLiteral("WirelessEnabled") << QVariant::fromValue(QDBusVariant(on));
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg, 10000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, on](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError())
            setLastError(QStringLiteral("Cannot turn Wi-Fi %1: %2").arg(on ? "on" : "off", w->error().message()));
    });
}

// ---------------------------------------------------------------- actions

void NetworkSettings::call(const QString &path, const QString &iface, const QString &method, const QVariantList &args,
                           std::function<void(const QDBusMessage &)> ok, std::function<void(const QDBusMessage &)> fail)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kNm, path, iface, method);
    msg.setArguments(args);
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg, 30000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, ok, fail, method](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusMessage reply = w->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qCWarning(lcNetCfg) << method << "failed:" << reply.errorName() << reply.errorMessage();
            if (fail)
                fail(reply);
            else
                setLastError(reply.errorMessage());
            return;
        }
        ok(reply);
    });
}

QString NetworkSettings::connectNetwork(const QString &ssid, const QString &password, bool hidden)
{
    if (ssid.isEmpty())
        return QStringLiteral("Enter a network name");
    if (m_connecting.value("stage") == QLatin1String("working"))
        return QStringLiteral("Already connecting");
    const WifiRow *row = hidden ? nullptr : m_networks->find(ssid);
    const QString security = row ? row->security : password.isEmpty() ? QStringLiteral("open") : QStringLiteral("psk");
    if (security == QLatin1String("enterprise"))
        return QStringLiteral("Enterprise Wi-Fi is not supported");
    if (row && row->secured() && !row->saved && password.isEmpty())
        return QStringLiteral("Enter the password");
    if ((security == QLatin1String("psk") || security == QLatin1String("sae")) && !password.isEmpty()
        && (password.size() < 8 || password.size() > 63))
        return QStringLiteral("The password must be 8 to 63 characters");

    setConnecting({{"ssid", ssid}, {"stage", "working"}, {"message", QString()}});
    m_connectTimer.start();

    if (m_demo) {
        QTimer::singleShot(1500, this, [this, ssid, password] {
            if (password == QLatin1String("wrong"))
                return finishConnect(false, failureText(9));
            for (WifiRow &r : m_rows) {
                r.active = r.ssid == ssid;
                if (r.active)
                    r.saved = true;
            }
            m_networks->sync(m_rows, false);
            finishConnect(true, {});
        });
        return {};
    }

    const Iface *w = wifiIface();
    if (!m_managed || !w || w->path.isEmpty()) {
        finishConnect(false, QStringLiteral("Wi-Fi is not available"));
        return {};
    }
    const QString device = w->path;
    const QStringList saved = savedFor(ssid);
    m_connectCreated.clear();
    m_connectActive.clear();
    m_deviceFailure = 0;

    auto watchActive = [this](const QString &active) {
        m_connectActive = active;
        scheduleReload();
    };

    if (!saved.isEmpty() && password.isEmpty()) {
        call(kNmPath, kNm, QStringLiteral("ActivateConnection"),
             {QVariant::fromValue(QDBusObjectPath(saved.first())), QVariant::fromValue(QDBusObjectPath(device)),
              QVariant::fromValue(QDBusObjectPath(row ? row->ap : QStringLiteral("/")))},
             [watchActive](const QDBusMessage &r) { watchActive(r.arguments().value(0).value<QDBusObjectPath>().path()); },
             [this](const QDBusMessage &r) { finishConnect(false, r.errorMessage()); });
        return {};
    }

    // A new password for a saved network replaces the old profile.
    for (const QString &c : saved)
        call(c, kConnection, QStringLiteral("Delete"), {}, [](const QDBusMessage &) {}, [](const QDBusMessage &) {});

    InterfaceMap settings;
    settings[QStringLiteral("connection")] = {{"type", "802-11-wireless"}, {"id", ssid}, {"autoconnect", true}};
    QVariantMap wireless{{"ssid", ssid.toUtf8()}, {"mode", "infrastructure"}};
    if (hidden)
        wireless.insert("hidden", true);
    settings[QStringLiteral("802-11-wireless")] = wireless;
    if (security == QLatin1String("psk") || security == QLatin1String("sae"))
        settings[QStringLiteral("802-11-wireless-security")] = {{"key-mgmt", security == QLatin1String("sae") ? "sae" : "wpa-psk"},
                                                               {"psk", password}};
    else if (security == QLatin1String("owe"))
        settings[QStringLiteral("802-11-wireless-security")] = {{"key-mgmt", "owe"}};
    else if (security == QLatin1String("wep")) {
        const bool rawKey = QList<qsizetype>{5, 10, 13, 26}.contains(password.size());
        settings[QStringLiteral("802-11-wireless-security")] = {
            {"key-mgmt", "none"}, {"wep-key0", password}, {"wep-key-type", uint(rawKey ? 1 : 2)}};
    }

    call(kNmPath, kNm, QStringLiteral("AddAndActivateConnection"),
         {QVariant::fromValue(settings), QVariant::fromValue(QDBusObjectPath(device)),
          QVariant::fromValue(QDBusObjectPath(row && !hidden ? row->ap : QStringLiteral("/")))},
         [this, watchActive](const QDBusMessage &r) {
             m_connectCreated = r.arguments().value(0).value<QDBusObjectPath>().path();
             watchActive(r.arguments().value(1).value<QDBusObjectPath>().path());
         },
         [this](const QDBusMessage &r) { finishConnect(false, r.errorMessage()); });
    return {};
}

void NetworkSettings::onActiveStateChanged(uint state, uint reason, const QDBusMessage &msg)
{
    if (msg.path() != m_connectActive || m_connectActive.isEmpty())
        return;
    if (state == 2) // activated
        finishConnect(true, {});
    else if (state == 4) // deactivated
        finishConnect(false, failureText(reason));
}

void NetworkSettings::onDeviceStateChanged(uint newState, uint, uint reason, const QDBusMessage &msg)
{
    const Iface *w = wifiIface();
    if (w && msg.path() == w->path && newState == 120) // FAILED
        m_deviceFailure = reason;
}

void NetworkSettings::finishConnect(bool ok, const QString &message)
{
    if (m_connecting.value("stage") != QLatin1String("working"))
        return;
    m_connectTimer.stop();
    const QString ssid = m_connecting.value("ssid").toString();
    // The device knows why better than the active connection does.
    QString text = message;
    if (!ok && (m_deviceFailure == 7 || m_deviceFailure == 8)) // NO_SECRETS, SUPPLICANT_DISCONNECT
        text = failureText(9);
    else if (!ok && m_deviceFailure == 5) // IP_CONFIG_UNAVAILABLE
        text = failureText(4);
    m_deviceFailure = 0;
    qCInfo(lcNetCfg) << "wifi" << ssid << (ok ? "connected" : "failed:") << text;
    if (!ok && !m_connectCreated.isEmpty() && !m_demo) {
        // Never keep a profile with a wrong password around.
        call(m_connectCreated, kConnection, QStringLiteral("Delete"), {}, [](const QDBusMessage &) {},
             [](const QDBusMessage &) {});
    }
    m_connectCreated.clear();
    m_connectActive.clear();
    if (ok)
        setConnecting({});
    else
        setConnecting({{"ssid", ssid}, {"stage", "error"}, {"message", text}});
    m_resortNext = true;
    scheduleReload();
}

QString NetworkSettings::failureText(uint reason)
{
    // NMActiveConnectionStateReason
    switch (reason) {
    case 9:  // NO_SECRETS
    case 10: // LOGIN_FAILED
        return QStringLiteral("Wrong password");
    case 5:
        return QStringLiteral("Timed out");
    case 4:
        return QStringLiteral("No address from the network");
    case 2:
        return QStringLiteral("Disconnected");
    default:
        return QStringLiteral("Could not connect");
    }
}

void NetworkSettings::disconnectWifi()
{
    if (m_demo) {
        for (WifiRow &r : m_rows)
            r.active = false;
        m_networks->sync(m_rows, false);
        return;
    }
    if (const Iface *w = wifiIface(); w && !w->path.isEmpty())
        call(w->path, kDevice, QStringLiteral("Disconnect"), {}, [this](const QDBusMessage &) { scheduleReload(); });
}

void NetworkSettings::forget(const QString &ssid)
{
    if (m_demo) {
        for (WifiRow &r : m_rows)
            if (r.ssid == ssid)
                r.saved = r.active = false;
        m_networks->sync(m_rows, false);
        return;
    }
    for (const QString &c : savedFor(ssid))
        call(c, kConnection, QStringLiteral("Delete"), {}, [this](const QDBusMessage &) { scheduleReload(); });
}

QString NetworkSettings::setIpv4(const QString &name, const QString &method, const QString &address,
                                 const QString &gateway, const QString &dns)
{
    const Iface *dev = iface(name);
    if (!dev)
        return QStringLiteral("Unknown interface");
    QVariantMap ipv4{{"method", method}};
    if (method == QLatin1String("manual")) {
        QHostAddress addr;
        int prefix = 24;
        if (!parseCidr(address, &addr, &prefix))
            return QStringLiteral("Address must look like 192.168.1.42/24");
        ipv4.insert("address-data", QVariant::fromValue(MapList{{{"address", addr.toString()}, {"prefix", uint(prefix)}}}));
        if (!gateway.trimmed().isEmpty()) {
            QHostAddress gw(gateway.trimmed());
            if (gw.protocol() != QAbstractSocket::IPv4Protocol)
                return QStringLiteral("Gateway must be an IPv4 address");
            ipv4.insert("gateway", gw.toString());
        }
        QList<uint> servers;
        for (const QString &s : dns.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts)) {
            QHostAddress a(s);
            if (a.protocol() != QAbstractSocket::IPv4Protocol)
                return QStringLiteral("DNS must be IPv4 addresses");
            servers << dnsWire(a);
        }
        ipv4.insert("dns", QVariant::fromValue(servers));
    } else if (method != QLatin1String("auto")) {
        return QStringLiteral("Unknown method");
    }

    if (m_demo) {
        for (Iface &i : m_ifaces)
            if (i.name == name) {
                i.method = method;
                if (method == QLatin1String("manual")) {
                    i.address = address.contains(QLatin1Char('/')) ? address : address + QStringLiteral("/24");
                    i.gateway = gateway.trimmed();
                    i.dns = dns.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
                }
            }
        Q_EMIT changed();
        return {};
    }
    if (!m_managed || !dev->editable)
        return QStringLiteral("This connection is managed outside the launcher");
    writeIpv4(*dev, ipv4);
    return {};
}

void NetworkSettings::writeIpv4(const Iface &dev, const QVariantMap &ipv4)
{
    const QString device = dev.path;
    const QString name = dev.name;
    auto activate = [this, device](const QString &connection) {
        call(kNmPath, kNm, QStringLiteral("ActivateConnection"),
             {QVariant::fromValue(QDBusObjectPath(connection)), QVariant::fromValue(QDBusObjectPath(device)),
              QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))},
             [this](const QDBusMessage &) { scheduleReload(); });
    };
    if (dev.connection.isEmpty()) {
        InterfaceMap settings;
        settings[QStringLiteral("connection")] = {{"type", "802-3-ethernet"}, {"id", QStringLiteral("Ethernet %1").arg(name)},
                                                  {"interface-name", name}, {"autoconnect", true}};
        settings[QStringLiteral("ipv4")] = ipv4;
        call(kNmPath, kNm, QStringLiteral("AddAndActivateConnection"),
             {QVariant::fromValue(settings), QVariant::fromValue(QDBusObjectPath(device)),
              QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))},
             [this](const QDBusMessage &) { scheduleReload(); });
        return;
    }
    const QString connection = dev.connection;
    call(connection, kConnection, QStringLiteral("GetSettings"), {}, [this, connection, ipv4, activate](const QDBusMessage &r) {
        InterfaceMap settings = qdbus_cast<InterfaceMap>(r.arguments().value(0));
        QVariantMap current = settings.value(QStringLiteral("ipv4"));
        // Drop every address/DNS key (including deprecated ones that would win over the new data).
        for (const char *k : {"addresses", "address-data", "gateway", "dns", "dns-data", "method"})
            current.remove(QString::fromLatin1(k));
        for (auto it = ipv4.cbegin(); it != ipv4.cend(); ++it)
            current.insert(it.key(), it.value());
        if (ipv4.value("method") == QLatin1String("auto")) {
            current.insert("address-data", QVariant::fromValue(MapList{}));
            current.insert("dns", QVariant::fromValue(QList<uint>{}));
        }
        settings.insert(QStringLiteral("ipv4"), current);
        call(connection, kConnection, QStringLiteral("Update"), {QVariant::fromValue(settings)},
             [activate, connection](const QDBusMessage &) { activate(connection); });
    });
}

// ---------------------------------------------------------------- NetworkManager state

void NetworkSettings::onSignal(const QDBusMessage &msg)
{
    if (msg.member() == QLatin1String("Updated") || msg.member() == QLatin1String("Removed"))
        m_saved.remove(msg.path());
    scheduleReload();
}

void NetworkSettings::scheduleReload()
{
    if (m_demo)
        return;
    if (!m_reloadTimer.isActive())
        m_reloadTimer.start();
}

void NetworkSettings::reload()
{
    if (m_demo)
        return;
    if (!m_managed) {
        reloadFallback();
        return;
    }
    const int gen = ++m_generation;
    QDBusMessage msg = QDBusMessage::createMethodCall(kNm, QStringLiteral("/org/freedesktop"),
                                                      QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                                      QStringLiteral("GetManagedObjects"));
    auto *w = new QDBusPendingCallWatcher(bus().asyncCall(msg, 10000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, gen](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (gen != m_generation)
            return;
        QDBusPendingReply<ObjectMap> reply = *w;
        if (reply.isError()) {
            qCWarning(lcNetCfg) << "NetworkManager unavailable:" << reply.error().message();
            m_managed = false;
            reloadFallback();
            return;
        }
        QMap<QString, InterfaceMap> objects;
        const ObjectMap raw = reply.value();
        for (auto it = raw.cbegin(); it != raw.cend(); ++it)
            objects.insert(it.key().path(), it.value());
        applyObjects(objects);
    });
}

void NetworkSettings::fetchSettings(const QString &connection)
{
    if (m_fetching.contains(connection))
        return;
    m_fetching.insert(connection);
    call(connection, kConnection, QStringLiteral("GetSettings"), {},
         [this, connection](const QDBusMessage &r) {
             m_fetching.remove(connection);
             const InterfaceMap s = qdbus_cast<InterfaceMap>(r.arguments().value(0));
             Saved saved;
             saved.type = s.value(QStringLiteral("connection")).value(QStringLiteral("type")).toString();
             saved.ssid = QString::fromUtf8(s.value(QStringLiteral("802-11-wireless")).value(QStringLiteral("ssid")).toByteArray());
             saved.method = s.value(QStringLiteral("ipv4")).value(QStringLiteral("method")).toString();
             m_saved.insert(connection, saved);
             scheduleReload();
         },
         [this, connection](const QDBusMessage &) { m_fetching.remove(connection); });
}

void NetworkSettings::applyObjects(const QMap<QString, QMap<QString, QVariantMap>> &objects)
{
    const QVariantMap nm = objects.value(kNmPath).value(kNm);
    m_wifiEnabled = nm.value("WirelessEnabled").toBool();

    // Saved profiles: settings are fetched once per connection and cached.
    QSet<QString> connections;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it)
        if (it.value().contains(kConnection))
            connections.insert(it.key());
    for (const QString &c : m_saved.keys())
        if (!connections.contains(c))
            m_saved.remove(c);
    for (const QString &c : connections)
        if (!m_saved.contains(c))
            fetchSettings(c);

    QList<Iface> ifaces;
    QString wifiActiveAp;
    for (const QString &path : DBusTypes::pathList(nm.value("Devices"))) {
        const QMap<QString, QVariantMap> obj = objects.value(path);
        const QVariantMap d = obj.value(kDevice);
        const uint type = d.value("DeviceType").toUInt();
        if (type != 1 && type != 2) // ethernet, wifi
            continue;
        if (type == 2 && !d.value("Managed").toBool())
            continue; // a radio NetworkManager ignores (e.g. used as an access point)
        Iface i;
        i.name = d.value("Interface").toString();
        i.kind = type == 2 ? QStringLiteral("wifi") : QStringLiteral("ethernet");
        i.path = path;
        i.state = deviceState(d.value("State").toUInt());
        i.editable = d.value("Managed").toBool();
        if (type == 1) {
            const QVariantMap wired = obj.value(kWired);
            i.carrier = wired.contains("Carrier") ? wired.value("Carrier").toBool()
                                                  : readTrimmed(QStringLiteral("/sys/class/net/%1/carrier").arg(i.name)) == QLatin1String("1");
            i.speed = int(wired.value("Speed").toUInt());
        } else {
            i.activeAp = DBusTypes::path(obj.value(kWireless).value("ActiveAccessPoint"));
            i.carrier = i.state == QLatin1String("connected");
            wifiActiveAp = i.activeAp;
        }
        const QString ip4 = DBusTypes::path(d.value("Ip4Config"));
        if (ip4.size() > 1 && i.state == QLatin1String("connected")) {
            const QVariantMap c = objects.value(ip4).value(kIp4);
            const MapList addrs = DBusTypes::mapList(c.value("AddressData"));
            if (!addrs.isEmpty())
                i.address = addrs.first().value("address").toString() + QLatin1Char('/')
                    + QString::number(addrs.first().value("prefix").toUInt());
            i.gateway = c.value("Gateway").toString();
            for (const QVariantMap &ns : DBusTypes::mapList(c.value("NameserverData")))
                i.dns << ns.value("address").toString();
        }
        const QString active = DBusTypes::path(d.value("ActiveConnection"));
        if (active.size() > 1)
            i.connection = DBusTypes::path(objects.value(active).value(kActive).value("Connection"));
        if (i.connection.isEmpty() && type == 1) {
            const QStringList available = DBusTypes::pathList(d.value("AvailableConnections"));
            if (!available.isEmpty())
                i.connection = available.first();
        }
        i.method = m_saved.value(i.connection).method;
        if (i.method.isEmpty() && i.connection.isEmpty())
            i.method = QStringLiteral("auto"); // NetworkManager's default for a new profile
        ifaces << i;
    }
    std::stable_sort(ifaces.begin(), ifaces.end(),
                     [](const Iface &a, const Iface &b) { return a.kind == QLatin1String("ethernet") && b.kind != a.kind; });
    m_ifaces = ifaces;
    if (!iface(m_selected) && !m_ifaces.isEmpty())
        m_selected = m_ifaces.first().name;

    // Visible networks, one row per SSID (strongest access point wins).
    QHash<QString, WifiRow> bySsid;
    if (const Iface *w = wifiIface(); w && m_wifiEnabled) {
        const QVariantMap wireless = objects.value(w->path).value(kWireless);
        for (const QString &ap : DBusTypes::pathList(wireless.value("AccessPoints"))) {
            const QVariantMap a = objects.value(ap).value(kAp);
            const QString ssid = QString::fromUtf8(a.value("Ssid").toByteArray());
            if (ssid.isEmpty())
                continue; // hidden: reachable through "Hidden network"
            WifiRow r;
            r.ssid = ssid;
            r.strength = a.value("Strength").toInt();
            r.security = securityFor(a.value("Flags").toUInt(), a.value("WpaFlags").toUInt(), a.value("RsnFlags").toUInt());
            r.ap = ap;
            r.active = ap == wifiActiveAp;
            r.saved = !savedFor(ssid).isEmpty();
            const auto existing = bySsid.constFind(ssid);
            if (existing == bySsid.cend() || r.active || (!existing->active && r.strength > existing->strength))
                bySsid.insert(ssid, r);
        }
    }
    m_rows = bySsid.values();
    m_networks->sync(m_rows, m_resortNext);
    m_resortNext = false;

    // Status row: the kind of the primary connection, and its signal.
    const QString primary = DBusTypes::path(nm.value("PrimaryConnection"));
    const QString type = objects.value(primary).value(kActive).value("Type").toString();
    m_primaryKind = type == QLatin1String("802-11-wireless") ? QStringLiteral("wifi") : QStringLiteral("ethernet");
    m_wifiBars = 0;
    QString activeSsid;
    for (const WifiRow &r : m_rows)
        if (r.active) {
            m_wifiBars = WifiModel::bars(r.strength);
            activeSsid = r.ssid;
        }
    if (m_wifiKnown && activeSsid != m_wifiSsid) {
        if (!activeSsid.isEmpty())
            Q_EMIT wifiLink(activeSsid, true);
        else
            Q_EMIT wifiLink(m_wifiSsid, false);
    }
    m_wifiKnown = true;
    m_wifiSsid = activeSsid;

    // A connection being watched may already be up (signal raced the reply).
    if (!m_connectActive.isEmpty() && objects.contains(m_connectActive)) {
        const uint state = objects.value(m_connectActive).value(kActive).value("State").toUInt();
        if (state == 2)
            finishConnect(true, {});
    }
    Q_EMIT changed();
}

// ---------------------------------------------------------------- without NetworkManager

void NetworkSettings::reloadFallback()
{
    if (m_demo || m_managed)
        return;
    const QByteArray route = readAll(QStringLiteral("/proc/net/route"));
    const QStringList dns = nameservers(readAll(QStringLiteral("/etc/resolv.conf")));
    QList<Iface> ifaces;
    const QStringList names = QDir(QStringLiteral("/sys/class/net")).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
    for (const QString &name : names) {
        const QString base = QStringLiteral("/sys/class/net/") + name;
        if (!QFileInfo::exists(base + QStringLiteral("/device")))
            continue; // loopback, bridges, veth, …
        Iface i;
        i.name = name;
        const bool wifi = QFileInfo::exists(base + QStringLiteral("/wireless")) || QFileInfo::exists(base + QStringLiteral("/phy80211"));
        i.kind = wifi ? QStringLiteral("wifi") : QStringLiteral("ethernet");
        i.carrier = readTrimmed(base + QStringLiteral("/carrier")) == QLatin1String("1");
        i.speed = readTrimmed(base + QStringLiteral("/speed")).toInt();
        const QNetworkInterface ni = QNetworkInterface::interfaceFromName(name);
        for (const QNetworkAddressEntry &e : ni.addressEntries())
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                i.address = e.ip().toString() + QLatin1Char('/') + QString::number(e.prefixLength());
                break;
            }
        i.gateway = defaultGateway(route, name);
        if (!i.address.isEmpty())
            i.dns = dns;
        i.state = !i.carrier ? QStringLiteral("unavailable")
            : i.address.isEmpty() ? QStringLiteral("disconnected") : QStringLiteral("connected");
        ifaces << i;
    }
    std::stable_sort(ifaces.begin(), ifaces.end(),
                     [](const Iface &a, const Iface &b) { return a.kind == QLatin1String("ethernet") && b.kind != a.kind; });
    m_ifaces = ifaces;
    if (!iface(m_selected) && !m_ifaces.isEmpty())
        m_selected = m_ifaces.first().name;
    m_wifiEnabled = false;
    m_rows.clear();
    m_networks->sync(m_rows, true);
    const QString primary = defaultRouteInterface(route);
    const Iface *p = iface(primary);
    m_primaryKind = p ? p->kind : QStringLiteral("ethernet");
    m_wifiBars = p && p->kind == QLatin1String("wifi") ? 3 : 0;
    Q_EMIT changed();
}

// ---------------------------------------------------------------- helpers

QString NetworkSettings::securityFor(uint flags, uint wpaFlags, uint rsnFlags)
{
    const uint mgmt = wpaFlags | rsnFlags;
    if (mgmt & (0x200 | 0x2000)) return QStringLiteral("enterprise"); // 802.1X, Suite-B
    if (mgmt & 0x100) return QStringLiteral("psk");
    if (mgmt & 0x400) return QStringLiteral("sae");
    if (mgmt & (0x800 | 0x1000)) return QStringLiteral("owe");
    if (flags & 0x1) return QStringLiteral("wep");
    return QStringLiteral("open");
}

QString NetworkSettings::speedText(int mbps)
{
    if (mbps <= 0)
        return {};
    if (mbps >= 1000 && mbps % 100 == 0)
        return QString::number(mbps / 1000.0, 'g', 3) + QStringLiteral(" Gb/s");
    return QString::number(mbps) + QStringLiteral(" Mb/s");
}

QString NetworkSettings::defaultRouteInterface(const QByteArray &table)
{
    const QList<QByteArray> lines = table.split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> f = lines.at(i).simplified().split(' ');
        if (f.size() >= 8 && f.at(1) == "00000000" && f.at(7) == "00000000")
            return QString::fromLatin1(f.at(0));
    }
    return {};
}

QString NetworkSettings::defaultGateway(const QByteArray &table, const QString &interface)
{
    const QList<QByteArray> lines = table.split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> f = lines.at(i).simplified().split(' ');
        if (f.size() < 8 || QString::fromLatin1(f.at(0)) != interface || f.at(1) != "00000000" || f.at(7) != "00000000")
            continue;
        bool ok = false;
        const quint32 le = f.at(2).toUInt(&ok, 16);
        if (!ok || le == 0)
            return {};
        return QHostAddress(qFromBigEndian(le)).toString(); // kernel stores network order
    }
    return {};
}

QStringList NetworkSettings::nameservers(const QByteArray &resolvConf)
{
    QStringList out;
    for (const QByteArray &line : resolvConf.split('\n')) {
        const QList<QByteArray> f = line.simplified().split(' ');
        if (f.size() >= 2 && f.at(0) == "nameserver")
            out << QString::fromLatin1(f.at(1));
    }
    return out;
}

bool NetworkSettings::parseCidr(const QString &text, QHostAddress *address, int *prefix)
{
    const QString t = text.trimmed();
    const QString host = t.section(QLatin1Char('/'), 0, 0);
    QHostAddress a(host);
    if (a.protocol() != QAbstractSocket::IPv4Protocol || host.count(QLatin1Char('.')) != 3)
        return false;
    int p = 24;
    if (t.contains(QLatin1Char('/'))) {
        bool ok = false;
        p = t.section(QLatin1Char('/'), 1).toInt(&ok);
        if (!ok || p < 1 || p > 32)
            return false;
    }
    *address = a;
    *prefix = p;
    return true;
}

quint32 NetworkSettings::dnsWire(const QHostAddress &address)
{
    // NetworkManager wants the address bytes in network order inside the uint32.
    return qToBigEndian(address.toIPv4Address());
}

// ---------------------------------------------------------------- demo

void NetworkSettings::populateDemo()
{
    m_managed = true;
    m_wifiEnabled = true;
    Iface eth;
    eth.name = QStringLiteral("eth0");
    eth.kind = QStringLiteral("ethernet");
    eth.carrier = true;
    eth.speed = 1000;
    eth.state = QStringLiteral("connected");
    eth.address = QStringLiteral("192.168.1.42/24");
    eth.gateway = QStringLiteral("192.168.1.1");
    eth.dns = {QStringLiteral("192.168.1.1")};
    eth.method = QStringLiteral("auto");
    eth.editable = true;
    Iface wlan;
    wlan.name = QStringLiteral("wlan0");
    wlan.kind = QStringLiteral("wifi");
    wlan.state = QStringLiteral("connected");
    wlan.carrier = true;
    m_ifaces = {eth, wlan};
    if (m_selected.isEmpty())
        m_selected = wlan.name;
    m_rows = {
        {QStringLiteral("Living Room 5G"), 88, QStringLiteral("psk"), true, true, {}},
        {QStringLiteral("Living Room 2.4G"), 74, QStringLiteral("psk"), false, true, {}},
        {QStringLiteral("Neighbours"), 48, QStringLiteral("sae"), false, false, {}},
        {QStringLiteral("Café Guest"), 30, QStringLiteral("open"), false, false, {}},
    };
    m_networks->sync(m_rows, true);
    m_primaryKind = QStringLiteral("ethernet");
    m_wifiBars = 3;
}

void NetworkSettings::demoShow(const QString &view)
{
    m_selected = view == QLatin1String("network-ethernet") ? QStringLiteral("eth0") : QStringLiteral("wlan0");
    if (view == QLatin1String("network-wifi")) {
        m_scanRemaining = 4;
        Q_EMIT scanChanged();
    }
    Q_EMIT changed();
}
