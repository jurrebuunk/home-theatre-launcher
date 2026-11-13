#include "SoundManager.h"

#include "Config.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcSound, "htl.sound")

SoundManager::SoundManager(const Config *config, bool demo, QObject *parent)
    : QObject(parent)
    , m_demo(demo)
{
    const QVariantMap c = config->sound();
    m_labels = c.value("labels").toMap();
    m_hidden = c.value("hidden").toStringList();
    m_step = c.value("step").toInt();
    m_maxVolume = c.value("maxVolume").toInt();
    if (!c.value("pactl").toString().isEmpty())
        m_pactl = c.value("pactl").toString();

    m_refresh.setSingleShot(true);
    m_refresh.setInterval(120);
    connect(&m_refresh, &QTimer::timeout, this, &SoundManager::refresh);
    m_retry.setSingleShot(true);
    m_retry.setInterval(5000);
    connect(&m_retry, &QTimer::timeout, this, &SoundManager::subscribe);

    if (demo) {
        m_available = true;
        m_loaded = true;
        m_sinks = {
            {"alsa_output.hdmi", "TV", "tv", false, {}, 60, false},
            {"bluez_output.soundbar", "Living Room Soundbar", "speaker", true, "00:11:22:33:44:04", 60, false},
            {"bluez_output.headphones", "Over-ear Headphones", "headphones", true, "00:11:22:33:44:05", 40, false},
        };
        m_default = QStringLiteral("bluez_output.soundbar");
    }
}

SoundManager::~SoundManager()
{
    if (m_subscriber) {
        m_subscriber->disconnect(this);
        m_subscriber->kill();
        m_subscriber->waitForFinished(500);
    }
}

void SoundManager::start()
{
    if (m_demo)
        return;
    subscribe();
    refresh();
}

// ---------------------------------------------------------------- state

const Sink *SoundManager::sink(const QString &name) const
{
    for (const Sink &s : m_sinks)
        if (s.name == name)
            return &s;
    return nullptr;
}

QVariantList SoundManager::outputs() const
{
    QVariantList out;
    for (const Sink &s : m_sinks) {
        const int battery = s.bluetooth && m_battery ? m_battery(s.address) : -1;
        out << QVariantMap{{"name", s.name}, {"label", s.label}, {"kind", s.kind}, {"bluetooth", s.bluetooth},
                           {"battery", battery}, {"current", s.name == m_default}};
    }
    return out;
}

QString SoundManager::currentKind() const
{
    const Sink *s = sink(m_default);
    return s ? s->kind : QString();
}

QString SoundManager::currentLabel() const
{
    const Sink *s = sink(m_default);
    return s ? s->label : QString();
}

int SoundManager::volume() const
{
    const Sink *s = sink(m_default);
    return s ? s->volume : 0;
}

bool SoundManager::muted() const
{
    const Sink *s = sink(m_default);
    return s && s->muted;
}

// ---------------------------------------------------------------- actions

void SoundManager::setOutput(const QString &name)
{
    if (name == m_default || !sink(name))
        return;
    if (m_demo) {
        apply(m_sinks, name);
        return;
    }
    run({QStringLiteral("set-default-sink"), name}, [this](int, const QByteArray &) { m_refresh.start(); });
}

void SoundManager::setVolume(int percent)
{
    percent = std::clamp(percent, 0, m_maxVolume);
    if (m_demo) {
        QList<Sink> sinks = m_sinks;
        for (Sink &s : sinks)
            if (s.name == m_default) {
                s.volume = percent;
                s.muted = false;
            }
        apply(sinks, m_default);
        return;
    }
    if (m_default.isEmpty())
        return;
    if (muted())
        run({QStringLiteral("set-sink-mute"), m_default, QStringLiteral("0")});
    run({QStringLiteral("set-sink-volume"), m_default, QString::number(percent) + QLatin1Char('%')},
        [this](int, const QByteArray &) { m_refresh.start(); });
}

void SoundManager::step(int direction)
{
    setVolume(volume() + (direction > 0 ? m_step : -m_step));
}

void SoundManager::toggleMute()
{
    if (m_demo) {
        QList<Sink> sinks = m_sinks;
        for (Sink &s : sinks)
            if (s.name == m_default)
                s.muted = !s.muted;
        apply(sinks, m_default);
        return;
    }
    if (!m_default.isEmpty())
        run({QStringLiteral("set-sink-mute"), m_default, QStringLiteral("toggle")},
            [this](int, const QByteArray &) { m_refresh.start(); });
}

// ---------------------------------------------------------------- pactl

void SoundManager::run(const QStringList &args, std::function<void(int, const QByteArray &)> done)
{
    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::SeparateChannels);
    connect(p, &QProcess::finished, this, [p, done](int code, QProcess::ExitStatus status) {
        p->deleteLater();
        if (done)
            done(status == QProcess::NormalExit ? code : -1, p->readAllStandardOutput());
    });
    connect(p, &QProcess::errorOccurred, this, [p, done, args](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        qCWarning(lcSound) << "cannot run pactl" << args;
        p->deleteLater();
        if (done)
            done(-1, {});
    });
    QTimer::singleShot(5000, p, [p] { p->kill(); });
    p->start(m_pactl, args);
}

