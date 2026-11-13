#pragma once

#include "WifiModel.h"

#include <QDBusMessage>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

#include <functional>

class Config;
class QDBusServiceWatcher;

// The Network screen: Ethernet details and Wi-Fi through NetworkManager's
// D-Bus API. Without NetworkManager it still shows the interfaces and their
// addresses (read from the kernel), read-only.
class NetworkSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool managed READ managed NOTIFY changed)
    Q_PROPERTY(QVariantList interfaces READ interfaces NOTIFY changed)
    Q_PROPERTY(QString selected READ selected NOTIFY changed)
    Q_PROPERTY(QVariantMap details READ details NOTIFY changed)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY changed)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanChanged)
    Q_PROPERTY(int scanRemaining READ scanRemaining NOTIFY scanChanged)
    Q_PROPERTY(int scanDuration READ scanDuration CONSTANT)
    Q_PROPERTY(WifiModel *networks READ networks CONSTANT)
    Q_PROPERTY(QVariantMap connecting READ connecting NOTIFY connectingChanged)
    Q_PROPERTY(QString primaryKind READ primaryKind NOTIFY changed)
    Q_PROPERTY(int wifiBars READ wifiBars NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit NetworkSettings(const Config *config, bool demo, QObject *parent = nullptr);

    bool managed() const { return m_managed; }
    QVariantList interfaces() const;
    QString selected() const { return m_selected; }
    QVariantMap details() const;
    bool wifiEnabled() const { return m_wifiEnabled; }
    bool scanning() const { return m_scanRemaining > 0; }
    int scanRemaining() const { return m_scanRemaining; }
    int scanDuration() const { return m_scanDuration; }
    WifiModel *networks() const { return m_networks; }
    QVariantMap connecting() const { return m_connecting; }
    QString primaryKind() const { return m_primaryKind; }
    int wifiBars() const { return m_wifiBars; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void select(const QString &name);
    Q_INVOKABLE void setWifiEnabled(bool on);
    Q_INVOKABLE void toggleScan();
    Q_INVOKABLE bool needsPassword(const QString &ssid) const;
    // Connect to a visible (or, with hidden=true, a hidden) network. Returns an
    // error for requests that cannot start; the outcome arrives in `connecting`.
    Q_INVOKABLE QString connectNetwork(const QString &ssid, const QString &password, bool hidden = false);
    Q_INVOKABLE void disconnectWifi();
    Q_INVOKABLE void forget(const QString &ssid);
    // method "auto" or "manual"; address as a.b.c.d[/prefix]; dns separated by spaces or commas.
    Q_INVOKABLE QString setIpv4(const QString &name, const QString &method, const QString &address,
                                const QString &gateway, const QString &dns);
    Q_INVOKABLE void clearConnecting();
    Q_INVOKABLE void clearError();

    // Pure helpers (unit tested)
    static QString securityFor(uint flags, uint wpaFlags, uint rsnFlags);
    static QString speedText(int mbps);
    static QString defaultRouteInterface(const QByteArray &procNetRoute);
    static QString defaultGateway(const QByteArray &procNetRoute, const QString &interface);
    static QStringList nameservers(const QByteArray &resolvConf);
    static bool parseCidr(const QString &text, QHostAddress *address, int *prefix);
    static quint32 dnsWire(const QHostAddress &address);
    static QString failureText(uint activeStateReason);

    void demoShow(const QString &view);

Q_SIGNALS:
    void changed();
    void scanChanged();
    void connectingChanged();
    void lastErrorChanged();
    void wifiLink(const QString &ssid, bool up); // after the first state

private Q_SLOTS:
    void onSignal(const QDBusMessage &msg);
    void onActiveStateChanged(uint state, uint reason, const QDBusMessage &msg);
    void onDeviceStateChanged(uint newState, uint oldState, uint reason, const QDBusMessage &msg);

private:
    struct Iface {
        QString name;
        QString kind;   // ethernet | wifi
        QString path;   // NetworkManager device, empty without it
        bool carrier = false;
        int speed = 0;
        QString state;  // connected | connecting | disconnected | unavailable | unmanaged
        QString address;
        QString gateway;
        QStringList dns;
        QString method; // auto | manual | ""
        QString connection;
        QString activeAp;
        bool editable = false;
    };
    struct Saved {
        QString type;
        QString ssid;
        QString method;
    };

    void scheduleReload();
    void reload();
    void applyObjects(const QMap<QString, QMap<QString, QVariantMap>> &objects);
    void fetchSettings(const QString &connection);
    void reloadFallback();
    void setLastError(const QString &message);
    const Iface *iface(const QString &name) const;
    const Iface *wifiIface() const;
    void call(const QString &path, const QString &iface, const QString &method, const QVariantList &args,
              std::function<void(const QDBusMessage &)> ok, std::function<void(const QDBusMessage &)> fail = {});
    void requestScan();
    void finishConnect(bool ok, const QString &message);
    void setConnecting(const QVariantMap &c);
    QStringList savedFor(const QString &ssid) const;
    void writeIpv4(const Iface &dev, const QVariantMap &ipv4);
    void populateDemo();

    const Config *m_config;
    bool m_demo;
    bool m_managed = false;
    bool m_open = false;
    QList<Iface> m_ifaces;
    QString m_selected;
    bool m_wifiEnabled = false;
    QString m_primaryKind = QStringLiteral("ethernet");
    int m_wifiBars = 0;
    QList<WifiRow> m_rows;
    WifiModel *m_networks;
    QHash<QString, Saved> m_saved;
    QSet<QString> m_fetching;

    int m_scanDuration = 10;
    int m_scanRemaining = 0;
    QTimer m_scanTimer;
    QTimer m_reloadTimer;
    QTimer m_fallbackTimer;
    QTimer m_connectTimer;
    bool m_resortNext = true;
    bool m_wifiKnown = false;
    QString m_wifiSsid;
    int m_generation = 0;

    QVariantMap m_connecting;
    QString m_connectActive;   // active connection being watched
    QString m_connectCreated;  // connection created for this attempt (deleted on failure)
    uint m_deviceFailure = 0;  // NMDeviceStateReason of the Wi-Fi device's last failure
    QString m_lastError;
    QDBusServiceWatcher *m_watcher = nullptr;
};
