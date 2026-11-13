#pragma once

#include <QObject>
#include <QStringList>

class IdleMonitor;
class LifecycleController;
class NetworkSettings;
class OverlayState;

// Session bus API nl.jurrebuunk.HomeTheatre1 at /nl/jurrebuunk/HomeTheatre,
// used by tests and automation (e.g. a compositor keybinding for Home).
class ControlService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "nl.jurrebuunk.HomeTheatre1")
    Q_PROPERTY(QString State READ state NOTIFY StateChanged SCRIPTABLE true)
    Q_PROPERTY(QString AppId READ appId SCRIPTABLE true)
    Q_PROPERTY(bool Idle READ idle SCRIPTABLE true)
    Q_PROPERTY(bool OverlayOpen READ overlayOpen SCRIPTABLE true)

public:
    ControlService(LifecycleController *life, NetworkSettings *network, IdleMonitor *idle, OverlayState *overlay,
                   QObject *parent = nullptr);
    bool registerOnBus();

    QString state() const;
    QString appId() const;
    bool idle() const;
    bool overlayOpen() const;

public Q_SLOTS:
    Q_SCRIPTABLE void Launch(const QString &id);
    Q_SCRIPTABLE void Home();
    Q_SCRIPTABLE void OpenBluetooth();
    Q_SCRIPTABLE void OpenNetwork();
    Q_SCRIPTABLE void EnterIdle();
    // In-app menu over the running app (ignored on the launcher); e.g. for a compositor keybind.
    Q_SCRIPTABLE void ToggleOverlay();
    Q_SCRIPTABLE void Wake();
    // "name kind" per interface, e.g. "eth0 ethernet"
    Q_SCRIPTABLE QStringList NetworkInterfaces();
    Q_SCRIPTABLE QStringList WifiNetworks();
    Q_SCRIPTABLE void WifiScan();
    // Starts connecting; returns an error only when the request cannot start.
    Q_SCRIPTABLE QString WifiConnect(const QString &ssid, const QString &password);
    // Outcome of the last WifiConnect: "", "working" or "error: <message>".
    Q_SCRIPTABLE QString WifiConnectStatus();
    Q_SCRIPTABLE QString SetIpv4(const QString &name, const QString &method, const QString &address,
                                 const QString &gateway, const QString &dns);

Q_SIGNALS:
    Q_SCRIPTABLE void StateChanged(const QString &state);
    void openBluetoothRequested();
    void openNetworkRequested();

private:
    LifecycleController *m_life;
    NetworkSettings *m_network;
    IdleMonitor *m_idle;
    OverlayState *m_overlay;
};