void SoundManager::subscribe()
{
    if (m_subscriber)
        return;
    m_subscriber = new QProcess(this);
    connect(m_subscriber, &QProcess::readyReadStandardOutput, this, [this] {
        // "Event 'change' on sink #57", "Event 'change' on server #…" (default sink), …
        const QByteArray lines = m_subscriber->readAllStandardOutput();
        if (lines.contains(" sink ") || lines.contains(" server ") || lines.contains(" card "))
            m_refresh.start();
    });
    connect(m_subscriber, &QProcess::finished, this, [this] {
        m_subscriber->deleteLater();
        m_subscriber = nullptr;
        m_retry.start(); // sound server restarted or not up yet
    });
    connect(m_subscriber, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        m_subscriber->deleteLater();
        m_subscriber = nullptr;
        m_retry.start();
    });
    m_subscriber->start(m_pactl, {QStringLiteral("subscribe")});
}

void SoundManager::refresh()
{
    if (m_refreshing) {
        m_refreshAgain = true;
        return;
    }
    m_refreshing = true;
    run({QStringLiteral("get-default-sink")}, [this](int code, const QByteArray &out) {
        const QString def = code == 0 ? QString::fromUtf8(out.trimmed()) : QString();
        run({QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), QStringLiteral("sinks")},
            [this, def](int code, const QByteArray &out) {
                m_refreshing = false;
                const bool ok = code == 0;
                if (ok != m_available) {
                    m_available = ok;
                    qCInfo(lcSound) << (ok ? "sound server available" : "sound server unavailable");
                }
                apply(ok ? parseSinks(out, m_labels, m_hidden) : QList<Sink>{}, def);
                if (m_refreshAgain) {
                    m_refreshAgain = false;
                    m_refresh.start();
                }
            });
    });
}

void SoundManager::apply(const QList<Sink> &sinks, const QString &defaultSink)
{
    const Sink *before = sink(m_default);
    const int oldVolume = before ? before->volume : -1;
    const bool oldMuted = before && before->muted;
    const QString oldDefault = m_default;

    m_sinks = sinks;
    m_default = defaultSink;
    const Sink *now = sink(m_default);
    Q_EMIT changed();

    if (!m_loaded) {
        m_loaded = true; // the first state is not a change the user made
        return;
    }
    if (now && oldDefault != m_default && !oldDefault.isEmpty()) {
        qCInfo(lcSound) << "output" << now->label;
        Q_EMIT outputSwitched(now->label, now->kind);
    } else if (now && (now->volume != oldVolume || now->muted != oldMuted)) {
        Q_EMIT volumeChanged();
    }
}

// ---------------------------------------------------------------- parsing

QString SoundManager::kindFor(const QVariantMap &p, const QString &name)
{
    const QString formFactor = p.value("device.form_factor").toString();
    const QString icon = p.value("device.icon_name").toString();
    if (formFactor == QLatin1String("headphone") || formFactor == QLatin1String("headset")
        || icon.contains(QLatin1String("headphone")) || icon.contains(QLatin1String("headset")))
        return QStringLiteral("headphones");
    const QString all = (name + QLatin1Char(' ') + p.value("device.profile.name").toString() + QLatin1Char(' ')
                         + p.value("device.description").toString()).toLower();
    if (all.contains(QLatin1String("hdmi")) || all.contains(QLatin1String("displayport")) || formFactor == QLatin1String("tv"))
        return QStringLiteral("tv");
    return QStringLiteral("speaker");
}

QList<Sink> SoundManager::parseSinks(const QByteArray &json, const QVariantMap &labels, const QStringList &hidden)
{
    QList<Sink> out;
    const QJsonArray arr = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Sink s;
        s.name = o.value("name").toString();
        if (s.name.isEmpty() || s.name == QLatin1String("auto_null"))
            continue;
        if (std::any_of(hidden.cbegin(), hidden.cend(), [&](const QString &h) { return s.name.contains(h); }))
            continue;
        const QVariantMap props = o.value("properties").toObject().toVariantMap();
        s.bluetooth = props.value("device.bus").toString() == QLatin1String("bluetooth") || s.name.startsWith(QLatin1String("bluez_"));
        s.address = props.value("api.bluez5.address").toString();
        s.kind = kindFor(props, s.name);
        s.label = labels.value(s.name).toString();
        if (s.label.isEmpty())
            s.label = s.kind == QLatin1String("tv") && !s.bluetooth ? QStringLiteral("TV")
                                                                  : o.value("description").toString();
        s.muted = o.value("mute").toBool();
        const QJsonObject vol = o.value("volume").toObject();
        int sum = 0, n = 0;
        for (auto it = vol.begin(); it != vol.end(); ++it) {
            const QString pct = it.value().toObject().value("value_percent").toString();
            sum += pct.left(pct.indexOf(QLatin1Char('%'))).trimmed().toInt();
            ++n;
        }
        s.volume = n ? int(std::lround(double(sum) / n)) : 0;
        out << s;
    }
    // Several HDMI outputs would all be "TV": number them.
    int tvs = 0;
    for (const Sink &s : out)
        if (s.label == QLatin1String("TV"))
            ++tvs;
    if (tvs > 1) {
        int i = 0;
        for (Sink &s : out)
            if (s.label == QLatin1String("TV") && ++i > 1)
                s.label += QLatin1Char(' ') + QString::number(i);
    }
    return out;
}
