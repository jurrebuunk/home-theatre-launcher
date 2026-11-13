#include "Config.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QSet>

namespace {

QVariantMap merged(const QVariantMap &defaults, const QVariantMap &overrides)
{
    QVariantMap out = defaults;
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        if (it.value().isNull())
            continue;
        if (it.value().typeId() == QMetaType::QVariantMap && out.value(it.key()).typeId() == QMetaType::QVariantMap)
            out.insert(it.key(), merged(out.value(it.key()).toMap(), it.value().toMap()));
        else
            out.insert(it.key(), it.value());
    }
    return out;
}

QStringList toStringList(const QJsonValue &v)
{
    QStringList out;
    for (const auto &e : v.toArray())
        out << e.toString();
    return out;
}

bool positive(const QVariant &v) { return v.toDouble() > 0; }

} // namespace

QVariantMap AppSpec::toVariant() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), label},
        {QStringLiteral("description"), description},
        {QStringLiteral("icon"), icon},
    };
}

Config::Config(QObject *parent)
    : QObject(parent)
{
    setDefaults();
    resolveKeys();
}

void Config::setDefaults()
{
    m_colors = {
        {"background", "#1b1818"}, {"surface", "#252121"}, {"surfaceAlt", "#302b2b"},
        {"text", "#c4c1c1"}, {"muted", "#918c8c"}, {"faint", "#777272"},
        {"border", "#666262"}, {"borderStrong", "#7b7676"}, {"accent", "#8aa1a8"},
        {"focus", "#a5bbc2"}, {"success", "#8fae9a"}, {"warning", "#c2a27d"}, {"error", "#c97070"},
    };
    m_fonts = {
        {"serif", "STIX Two Text"},
        {"mono", "IBM Plex Mono"},
        {"sizes", QVariantMap{{"display", 56}, {"title", 44}, {"body", 28}, {"supporting", 24},
                              {"clock", 40}, {"value", 20}, {"code", 96}, {"idleClock", 288},
                              {"idleDate", 52}}},
    };
    m_layout = {
        {"uiScale", 1.0}, {"safeMargin", 96}, {"safeMarginVertical", 56}, {"statusRowHeight", 88},
        {"tile", QVariantMap{{"width", 480}, {"height", 360}, {"padding", 40}}},
        {"iconSize", 120}, {"statusIconSize", 32}, {"gap", 48}, {"columns", 4},
        {"alignment", "center"}, {"borderWidth", 1}, {"focusWidth", 4}, {"focusOffset", 6},
        {"buttonHeight", 80}, {"rowHeight", 80}, {"transitionMs", 150}, {"glyphStyle", "color"},
    };
    m_input = {
        {"confirmButton", "south"}, {"homeHoldMs", 1000}, {"repeatDelayMs", 400},
        {"repeatIntervalMs", 120}, {"stickDeadzone", 0.5},
        {"gamepadHomeCodes", QStringList{"BTN_MODE", "KEY_HOMEPAGE"}},
        {"keyboardHomeCodes", QStringList{"KEY_HOMEPAGE", "KEY_HOME"}},
        {"keys", QVariantMap{{"select", QStringList{"Return", "Enter", "Space", "Select"}},
                             {"back", QStringList{"Escape", "Back", "Backspace"}}}},
        {"defaultGlyphs", "xbox"},
    };
    m_bluetooth = {
        {"enable", true}, {"adapter", QString()}, {"discoveryTimeout", 30},
        {"pairingTimeout", 60}, {"agentCapability", "KeyboardDisplay"},
    };
    m_internet = {
        {"interface", "eth0"}, {"url", "https://www.google.com/generate_204"},
        {"expectStatus", 204}, {"dnsHost", QString()}, {"timeout", 5},
        {"interval", 30}, {"offlineInterval", 5},
    };
    m_clock = {{"format", "HH:mm"}, {"dateFormat", "dddd d MMMM"}, {"locale", "en_GB"}, {"timeZone", QString()}};
    m_network = {{"scanDuration", 10}};
    m_idle = {
        {"enable", true}, {"timeout", 300}, {"showStatus", true},
        {"background", QVariantMap{{"type", "color"}, {"color", QString()}, {"image", QString()},
                                   {"images", QStringList()}, {"directory", QString()}, {"interval", 30},
                                   {"fadeMs", 1500}, {"shuffle", false}, {"video", QString()}, {"muted", true},
                                   {"fill", "cover"}, {"dim", 0.25}}},
    };
}

