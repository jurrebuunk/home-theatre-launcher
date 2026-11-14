#pragma once

#include "MoonlightProtocol.h"

#include <QDBusMessage>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

#include <functional>

class Config;
class LifecycleController;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

// The Moonlight library: paired hosts and their apps (box art, what is
// running), Wake-on-LAN, discovery (Avahi), pairing and streaming. Hosts live
// in moonlight-qt's own settings, so the stock client keeps working; status,
// apps and art come straight from the host's GameStream/Sunshine API with
// moonlight's client certificate. Pairing and streaming run moonlight-qt.
class MoonlightLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QVariantList hosts READ hosts NOTIFY hostsChanged)
    Q_PROPERTY(QVariantList found READ found NOTIFY hostsChanged)
    Q_PROPERTY(QString host READ host NOTIFY hostsChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY hostsChanged)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(bool loadingApps READ loadingApps NOTIFY appsChanged)
    Q_PROPERTY(QVariantMap pairing READ pairing NOTIFY pairingChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    MoonlightLibrary(const Config *config, LifecycleController *life, bool demo, QObject *parent = nullptr);
    ~MoonlightLibrary() override;

    bool available() const { return !m_appId.isEmpty(); }
    QVariantList hosts() const;
    QVariantList found() const;
    QString host() const { return m_selected; }
    QVariantMap current() const;
    QVariantList apps() const;
    bool loadingApps() const;
    QVariantMap pairing() const { return m_pairing; }
    QString message() const { return m_message; }

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void selectHost(const QString &uuid);
    Q_INVOKABLE void cycleHost();
    // "" when the stream starts; "quit" when another app runs on the host
    // (confirm, then quitAndPlay); "offline" when the host is not reachable.
    Q_INVOKABLE QString play(int appId);
    Q_INVOKABLE void quitAndPlay(int appId);
    Q_INVOKABLE void wake(const QString &uuid);
    Q_INVOKABLE void setMain(const QString &uuid);
    Q_INVOKABLE void remove(const QString &uuid);
    // Starts pairing with an address ("" on success, else why not).
    Q_INVOKABLE QString pair(const QString &address);
    Q_INVOKABLE void cancelPair();
    // In-app menu: end the stream and quit the game on the host.
    Q_INVOKABLE void stopPlaying();
    Q_INVOKABLE void clearMessage();

    // Control API
    QString streamByName(const QString &host, const QString &app);

    void demoShow(const QString &view);

Q_SIGNALS:
    void hostsChanged();
    void appsChanged();
    void pairingChanged();
    void messageChanged();
    void stopped(const QString &appName, const QString &hostName);

private Q_SLOTS:
    void onAvahiItemNew(int interface, int protocol, const QString &name, const QString &type, const QString &domain,
                        uint flags);
    void onAvahiItemRemove(int interface, int protocol, const QString &name, const QString &type,
                           const QString &domain, uint flags);

private:
    struct HostState {
        Moonlight::Host conf;
        QString state = QStringLiteral("checking"); // checking | online | offline | waking
        Moonlight::Address active;
        int httpsPort = 47984;
        int currentGame = 0;
        qint64 wakeUntil = 0;
        QList<Moonlight::App> apps;
        bool appsLoaded = false;
        bool busy = false;
    };
    struct Found {
        QString name;
        Moonlight::Address address;
        QString uuid;
        QString avahiName;
    };
    using Reply = std::function<void(bool ok, const QByteArray &body)>;

    void reload();
    void refreshAll();
    void refresh(const QString &uuid);
    void probe(const QString &uuid, int index);
    void fetchSecure(const QString &uuid);
    void fetchArt(const HostState &h);
    void get(const Moonlight::Address &address, bool https, const QString &path, const QList<QPair<QString, QString>> &query,
             const QByteArray &pinnedCert, int timeoutMs, Reply done);
    void quitOnHost(const QString &uuid, std::function<void(bool)> done);
    void launch(const HostState &h, const Moonlight::App &app);
    HostState *state(const QString &uuid);
    const HostState *state(const QString &uuid) const;
    QString artPath(const QString &uuid, int appId) const;
    void setMessage(const QString &m);
    void setPairing(const QVariantMap &p);
    void finishPair(bool ok, const QString &error);
    void checkPaired();
    void startDiscovery();
    void stopDiscovery();
    void addFound(const Found &f);
    void loadMain();
    void saveMain();

    const Config *m_config;
    LifecycleController *m_life;
    bool m_demo;
    QString m_appId;
    QString m_moonlight;      // binary
    QStringList m_streamArgs;
    QStringList m_environment;
    QString m_confPath;
    QNetworkAccessManager *m_net = nullptr;
    Moonlight::Identity m_identity;

    bool m_open = false;
    int m_generation = 0; // bumped by close()
    QList<HostState> m_hosts;
    QList<Found> m_found;
    QString m_selected;
    QString m_main;
    QSet<QString> m_artRequested;
    QString m_message;
    QTimer m_refreshTimer;

    // Pairing
    QVariantMap m_pairing;
    QProcess *m_pairProcess = nullptr;
    QString m_pairUuid;
    QTimer m_pairPoll;
    QTimer m_pairTimeout;

    // Discovery
    QString m_browser;
    bool m_discovery = true;

    // Quit after the stream closed (Stop playing)
    QString m_quitHost;
    QString m_quitApp;
};
