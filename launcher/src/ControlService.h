#pragma once

#include <QObject>
#include <QStringList>

class IdleMonitor;
class LifecycleController;
class NetworkSettings;
class OverlayState;
class SoundManager;
class Notifier;
class MoonlightLibrary;

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
    Q_PROPERTY(int Volume READ volume SCRIPTABLE true)

public:
    ControlService(LifecycleController *life, NetworkSettings *network, IdleMonitor *idle, OverlayState *overlay,
                   QObject *parent = nullptr);
    bool registerOnBus();

    QString state() const;
    QString appId() const;
    bool idle() const;
    bool overlayOpen() const;
    int volume() const;
    void setSound(SoundManager *sound, Notifier *notifier) { m_sound = sound; m_notifier = notifier; }
    void setMoonlight(MoonlightLibrary *moonlight) { m_moonlight = moonlight; }

public Q_SLOTS:
    Q_SCRIPTABLE void Launch(const QString &id);
    Q_SCRIPTABLE void Home();
    Q_SCRIPTABLE void OpenBluetooth();
    Q_SCRIPTABLE void OpenNetwork();
    Q_SCRIPTABLE void EnterIdle();
    // In-app menu over the running app (ignored on the launcher); e.g. for a compositor keybind.
    Q_SCRIPTABLE void ToggleOverlay();
    // Sound: "name label" per output, the current one marked with "*"
    Q_SCRIPTABLE QStringList SoundOutputs();
    Q_SCRIPTABLE void SetSoundOutput(const QString &name);
    Q_SCRIPTABLE void SetVolume(int percent);
    Q_SCRIPTABLE void VolumeStep(int direction);
    // Titles of the visible notifications, newest first.
    Q_SCRIPTABLE QStringList Notifications();
    Q_SCRIPTABLE void Wake();
    // Moonlight library
    Q_SCRIPTABLE void OpenMoonlight();
    // "uuid state name" per paired host (state: checking, online, offline, waking)
    Q_SCRIPTABLE QStringList MoonlightHosts();
    // "address name" per host found on the network (unpaired)
    Q_SCRIPTABLE QStringList MoonlightFound();
    // "id name" per app of the selected host, "*" appended to the running one
    Q_SCRIPTABLE QStringList MoonlightApps();
    // Stream an app (host by name or uuid); returns an error or "".
    Q_SCRIPTABLE QString MoonlightStream(const QString &host, const QString &app);
    // End the stream and quit the game on the host.
    Q_SCRIPTABLE void MoonlightStopPlaying();
    // "appName@hostName" of the running stream, "" otherwise.
    Q_SCRIPTABLE QString Session();
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
    void openMoonlightRequested();

private:
    LifecycleController *m_life;
    NetworkSettings *m_network;
    IdleMonitor *m_idle;
    OverlayState *m_overlay;
    SoundManager *m_sound = nullptr;
    Notifier *m_notifier = nullptr;
    MoonlightLibrary *m_moonlight = nullptr;
};
