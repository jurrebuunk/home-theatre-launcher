#include "MoonlightLibrary.h"

#include "Config.h"
#include "lifecycle/LifecycleController.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QProcess>
#include <QRandomGenerator>
#include <QSslConfiguration>
#include <QSslKey>
#include <QStandardPaths>
#include <QUdpSocket>
#include <QUrlQuery>
#include <QUuid>

Q_LOGGING_CATEGORY(lcMoonlight, "htl.moonlight")

namespace {

const QString Avahi = QStringLiteral("org.freedesktop.Avahi");
const QString Browser = QStringLiteral("org.freedesktop.Avahi.ServiceBrowser");

QString display(const Moonlight::Address &a)
{
    if (a.port == 47989)
        return a.host;
    return a.host.contains(QLatin1Char(':')) ? QStringLiteral("[%1]:%2").arg(a.host).arg(a.port)
                                             : QStringLiteral("%1:%2").arg(a.host).arg(a.port);
}

QString slug(const QString &name)
{
    QString s = name.toLower();
    s.replace(QLatin1Char(' '), QLatin1Char('-'));
    return s;
}

} // namespace

MoonlightLibrary::MoonlightLibrary(const Config *config, LifecycleController *life, bool demo, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_life(life)
    , m_demo(demo)
{
    if (const AppSpec *app = config->moonlightApp()) {
        m_appId = app->id;
        m_moonlight = app->command.value(0);
        m_streamArgs = app->streamArguments;
        m_environment = app->environment;
    }
    m_confPath = qEnvironmentVariable("HTL_MOONLIGHT_CONF", Moonlight::defaultConfPath());
    m_discovery = config->moonlight().value("discovery").toBool();

    m_refreshTimer.setInterval(config->moonlight().value("refreshSeconds").toInt() * 1000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &MoonlightLibrary::refreshAll);
    m_pairPoll.setInterval(1000);
    connect(&m_pairPoll, &QTimer::timeout, this, &MoonlightLibrary::checkPaired);
    m_pairTimeout.setSingleShot(true);
    m_pairTimeout.setInterval(config->moonlight().value("pairTimeout").toInt() * 1000);
    connect(&m_pairTimeout, &QTimer::timeout, this,
            [this] { finishPair(false, QStringLiteral("Pairing timed out. Try again.")); });

    // Stop playing: once the stream has closed, quit the game on the host.
    connect(m_life, &LifecycleController::changed, this, [this] {
        if (m_quitHost.isEmpty())
            return;
        if (m_life->state() == LifecycleController::State::Error) {
            m_quitHost.clear();
            return;
        }
        if (m_life->state() != LifecycleController::State::Launcher)
            return;
        const QString uuid = m_quitHost;
        const QString app = m_quitApp;
        m_quitHost.clear();
        if (m_hosts.isEmpty())
            reload();
        quitOnHost(uuid, [this, uuid, app](bool ok) {
            const HostState *h = state(uuid);
            const QString name = h ? h->conf.name : QString();
            if (ok)
                Q_EMIT stopped(app, name);
            else
                setMessage(QStringLiteral("Couldn’t quit %1 on %2").arg(app, name));
        });
    });

    if (!demo) {
        m_net = new QNetworkAccessManager(this);
        m_net->setProxy(QNetworkProxy::NoProxy);
    }
    loadMain();
    if (demo)
        demoShow({});
}

MoonlightLibrary::~MoonlightLibrary()
{
    if (m_pairProcess) {
        m_pairProcess->disconnect(this);
        m_pairProcess->kill();
        m_pairProcess->waitForFinished(1000);
    }
}

// ---------------------------------------------------------------- state

MoonlightLibrary::HostState *MoonlightLibrary::state(const QString &uuid)
{
    for (HostState &h : m_hosts)
        if (h.conf.uuid == uuid)
            return &h;
    return nullptr;
}

const MoonlightLibrary::HostState *MoonlightLibrary::state(const QString &uuid) const
{
    for (const HostState &h : m_hosts)
        if (h.conf.uuid == uuid)
            return &h;
    return nullptr;
}

QVariantList MoonlightLibrary::hosts() const
{
    QVariantList out;
    for (const HostState &h : m_hosts) {
        if (!h.conf.paired())
            continue;
        const QString address = !h.active.host.isEmpty() ? display(h.active)
            : h.conf.addresses.isEmpty()                 ? QString()
                                                         : display(h.conf.addresses.first());
        out << QVariantMap{{"uuid", h.conf.uuid},
                           {"name", h.conf.name},
                           {"address", address},
                           {"state", h.state},
                           {"main", h.conf.uuid == m_main},
                           {"currentGame", h.currentGame},
                           {"canWake", h.conf.mac.size() == 6}};
    }
    return out;
}

QVariantList MoonlightLibrary::found() const
{
    QVariantList out;
    for (const Found &f : m_found)
        out << QVariantMap{{"name", f.name.isEmpty() ? f.address.host : f.name}, {"address", display(f.address)}};
    return out;
}

QVariantMap MoonlightLibrary::current() const
{
    for (const QVariant &v : hosts())
        if (v.toMap().value("uuid").toString() == m_selected)
            return v.toMap();
    return {};
}

QString MoonlightLibrary::artPath(const QString &uuid, int appId) const
{
    const QString own = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/boxart/%1/%2.png").arg(uuid).arg(appId);
    if (QFileInfo::exists(own))
        return own;
    // moonlight-qt's own cache, if it has fetched the art already
    const QString theirs = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/Moonlight Game Streaming Project/Moonlight/boxart/%1/%2.png").arg(uuid).arg(appId);
    return QFileInfo::exists(theirs) ? theirs : QString();
}

QVariantList MoonlightLibrary::apps() const
{
    QVariantList out;
    const HostState *h = state(m_selected);
    if (!h)
        return out;
    for (const Moonlight::App &a : h->apps) {
        if (a.hidden)
            continue;
        QString art;
        if (m_demo) {
            art = QStringLiteral("image://icon/background/boxart-") + slug(a.name);
        } else if (const QString p = artPath(h->conf.uuid, a.id); !p.isEmpty()) {
            art = QUrl::fromLocalFile(p).toString();
        }
        out << QVariantMap{{"id", a.id}, {"name", a.name}, {"art", art}, {"running", h->currentGame == a.id}};
    }
    return out;
}

bool MoonlightLibrary::loadingApps() const
{
    const HostState *h = state(m_selected);
    return h && !h->appsLoaded && h->apps.isEmpty()
        && (h->state == QLatin1String("checking") || h->state == QLatin1String("online"));
}

void MoonlightLibrary::setMessage(const QString &m)
{
    if (m == m_message)
        return;
    m_message = m;
    Q_EMIT messageChanged();
}

void MoonlightLibrary::clearMessage() { setMessage({}); }

void MoonlightLibrary::setPairing(const QVariantMap &p)
{
    m_pairing = p;
    Q_EMIT pairingChanged();
}

void MoonlightLibrary::loadMain()
{
    QFile f(QStandardPaths::writableLocation(QStandardPaths::StateLocation) + QStringLiteral("/moonlight.json"));
    if (f.open(QIODevice::ReadOnly))
        m_main = QJsonDocument::fromJson(f.readAll()).object().value("mainHost").toString();
}

void MoonlightLibrary::saveMain()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/moonlight.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(QJsonObject{{"mainHost", m_main}}).toJson());
}

