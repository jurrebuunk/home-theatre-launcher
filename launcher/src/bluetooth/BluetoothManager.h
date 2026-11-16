#pragma once

#include "DeviceModel.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class Config;
class QDBusServiceWatcher;

// BlueZ over D-Bus: adapters, discovery, paired/nearby devices, pairing agent.
class BluetoothManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY changed)
    Q_PROPERTY(bool hasAdapter READ hasAdapter NOTIFY changed)
    Q_PROPERTY(QStringList adapterNames READ adapterNames NOTIFY changed)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    Q_PROPERTY(bool discovering READ discovering NOTIFY changed)
    Q_PROPERTY(int scanRemaining READ scanRemaining NOTIFY scanRemainingChanged)
    Q_PROPERTY(int scanDuration READ scanDuration CONSTANT)
    Q_PROPERTY(int pairingTimeout READ pairingTimeout CONSTANT)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY changed)
    Q_PROPERTY(DeviceModel *paired READ paired CONSTANT)
    Q_PROPERTY(DeviceModel *nearby READ nearby CONSTANT)
    Q_PROPERTY(QVariantMap request READ request NOTIFY requestChanged)
    Q_PROPERTY(QVariantMap pairing READ pairing NOTIFY pairingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString errorPath READ errorPath NOTIFY lastErrorChanged)

public:
    explicit BluetoothManager(const Config *config, bool demo, QObject *parent = nullptr);

    bool serviceAvailable() const { return m_serviceAvailable; }
    bool hasAdapter() const { return !m_adapters.isEmpty(); }
    QStringList adapterNames() const;
    QString adapterName() const;
    bool powered() const;
    bool discovering() const;
    int scanRemaining() const { return m_scanRemaining; }
    int scanDuration() const { return m_scanDuration; }
    int pairingTimeout() const { return m_pairingTimeout; }
    int connectedCount() const;
    DeviceModel *paired() const { return m_paired; }
    DeviceModel *nearby() const { return m_nearby; }
    QVariantMap request() const { return m_request; }
    QVariantMap pairing() const { return m_pairing; }
    QString lastError() const { return m_lastError; }
    QString errorPath() const { return m_errorPath; }

    Q_INVOKABLE void retry();
    Q_INVOKABLE void selectAdapter(int delta);
    Q_INVOKABLE void setPowered(bool on);
    Q_INVOKABLE void toggleScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void connectDevice(const QString &path);
    Q_INVOKABLE void disconnectDevice(const QString &path);
    Q_INVOKABLE void setTrusted(const QString &path, bool trusted);
    Q_INVOKABLE void forget(const QString &path);
    Q_INVOKABLE void pair(const QString &path);
    Q_INVOKABLE void cancelPairing();
    Q_INVOKABLE void setTrustOnPair(bool trust) { m_trustOnPair = trust; }
    // Answer the outstanding agent request (value: PIN / passkey text where needed).
    Q_INVOKABLE void acceptRequest(const QString &value = {});
    Q_INVOKABLE void rejectRequest();
    Q_INVOKABLE void clearError();
    // Leaving the Bluetooth screen: stop discovery, cancel pending prompts.
    Q_INVOKABLE void close();

    // name, kind, battery (-1 unknown), connected
    Q_INVOKABLE QVariantMap deviceInfo(const QString &path) const;
    int batteryForAddress(const QString &address) const;
    // Disconnects the connected device with this address (a controller then turns off).
    void disconnectAddress(const QString &address);

    // Demo/screenshot support
    void demoShowPairing();

Q_SIGNALS:
    void changed();
    void scanRemainingChanged();
    void requestChanged();
    void pairingChanged();
    void lastErrorChanged();
    void connectionChanged(const QString &path, bool connected);
    void batteryChanged(const QString &path, int percent);

private Q_SLOTS:
    void onInterfacesAdded(const QDBusObjectPath &path, const QMap<QString, QVariantMap> &interfaces);
    void onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    void onPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated,
                             const QDBusMessage &msg);

private:
    QString currentAdapter() const;
    void loadObjects();
    void registerAgent();
    void rebuild();
    void call(const QString &path, const QString &iface, const QString &method, const QVariantList &args,
              int timeoutMs, std::function<void(const QDBusMessage &)> ok);
    void setProperty(const QString &path, const QString &iface, const QString &name, const QVariant &value);
    void setBusy(const QString &path, bool busy);
    void reportError(const QString &path, const QDBusMessage &reply);
    static QString friendlyError(const QString &name, const QString &message);
    void handleAgent(const QString &type, const QString &device, const QVariant &value, const QDBusMessage &msg);
    void cancelAgent();
    void finishPairing(bool ok);
    QString deviceName(const QString &path) const;
    void populateDemo();

    const Config *m_config;
    bool m_demo;
    bool m_serviceAvailable = false;
    QStringList m_adapters;              // object paths, sorted
    int m_adapterIndex = 0;
    QString m_preferredAdapter;
    QHash<QString, QVariantMap> m_adapterProps;
    QHash<QString, QVariantMap> m_devices;
    DeviceModel *m_paired;
    DeviceModel *m_nearby;

    int m_scanDuration = 30;
    int m_pairingTimeout = 60;
    int m_scanRemaining = 0;
    QTimer m_scanTimer;

    QString m_capability;
    QVariantMap m_request;
    QDBusMessage m_pendingAgentMsg;
    QVariantMap m_pairing;
    bool m_trustOnPair = false;
    QString m_connectingPath;
    QString m_lastError;
    QString m_errorPath;
    QDBusServiceWatcher *m_watcher = nullptr;
    QObject *m_agentObject = nullptr;
};