int Config::qtKeyFromName(const QString &name)
{
    static const QMetaEnum e = QMetaEnum::fromType<Qt::Key>();
    bool ok = false;
    const int v = e.keyToValue(QByteArray("Key_" + name.toLatin1()), &ok);
    return ok ? v : -1;
}

void Config::resolveKeys()
{
    m_selectKeys.clear();
    m_backKeys.clear();
    const QVariantMap keys = m_input.value("keys").toMap();
    for (const auto &n : keys.value("select").toStringList()) {
        const int k = qtKeyFromName(n);
        if (k < 0)
            m_errors << QStringLiteral("behavior.input.keys.select: unknown key '%1'").arg(n);
        else
            m_selectKeys << k;
    }
    for (const auto &n : keys.value("back").toStringList()) {
        const int k = qtKeyFromName(n);
        if (k < 0)
            m_errors << QStringLiteral("behavior.input.keys.back: unknown key '%1'").arg(n);
        else
            m_backKeys << k;
    }
}

bool Config::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_errors << QStringLiteral("cannot read %1: %2").arg(path, f.errorString());
        return false;
    }
    return loadJson(f.readAll());
}

bool Config::loadJson(const QByteArray &json)
{
    m_errors.clear();
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
    if (!doc.isObject()) {
        m_errors << QStringLiteral("invalid JSON: %1").arg(perr.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value("version").toInt() != SchemaVersion) {
        m_errors << QStringLiteral("unsupported config version %1 (expected %2)")
                        .arg(root.value("version").toInt()).arg(SchemaVersion);
        return false;
    }

    setDefaults();
    const QVariantMap appearance = root.value("appearance").toObject().toVariantMap();
    const QVariantMap behavior = root.value("behavior").toObject().toVariantMap();
    m_colors = merged(m_colors, appearance.value("colors").toMap());
    m_fonts = merged(m_fonts, appearance.value("fonts").toMap());
    QVariantMap layout = appearance;
    layout.remove("colors");
    layout.remove("fonts");
    m_layout = merged(m_layout, layout);
    m_input = merged(m_input, behavior.value("input").toMap());
    m_bluetooth = merged(m_bluetooth, behavior.value("bluetooth").toMap());
    m_internet = merged(m_internet, behavior.value("internet").toMap());
    m_clock = merged(m_clock, behavior.value("clock").toMap());
    m_network = merged(m_network, behavior.value("network").toMap());
    m_idle = merged(m_idle, behavior.value("idle").toMap());

    // Validate appearance.
    static const QRegularExpression colorRe(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    for (auto it = m_colors.cbegin(); it != m_colors.cend(); ++it)
        if (!colorRe.match(it.value().toString()).hasMatch())
            m_errors << QStringLiteral("appearance.colors.%1: invalid colour '%2'").arg(it.key(), it.value().toString());
    const QVariantMap sizes = m_fonts.value("sizes").toMap();
    for (auto it = sizes.cbegin(); it != sizes.cend(); ++it)
        if (!positive(it.value()))
            m_errors << QStringLiteral("appearance.fonts.sizes.%1 must be > 0").arg(it.key());
    const QVariantMap tile = m_layout.value("tile").toMap();
    for (const char *k : {"width", "height"})
        if (!positive(tile.value(k)))
            m_errors << QStringLiteral("appearance.tile.%1 must be > 0").arg(k);
    for (const char *k : {"iconSize", "statusIconSize", "statusRowHeight", "buttonHeight", "rowHeight", "columns"})
        if (!positive(m_layout.value(k)))
            m_errors << QStringLiteral("appearance.%1 must be > 0").arg(k);
    for (const char *k : {"safeMargin", "safeMarginVertical", "gap", "borderWidth", "focusWidth", "focusOffset", "transitionMs"})
        if (m_layout.value(k).toDouble() < 0)
            m_errors << QStringLiteral("appearance.%1 must be >= 0").arg(k);
    const double scale = m_layout.value("uiScale").toDouble();
    if (scale <= 0 || scale > 4)
        m_errors << QStringLiteral("appearance.uiScale must be in (0, 4]");
    if (!QStringList{"left", "center"}.contains(m_layout.value("alignment").toString()))
        m_errors << QStringLiteral("appearance.alignment must be left or center");

    validateIdle();
    if (m_network.value("scanDuration").toInt() <= 0)
        m_errors << QStringLiteral("behavior.network.scanDuration must be > 0");

    // Apps.
    m_apps.clear();
    QSet<QString> ids;
    static const QRegularExpression idRe(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
    for (const auto &v : root.value("apps").toArray()) {
        const QJsonObject o = v.toObject();
        AppSpec a;
        a.id = o.value("id").toString();
        a.label = o.value("label").toString();
        a.description = o.value("description").toString();
        a.icon = o.value("icon").toString();
        a.command = toStringList(o.value("command"));
        const QJsonObject env = o.value("environment").toObject();
        for (auto it = env.begin(); it != env.end(); ++it)
            a.environment << it.key() + QLatin1Char('=') + it.value().toString();
        a.workingDirectory = o.value("workingDirectory").toString();
        a.systemUnits = toStringList(o.value("systemUnits"));
        a.stopCommand = toStringList(o.value("stopCommand"));
        const QJsonObject ready = o.value("ready").toObject();
        const QJsonObject win = ready.value("window").toObject();
        a.readyAppId = win.value("appId").toString();
        a.readyAppIdRegex = win.value("appIdRegex").toString();
        a.readyTitleRegex = win.value("titleRegex").toString();
        a.readyAnyWindow = win.value("any").toBool(false);
        a.readyDbusName = ready.value("dbusName").toString();
        const QJsonObject exitWhen = o.value("exitWhen").toObject();
        a.exitOnUnitExit = exitWhen.value("unitExit").toBool(true);
        a.exitOnWindowClosed = exitWhen.value("windowClosed").toBool(a.hasWindowRule());
        a.exitOnDbusNameLost = exitWhen.value("dbusNameLost").toBool(!a.readyDbusName.isEmpty());
        const QJsonObject timeouts = o.value("timeouts").toObject();
        a.startTimeout = timeouts.value("start").toInt(30);
        a.stopTimeout = timeouts.value("stop").toInt(10);
        const QJsonObject hooks = o.value("hooks").toObject();
        a.preStart = toStringList(hooks.value("preStart"));
        a.postStop = toStringList(hooks.value("postStop"));
        a.hookTimeout = hooks.value("timeout").toInt(15);

        const QString where = QStringLiteral("apps.%1").arg(a.id.isEmpty() ? QStringLiteral("?") : a.id);
        if (!idRe.match(a.id).hasMatch())
            m_errors << where + QStringLiteral(": invalid id");
        if (ids.contains(a.id))
            m_errors << where + QStringLiteral(": duplicate id");
        ids.insert(a.id);
        if (!a.icon.isEmpty() && !a.icon.startsWith(QLatin1Char('/')) && !a.icon.startsWith(QLatin1String("builtin:")))
            m_errors << where + QStringLiteral(": icon must be an absolute path or builtin:<name>");
        else if (a.icon.startsWith(QLatin1Char('/')) && !QFileInfo::exists(a.icon))
            m_errors << where + QStringLiteral(": icon %1 does not exist").arg(a.icon);
        else if (a.icon.startsWith(QLatin1String("builtin:"))
                 && !QFileInfo::exists(QStringLiteral(":/apps/%1.svg").arg(a.icon.mid(8))))
            m_errors << where + QStringLiteral(": unknown builtin icon '%1'").arg(a.icon);
        if (a.label.isEmpty())
            m_errors << where + QStringLiteral(": empty label");
        if (a.command.isEmpty() || a.command.first().isEmpty())
            m_errors << where + QStringLiteral(": empty command");
        else if (!a.command.first().startsWith(QLatin1Char('/')))
            m_errors << where + QStringLiteral(": command[0] must be an absolute path");
        if (a.startTimeout <= 0 || a.stopTimeout <= 0 || a.hookTimeout <= 0)
            m_errors << where + QStringLiteral(": timeouts must be > 0");
        for (const QString &re : {a.readyAppIdRegex, a.readyTitleRegex})
            if (!re.isEmpty() && !QRegularExpression(re).isValid())
                m_errors << where + QStringLiteral(": invalid regex '%1'").arg(re);
        m_apps << a;
    }

    resolveKeys();
    return m_errors.isEmpty();
}

void Config::validateIdle()
{
    static const QRegularExpression colorRe(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    if (m_idle.value("timeout").toInt() <= 0)
        m_errors << QStringLiteral("behavior.idle.timeout must be > 0");
    QVariantMap bg = m_idle.value("background").toMap();
    const QString type = bg.value("type").toString();
    if (!QStringList{"color", "image", "slideshow", "video"}.contains(type))
        m_errors << QStringLiteral("behavior.idle.background.type must be color, image, slideshow or video");
    const QString color = bg.value("color").toString();
    if (!color.isEmpty() && !colorRe.match(color).hasMatch())
        m_errors << QStringLiteral("behavior.idle.background.color: invalid colour '%1'").arg(color);
    if (!QStringList{"cover", "contain"}.contains(bg.value("fill").toString()))
        m_errors << QStringLiteral("behavior.idle.background.fill must be cover or contain");
    const double dim = bg.value("dim").toDouble();
    if (dim < 0 || dim > 1)
        m_errors << QStringLiteral("behavior.idle.background.dim must be in [0, 1]");
    if (bg.value("interval").toInt() <= 0 || bg.value("fadeMs").toInt() < 0)
        m_errors << QStringLiteral("behavior.idle.background: interval must be > 0 and fadeMs >= 0");

    auto checkImage = [this](const QString &where, const QString &path) {
        if (path.startsWith(QLatin1String("builtin:"))) {
            if (!QFileInfo::exists(QStringLiteral(":/backgrounds/%1.svg").arg(path.mid(8))))
                m_errors << QStringLiteral("%1: unknown builtin background '%2'").arg(where, path);
        } else if (!path.startsWith(QLatin1Char('/'))) {
            m_errors << QStringLiteral("%1: '%2' must be an absolute path or builtin:<name>").arg(where, path);
        }
    };
    if (type == QLatin1String("image")) {
        if (bg.value("image").toString().isEmpty())
            m_errors << QStringLiteral("behavior.idle.background.image is required for type image");
        else
            checkImage(QStringLiteral("behavior.idle.background.image"), bg.value("image").toString());
    } else if (type == QLatin1String("slideshow")) {
        const QStringList images = bg.value("images").toStringList();
        const QString dir = bg.value("directory").toString();
        if (images.isEmpty() && dir.isEmpty())
            m_errors << QStringLiteral("behavior.idle.background: a slideshow needs images or a directory");
        for (const QString &i : images)
            checkImage(QStringLiteral("behavior.idle.background.images"), i);
        if (!dir.isEmpty() && !dir.startsWith(QLatin1Char('/')))
            m_errors << QStringLiteral("behavior.idle.background.directory must be an absolute path");
    } else if (type == QLatin1String("video")) {
        const QString video = bg.value("video").toString();
        if (video.isEmpty())
            m_errors << QStringLiteral("behavior.idle.background.video is required for type video");
        else if (!video.startsWith(QLatin1Char('/')) && !video.contains(QLatin1String("://")))
            m_errors << QStringLiteral("behavior.idle.background.video must be an absolute path or URL");
    }
}

void Config::loadDemoApps()
{
    m_apps.clear();
    AppSpec a;
    a.id = "moonlight";
    a.label = "Moonlight";
    a.description = "Stream games from the PC";
    a.icon = "builtin:stream";
    a.command = {"/bin/true"};
    a.readyAnyWindow = true;
    a.exitOnWindowClosed = true;
    m_apps << a;
    AppSpec b;
    b.id = "android-tv";
    b.label = "Android TV";
    b.description = "Apps and streaming services";
    b.icon = "builtin:tv";
    b.command = {"/bin/true"};
    b.readyAnyWindow = true;
    b.exitOnWindowClosed = true;
    m_apps << b;
}

const AppSpec *Config::app(const QString &id) const
{
    for (const auto &a : m_apps)
        if (a.id == id)
            return &a;
    return nullptr;
}

QVariantList Config::appsVariant() const
{
    QVariantList out;
    for (const auto &a : m_apps)
        out << a.toVariant();
    return out;
}