// ---------------------------------------------------------------- library

void MoonlightLibrary::open()
{
    m_open = true;
    if (m_demo)
        return;
    m_identity = Moonlight::readIdentity(m_confPath);
    reload();
    refreshAll();
    m_refreshTimer.start();
    startDiscovery();
}

void MoonlightLibrary::close()
{
    m_open = false;
    m_refreshTimer.stop();
    stopDiscovery();
    if (m_pairProcess || m_pairing.value("state") == QLatin1String("connecting"))
        cancelPair();
    clearMessage();
}

void MoonlightLibrary::reload()
{
    if (m_demo)
        return;
    const QList<Moonlight::Host> conf = Moonlight::readHosts(m_confPath);
    QList<HostState> next;
    for (const Moonlight::Host &c : conf) {
        HostState s;
        if (const HostState *old = state(c.uuid))
            s = *old;
        s.conf = c;
        if (!s.appsLoaded)
            s.apps = c.apps;
        // Keep moonlight's hidden flags on freshly fetched lists.
        for (Moonlight::App &a : s.apps)
            for (const Moonlight::App &ca : c.apps)
                if (ca.id == a.id)
                    a.hidden = ca.hidden;
        next << s;
    }
    m_hosts = next;

    // Known but unpaired hosts are offered for pairing.
    QList<Found> found;
    for (const Found &f : m_found) {
        const HostState *h = f.uuid.isEmpty() ? nullptr : state(f.uuid);
        if (!h || !h->conf.paired())
            found << f;
    }
    m_found = found;
    for (const HostState &h : m_hosts)
        if (!h.conf.paired() && !h.conf.addresses.isEmpty())
            addFound({h.conf.name, h.conf.addresses.first(), h.conf.uuid, {}});

    const HostState *sel = state(m_selected);
    if (!sel || !sel->conf.paired()) {
        m_selected.clear();
        if (const HostState *m = state(m_main); m && m->conf.paired())
            m_selected = m_main;
        for (const HostState &h : m_hosts)
            if (m_selected.isEmpty() && h.conf.paired())
                m_selected = h.conf.uuid;
    }
    Q_EMIT hostsChanged();
    Q_EMIT appsChanged();
}

void MoonlightLibrary::selectHost(const QString &uuid)
{
    const HostState *h = state(uuid);
    if (!h || !h->conf.paired() || uuid == m_selected)
        return;
    m_selected = uuid;
    Q_EMIT hostsChanged();
    Q_EMIT appsChanged();
    if (!m_demo)
        refresh(uuid);
}

void MoonlightLibrary::cycleHost()
{
    QStringList paired;
    for (const HostState &h : m_hosts)
        if (h.conf.paired())
            paired << h.conf.uuid;
    if (paired.size() < 2)
        return;
    selectHost(paired.at((paired.indexOf(m_selected) + 1) % paired.size()));
}

void MoonlightLibrary::refreshAll()
{
    for (const HostState &h : m_hosts)
        if (h.conf.paired())
            refresh(h.conf.uuid);
}

// Find a reachable address (HTTP serverinfo), then ask over HTTPS what runs
// and, for the selected host, which apps there are.
void MoonlightLibrary::refresh(const QString &uuid)
{
    HostState *h = state(uuid);
    if (!h || h->busy || m_demo)
        return;
    h->busy = true;
    probe(uuid, 0);
}

void MoonlightLibrary::probe(const QString &uuid, int index)
{
    HostState *h = state(uuid);
    if (!h)
        return;
    QList<Moonlight::Address> candidates;
    if (!h->active.host.isEmpty())
        candidates << h->active;
    for (const Moonlight::Address &a : h->conf.addresses)
        if (!candidates.contains(a))
            candidates << a;
    if (index >= candidates.size()) {
        h->busy = false;
        const QString st = QDateTime::currentMSecsSinceEpoch() < h->wakeUntil ? QStringLiteral("waking")
                                                                               : QStringLiteral("offline");
        if (h->state != st) {
            h->state = st;
            h->currentGame = 0;
            Q_EMIT hostsChanged();
            Q_EMIT appsChanged();
        }
        return;
    }
    const Moonlight::Address address = candidates.at(index);
    get(address, false, QStringLiteral("/serverinfo"), {}, {}, 1500, [this, uuid, index, address](bool ok, const QByteArray &body) {
        HostState *h = state(uuid);
        if (!h)
            return;
        const Moonlight::ServerInfo info = ok ? Moonlight::parseServerInfo(body) : Moonlight::ServerInfo{};
        if (!info.ok || (!info.uuid.isEmpty() && info.uuid != uuid)) {
            probe(uuid, index + 1);
            return;
        }
        h->active = address;
        h->httpsPort = info.httpsPort;
        h->wakeUntil = 0;
        const bool changed = h->state != QLatin1String("online");
        h->state = QStringLiteral("online");
        if (changed) {
            Q_EMIT hostsChanged();
            Q_EMIT appsChanged();
        }
        if (h->conf.paired() && m_identity.valid())
            fetchSecure(uuid);
        else
            h->busy = false;
    });
}

void MoonlightLibrary::fetchSecure(const QString &uuid)
{
    const HostState *h = state(uuid);
    const Moonlight::Address secure{h->active.host, h->httpsPort};
    get(secure, true, QStringLiteral("/serverinfo"), {}, h->conf.serverCert, 3000,
        [this, uuid, secure](bool ok, const QByteArray &body) {
            HostState *h = state(uuid);
            if (!h)
                return;
            const Moonlight::ServerInfo info = ok ? Moonlight::parseServerInfo(body) : Moonlight::ServerInfo{};
            if (info.ok && info.currentGame != h->currentGame) {
                h->currentGame = info.currentGame;
                Q_EMIT hostsChanged();
                Q_EMIT appsChanged();
            }
            if (!info.ok || uuid != m_selected) {
                h->busy = false;
                return;
            }
            get(secure, true, QStringLiteral("/applist"), {}, h->conf.serverCert, 5000,
                [this, uuid](bool ok, const QByteArray &body) {
                    HostState *h = state(uuid);
                    if (!h)
                        return;
                    h->busy = false;
                    bool parsed = false;
                    QList<Moonlight::App> list = ok ? Moonlight::parseAppList(body, &parsed) : QList<Moonlight::App>{};
                    if (!parsed)
                        return;
                    for (Moonlight::App &a : list)
                        for (const Moonlight::App &ca : h->conf.apps)
                            if (ca.id == a.id)
                                a.hidden = ca.hidden;
                    const bool changed = !h->appsLoaded || list.size() != h->apps.size()
                        || !std::equal(list.cbegin(), list.cend(), h->apps.cbegin(),
                                       [](const Moonlight::App &x, const Moonlight::App &y) {
                                           return x.id == y.id && x.name == y.name && x.hidden == y.hidden;
                                       });
                    h->apps = list;
                    h->appsLoaded = true;
                    if (changed)
                        Q_EMIT appsChanged();
                    fetchArt(*h);
                });
        });
}

void MoonlightLibrary::fetchArt(const HostState &h)
{
    const Moonlight::Address secure{h.active.host, h.httpsPort};
    for (const Moonlight::App &a : h.apps) {
        const QString key = h.conf.uuid + QLatin1Char('/') + QString::number(a.id);
        if (a.hidden || m_artRequested.contains(key) || !artPath(h.conf.uuid, a.id).isEmpty())
            continue;
        m_artRequested.insert(key);
        const QString uuid = h.conf.uuid;
        const int id = a.id;
        get(secure, true, QStringLiteral("/appasset"),
            {{"appid", QString::number(id)}, {"AssetType", "2"}, {"AssetIdx", "0"}}, h.conf.serverCert, 10000,
            [this, uuid, id](bool ok, const QByteArray &body) {
                if (!ok || QImage::fromData(body).isNull())
                    return;
                const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                    + QStringLiteral("/boxart/") + uuid;
                QDir().mkpath(dir);
                QFile f(dir + QStringLiteral("/%1.png").arg(id));
                if (f.open(QIODevice::WriteOnly) && f.write(body) == body.size() && uuid == m_selected)
                    Q_EMIT appsChanged();
            });
    }
}

void MoonlightLibrary::get(const Moonlight::Address &address, bool https, const QString &path,
                           const QList<QPair<QString, QString>> &query, const QByteArray &pinnedCert, int timeoutMs,
                           Reply done)
{
    if (!m_net) {
        done(false, {});
        return;
    }
    QUrl url;
    url.setScheme(https ? QStringLiteral("https") : QStringLiteral("http"));
    url.setHost(address.host);
    url.setPort(address.port);
    url.setPath(path);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("uniqueid"), QStringLiteral("0123456789ABCDEF"));
    q.addQueryItem(QStringLiteral("uuid"), QUuid::createUuid().toString(QUuid::Id128));
    for (const auto &[k, v] : query)
        q.addQueryItem(k, v);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setTransferTimeout(timeoutMs);
    if (https) {
        QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
        ssl.setLocalCertificate(QSslCertificate(m_identity.certificate));
        ssl.setPrivateKey(QSslKey(m_identity.key, QSsl::Rsa));
        // Hosts use self-signed certificates: pin the one stored at pairing instead.
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        req.setSslConfiguration(ssl);
    }
    QNetworkReply *reply = m_net->get(req);
    if (https && !pinnedCert.isEmpty()) {
        const QSslCertificate pinned(pinnedCert);
        connect(reply, &QNetworkReply::encrypted, reply, [reply, pinned] {
            if (reply->sslConfiguration().peerCertificate() != pinned) {
                qCWarning(lcMoonlight) << "certificate mismatch for" << reply->url().host();
                reply->abort();
            }
        });
    }
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        done(reply->error() == QNetworkReply::NoError, reply->readAll());
    });
}

// ---------------------------------------------------------------- actions

void MoonlightLibrary::launch(const HostState &h, const Moonlight::App &app)
{
    QString art;
    if (const QString p = artPath(h.conf.uuid, app.id); !p.isEmpty())
        art = QUrl::fromLocalFile(p).toString();
    m_life->launchSession(m_appId, QStringList{QStringLiteral("stream"), h.conf.uuid, app.name} + m_streamArgs,
                          {{"host", h.conf.uuid}, {"hostName", h.conf.name}, {"appId", app.id},
                           {"appName", app.name}, {"art", art}});
}

QString MoonlightLibrary::play(int appId)
{
    const HostState *h = state(m_selected);
    if (!h || m_life->state() != LifecycleController::State::Launcher)
        return QStringLiteral("offline");
    if (h->state != QLatin1String("online"))
        return QStringLiteral("offline");
    for (const Moonlight::App &a : h->apps) {
        if (a.id != appId)
            continue;
        if (h->currentGame != 0 && h->currentGame != appId)
            return QStringLiteral("quit");
        launch(*h, a);
        return {};
    }
    return QStringLiteral("offline");
}

void MoonlightLibrary::quitAndPlay(int appId)
{
    const QString uuid = m_selected;
    if (m_demo) {
        if (HostState *h = state(uuid))
            h->currentGame = 0;
        play(appId);
        return;
    }
    quitOnHost(uuid, [this, uuid, appId](bool ok) {
        const HostState *h = state(uuid);
        if (!h)
            return;
        if (!ok) {
            setMessage(QStringLiteral("Couldn’t quit the running app on %1").arg(h->conf.name));
            return;
        }
        if (uuid == m_selected)
            play(appId);
    });
}

void MoonlightLibrary::quitOnHost(const QString &uuid, std::function<void(bool)> done)
{
    HostState *h = state(uuid);
    if (!h || !m_identity.valid()) {
        if (!m_identity.valid())
            m_identity = Moonlight::readIdentity(m_confPath);
        h = state(uuid);
        if (!h || !m_identity.valid()) {
            done(false);
            return;
        }
    }
    auto cancel = [this, uuid, done] {
        HostState *h = state(uuid);
        if (!h || h->state != QLatin1String("online")) {
            done(false);
            return;
        }
        get({h->active.host, h->httpsPort}, true, QStringLiteral("/cancel"), {}, h->conf.serverCert, 10000,
            [this, uuid, done](bool ok, const QByteArray &body) {
                ok = ok && Moonlight::parseServerInfo(body).ok;
                if (HostState *h = state(uuid); h && ok) {
                    h->currentGame = 0;
                    Q_EMIT hostsChanged();
                    Q_EMIT appsChanged();
                }
                done(ok);
            });
    };
    if (h->state == QLatin1String("online") && !h->active.host.isEmpty()) {
        cancel();
        return;
    }
    // Not checked yet (e.g. the library was never opened): find the host first.
    h->busy = true;
    get(h->conf.addresses.value(0), false, QStringLiteral("/serverinfo"), {}, {}, 2000,
        [this, uuid, cancel](bool ok, const QByteArray &body) {
            HostState *h = state(uuid);
            if (!h)
                return;
            h->busy = false;
            const Moonlight::ServerInfo info = ok ? Moonlight::parseServerInfo(body) : Moonlight::ServerInfo{};
            if (info.ok) {
                h->active = h->conf.addresses.value(0);
                h->httpsPort = info.httpsPort;
                h->state = QStringLiteral("online");
            }
            cancel();
        });
}

void MoonlightLibrary::stopPlaying()
{
    const QVariantMap s = m_life->session();
    m_quitHost = s.value("host").toString();
    m_quitApp = s.value("appName").toString();
    m_life->home();
}

void MoonlightLibrary::wake(const QString &uuid)
{
    HostState *h = state(uuid);
    if (!h)
        return;
    if (!m_demo) {
        const QByteArray packet = Moonlight::wakePacket(h->conf.mac);
        if (packet.isEmpty()) {
            setMessage(QStringLiteral("%1 has no known network address to wake").arg(h->conf.name));
            return;
        }
        QUdpSocket socket;
        for (quint16 port : {9, 7, 47998, 47999, 48000, 48002, 48010}) {
            socket.writeDatagram(packet, QHostAddress::Broadcast, port);
            for (const Moonlight::Address &a : h->conf.addresses)
                if (QHostAddress addr(a.host); addr.protocol() == QAbstractSocket::IPv4Protocol)
                    socket.writeDatagram(packet, addr, port);
        }
        qCInfo(lcMoonlight) << "sent Wake-on-LAN to" << h->conf.name;
    }
    h->state = QStringLiteral("waking");
    h->wakeUntil = QDateTime::currentMSecsSinceEpoch() + 90 * 1000;
    Q_EMIT hostsChanged();
}

void MoonlightLibrary::setMain(const QString &uuid)
{
    if (!state(uuid) || m_main == uuid)
        return;
    m_main = uuid;
    if (!m_demo)
        saveMain();
    Q_EMIT hostsChanged();
}

void MoonlightLibrary::remove(const QString &uuid)
{
    if (m_life->state() != LifecycleController::State::Launcher)
        return;
    if (m_demo) {
        m_hosts.removeIf([&](const HostState &h) { return h.conf.uuid == uuid; });
    } else {
        if (!Moonlight::removeHost(m_confPath, uuid)) {
            setMessage(QStringLiteral("Couldn’t remove the host"));
            return;
        }
        QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/boxart/") + uuid)
            .removeRecursively();
    }
    if (m_main == uuid) {
        m_main.clear();
        if (!m_demo)
            saveMain();
    }
    if (m_selected == uuid)
        m_selected.clear();
    reload();
    if (m_demo && m_selected.isEmpty() && !m_hosts.isEmpty())
        m_selected = m_hosts.first().conf.uuid;
    Q_EMIT hostsChanged();
    Q_EMIT appsChanged();
}

QString MoonlightLibrary::streamByName(const QString &host, const QString &app)
{
    if (m_hosts.isEmpty())
        reload();
    for (const HostState &h : m_hosts) {
        if (!h.conf.paired() || (h.conf.uuid != host && h.conf.name.compare(host, Qt::CaseInsensitive) != 0))
            continue;
        for (const Moonlight::App &a : h.apps)
            if (a.name.compare(app, Qt::CaseInsensitive) == 0) {
                if (m_life->state() != LifecycleController::State::Launcher)
                    return QStringLiteral("busy");
                launch(h, a);
                return {};
            }
        return QStringLiteral("unknown app");
    }
    return QStringLiteral("unknown host");
}

// ---------------------------------------------------------------- pairing

QString MoonlightLibrary::pair(const QString &text)
{
    Moonlight::Address address;
    if (!Moonlight::parseAddress(text, &address))
        return QStringLiteral("Enter an IP address or host name");
    if (m_pairProcess || !m_pairing.isEmpty())
        cancelPair();
    QString name = address.host;
    for (const Found &f : m_found)
        if (f.address == address && !f.name.isEmpty())
            name = f.name;
    const QString pin = QStringLiteral("%1").arg(QRandomGenerator::global()->bounded(10000), 4, 10, QLatin1Char('0'));
    const QString key = display(address);
    setPairing({{"address", key}, {"name", name}, {"pin", pin}, {"state", "connecting"}, {"error", ""}});
    if (m_demo)
        return {};

    get(address, false, QStringLiteral("/serverinfo"), {}, {}, 4000, [this, address, key, pin](bool ok, const QByteArray &body) {
        if (m_pairing.value("address").toString() != key || m_pairing.value("state") != QLatin1String("connecting"))
            return; // cancelled meanwhile
        const Moonlight::ServerInfo info = ok ? Moonlight::parseServerInfo(body) : Moonlight::ServerInfo{};
        if (!info.ok) {
            finishPair(false, QStringLiteral("Couldn’t reach %1").arg(key));
            return;
        }
        QVariantMap p = m_pairing;
        if (!info.hostname.isEmpty())
            p.insert("name", info.hostname);
        if (const HostState *h = state(info.uuid); h && h->conf.paired()) {
            setPairing(p);
            finishPair(false, QStringLiteral("%1 is already paired").arg(h->conf.name));
            return;
        }
        m_pairUuid = info.uuid;
        p.insert("state", "waiting");
        setPairing(p);

        m_pairProcess = new QProcess(this);
        m_pairProcess->setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (const QString &kv : m_environment)
            env.insert(kv.section(QLatin1Char('='), 0, 0), kv.section(QLatin1Char('='), 1));
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen")); // its own dialogs stay hidden
        m_pairProcess->setProcessEnvironment(env);
        connect(m_pairProcess, &QProcess::readyRead, this, [this] {
            while (m_pairProcess && m_pairProcess->canReadLine()) {
                const QString line = QString::fromUtf8(m_pairProcess->readLine()).trimmed();
                qCDebug(lcMoonlight) << "pair:" << line;
                if (const QString failure = Moonlight::pairFailure(line); !failure.isEmpty()) {
                    finishPair(false, failure);
                    return;
                }
            }
        });
        connect(m_pairProcess, &QProcess::finished, this, [this] {
            checkPaired();
            if (m_pairing.value("state") == QLatin1String("waiting"))
                finishPair(false, QStringLiteral("Pairing failed. Try again."));
        });
        m_pairProcess->start(m_moonlight, {QStringLiteral("pair"), key, QStringLiteral("--pin"), pin});
        m_pairPoll.start();
        m_pairTimeout.start();
    });
    return {};
}

void MoonlightLibrary::checkPaired()
{
    if (m_pairUuid.isEmpty() || m_pairing.value("state") != QLatin1String("waiting"))
        return;
    for (const Moonlight::Host &h : Moonlight::readHosts(m_confPath))
        if (h.uuid == m_pairUuid && h.paired()) {
            finishPair(true, {});
            return;
        }
}

void MoonlightLibrary::finishPair(bool ok, const QString &error)
{
    m_pairPoll.stop();
    m_pairTimeout.stop();
    if (QProcess *p = m_pairProcess) {
        m_pairProcess = nullptr;
        p->disconnect(this);
        connect(p, &QProcess::finished, p, &QObject::deleteLater);
        if (p->state() == QProcess::NotRunning) {
            p->deleteLater();
        } else {
            p->terminate(); // moonlight saves its hosts on the way out
            QTimer::singleShot(3000, p, [p] { p->kill(); });
        }
    }
    QVariantMap p = m_pairing;
    p.insert("state", ok ? "done" : "failed");
    p.insert("error", error);
    setPairing(p);
    if (!ok)
        return;
    const QString uuid = m_pairUuid;
    m_pairUuid.clear();
    reload();
    if (m_main.isEmpty() || !state(m_main))
        setMain(uuid);
    selectHost(uuid);
    refresh(uuid);
}

void MoonlightLibrary::cancelPair()
{
    m_pairUuid.clear();
    if (m_pairProcess)
        finishPair(false, {});
    m_pairPoll.stop();
    m_pairTimeout.stop();
    setPairing({});
}

// ---------------------------------------------------------------- discovery (Avahi)

void MoonlightLibrary::startDiscovery()
{
    if (!m_discovery || m_demo || !m_browser.isEmpty())
        return;
    QDBusMessage m = QDBusMessage::createMethodCall(Avahi, QStringLiteral("/"), QStringLiteral("org.freedesktop.Avahi.Server2"),
                                                    QStringLiteral("ServiceBrowserPrepare"));
    m << -1 << 0 << QStringLiteral("_nvstream._tcp") << QString() << 0u; // any interface, IPv4
    auto *w = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(m, 3000), this);
    m_browser = QStringLiteral("pending");
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w] {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            qCInfo(lcMoonlight) << "no discovery (Avahi):" << reply.error().message();
            m_browser.clear();
            return;
        }
        const QString path = reply.value().path();
        auto bus = QDBusConnection::systemBus();
        if (!m_open) {
            bus.asyncCall(QDBusMessage::createMethodCall(Avahi, path, Browser, QStringLiteral("Free")));
            m_browser.clear();
            return;
        }
        m_browser = path;
        bus.connect(Avahi, path, Browser, QStringLiteral("ItemNew"), this,
                    SLOT(onAvahiItemNew(int, int, QString, QString, QString, uint)));
        bus.connect(Avahi, path, Browser, QStringLiteral("ItemRemove"), this,
                    SLOT(onAvahiItemRemove(int, int, QString, QString, QString, uint)));
        bus.asyncCall(QDBusMessage::createMethodCall(Avahi, path, Browser, QStringLiteral("Start")));
    });
}

void MoonlightLibrary::stopDiscovery()
{
    if (m_browser.isEmpty() || m_browser == QLatin1String("pending"))
        return;
    auto bus = QDBusConnection::systemBus();
    bus.disconnect(Avahi, m_browser, Browser, QStringLiteral("ItemNew"), this,
                   SLOT(onAvahiItemNew(int, int, QString, QString, QString, uint)));
    bus.disconnect(Avahi, m_browser, Browser, QStringLiteral("ItemRemove"), this,
                   SLOT(onAvahiItemRemove(int, int, QString, QString, QString, uint)));
    bus.asyncCall(QDBusMessage::createMethodCall(Avahi, m_browser, Browser, QStringLiteral("Free")));
    m_browser.clear();
}

void MoonlightLibrary::onAvahiItemNew(int interface, int protocol, const QString &name, const QString &type,
                                      const QString &domain, uint)
{
    QDBusMessage m = QDBusMessage::createMethodCall(Avahi, QStringLiteral("/"), QStringLiteral("org.freedesktop.Avahi.Server"),
                                                    QStringLiteral("ResolveService"));
    m << interface << protocol << name << type << domain << 0 << 0u;
    auto *w = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(m, 5000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w, name] {
        w->deleteLater();
        const QDBusMessage reply = w->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 9)
            return;
        const Moonlight::Address address{reply.arguments().at(7).toString(), int(reply.arguments().at(8).toUInt())};
        get(address, false, QStringLiteral("/serverinfo"), {}, {}, 3000, [this, address, name](bool ok, const QByteArray &body) {
            const Moonlight::ServerInfo info = ok ? Moonlight::parseServerInfo(body) : Moonlight::ServerInfo{};
            if (!info.ok)
                return;
            if (const HostState *h = state(info.uuid); h && h->conf.paired())
                return; // already in the list
            addFound({info.hostname.isEmpty() ? name : info.hostname, address, info.uuid, name});
            Q_EMIT hostsChanged();
        });
    });
}

void MoonlightLibrary::onAvahiItemRemove(int, int, const QString &name, const QString &, const QString &, uint)
{
    if (m_found.removeIf([&](const Found &f) { return f.avahiName == name; }) > 0)
        Q_EMIT hostsChanged();
}

void MoonlightLibrary::addFound(const Found &f)
{
    for (Found &e : m_found) {
        if ((!f.uuid.isEmpty() && e.uuid == f.uuid) || e.address == f.address) {
            e.name = f.name.isEmpty() ? e.name : f.name;
            e.address = f.address;
            e.uuid = f.uuid.isEmpty() ? e.uuid : f.uuid;
            e.avahiName = f.avahiName.isEmpty() ? e.avahiName : f.avahiName;
            return;
        }
    }
    m_found << f;
}

// ---------------------------------------------------------------- demo

void MoonlightLibrary::demoShow(const QString &view)
{
    if (m_hosts.isEmpty()) {
        auto host = [](const QString &uuid, const QString &name, const QString &ip) {
            HostState h;
            h.conf.uuid = uuid;
            h.conf.name = name;
            h.conf.addresses = {{ip, 47989}};
            h.conf.serverCert = "demo";
            h.conf.mac = QByteArray::fromHex("0011223344aa");
            h.active = h.conf.addresses.first();
            h.appsLoaded = true;
            return h;
        };
        HostState gaming = host("demo-gaming", "Gaming PC", "192.168.1.20");
        gaming.state = "online";
        int id = 1;
        for (const char *n : {"Desktop", "Virtual Desktop", "Steam Big Picture", "Ember Coast", "Night Circuit",
                              "Hollow Peaks", "Tide Runner", "Glass Orbit", "Salt Signal"})
            gaming.apps << Moonlight::App{id++, QString::fromLatin1(n), false};
        gaming.currentGame = 3;
        HostState living = host("demo-living", "Living Room PC", "192.168.1.31");
        living.state = "offline";
        living.apps = {{1, "Desktop", false}, {2, "Steam Big Picture", false}};
        m_hosts = {gaming, living};
        m_found = {{"Office PC", {"192.168.1.60", 47989}, "demo-office", {}}};
        m_main = "demo-gaming";
        m_selected = "demo-gaming";
    }
    if (view == QLatin1String("moonlight-pair"))
        setPairing({{"address", "192.168.1.60"}, {"name", "Office PC"}, {"pin", "4821"}, {"state", "waiting"}, {"error", ""}});
    Q_EMIT hostsChanged();
    Q_EMIT appsChanged();
}
