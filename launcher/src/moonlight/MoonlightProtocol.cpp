#include "MoonlightProtocol.h"

#include <QDir>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QXmlStreamReader>

namespace Moonlight {

QString defaultConfPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/Moonlight Game Streaming Project/Moonlight.conf");
}

QList<Host> readHosts(const QString &confPath)
{
    QList<Host> out;
    QSettings s(confPath, QSettings::IniFormat);
    const int n = s.beginReadArray(QStringLiteral("hosts"));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        Host h;
        h.uuid = s.value(QStringLiteral("uuid")).toString();
        h.name = s.value(QStringLiteral("hostname")).toString();
        h.mac = s.value(QStringLiteral("mac")).toByteArray();
        h.serverCert = s.value(QStringLiteral("srvcert")).toByteArray();
        for (const char *key : {"localaddress", "manualaddress", "remoteaddress", "ipv6address"}) {
            const QString host = s.value(QLatin1String(key)).toString();
            if (host.isEmpty())
                continue;
            QString portKey = QLatin1String(key);
            portKey.replace(QStringLiteral("address"), QStringLiteral("port"));
            Address a{host, s.value(portKey, 47989).toInt()};
            if (a.port <= 0)
                a.port = 47989;
            if (!h.addresses.contains(a))
                h.addresses << a;
        }
        const int apps = s.beginReadArray(QStringLiteral("apps"));
        for (int j = 0; j < apps; ++j) {
            s.setArrayIndex(j);
            h.apps << App{s.value(QStringLiteral("id")).toInt(), s.value(QStringLiteral("name")).toString(),
                          s.value(QStringLiteral("hidden")).toBool()};
        }
        s.endArray();
        if (!h.uuid.isEmpty())
            out << h;
    }
    s.endArray();
    return out;
}

Identity readIdentity(const QString &confPath)
{
    QSettings s(confPath, QSettings::IniFormat);
    return {s.value(QStringLiteral("certificate")).toByteArray(), s.value(QStringLiteral("key")).toByteArray()};
}

bool removeHost(const QString &confPath, const QString &uuid)
{
    QSettings s(confPath, QSettings::IniFormat);
    QList<QVariantMap> keep;
    bool found = false;
    const int n = s.beginReadArray(QStringLiteral("hosts"));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        if (s.value(QStringLiteral("uuid")).toString() == uuid) {
            found = true;
            continue;
        }
        QVariantMap entry;
        for (const QString &k : s.allKeys())
            entry.insert(k, s.value(k));
        keep << entry;
    }
    s.endArray();
    if (!found)
        return false;
    s.remove(QStringLiteral("hosts"));
    for (int i = 0; i < keep.size(); ++i)
        for (auto it = keep[i].cbegin(); it != keep[i].cend(); ++it)
            s.setValue(QStringLiteral("hosts/%1/%2").arg(i + 1).arg(it.key()), it.value());
    s.setValue(QStringLiteral("hosts/size"), keep.size());
    s.sync();
    return s.status() == QSettings::NoError;
}

namespace {

// <root status_code="200" status_message="…">; true when it is 200.
bool rootOk(QXmlStreamReader &xml, QString *error)
{
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("root")) {
            const int code = xml.attributes().value(QLatin1String("status_code")).toInt();
            if (code == 200)
                return true;
            if (error)
                *error = xml.attributes().value(QLatin1String("status_message")).toString();
            return false;
        }
        xml.skipCurrentElement();
    }
    if (error)
        *error = QStringLiteral("invalid response");
    return false;
}

} // namespace

ServerInfo parseServerInfo(const QByteArray &data)
{
    ServerInfo info;
    QXmlStreamReader xml(data);
    if (!rootOk(xml, &info.error))
        return info;
    while (xml.readNextStartElement()) {
        const auto name = xml.name();
        const QString text = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
        if (name == QLatin1String("hostname"))
            info.hostname = text;
        else if (name == QLatin1String("uniqueid"))
            info.uuid = text;
        else if (name == QLatin1String("mac"))
            info.mac = text;
        else if (name == QLatin1String("LocalIP"))
            info.localIp = text;
        else if (name == QLatin1String("HttpsPort"))
            info.httpsPort = text.toInt() > 0 ? text.toInt() : 47984;
        else if (name == QLatin1String("currentgame"))
            info.currentGame = text.toInt();
        else if (name == QLatin1String("PairStatus"))
            info.paired = text == QLatin1String("1");
    }
    info.ok = !xml.hasError() || xml.error() == QXmlStreamReader::PrematureEndOfDocumentError;
    if (!info.ok)
        info.error = xml.errorString();
    return info;
}

QList<App> parseAppList(const QByteArray &data, bool *ok)
{
    QList<App> apps;
    QXmlStreamReader xml(data);
    const bool good = rootOk(xml, nullptr);
    if (ok)
        *ok = good;
    if (!good)
        return apps;
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("App")) {
            xml.skipCurrentElement();
            continue;
        }
        App app;
        while (xml.readNextStartElement()) {
            const auto name = xml.name();
            const QString text = xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            if (name == QLatin1String("AppTitle"))
                app.name = text;
            else if (name == QLatin1String("ID"))
                app.id = text.toInt();
        }
        if (app.id != 0 && !app.name.isEmpty())
            apps << app;
    }
    return apps;
}

QByteArray wakePacket(const QByteArray &mac)
{
    if (mac.size() != 6)
        return {};
    QByteArray p(6, char(0xff));
    for (int i = 0; i < 16; ++i)
        p += mac;
    return p;
}

QByteArray parseMac(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^([0-9a-fA-F]{2})([:-]?[0-9a-fA-F]{2}){5}$"));
    if (!re.match(text.trimmed()).hasMatch())
        return {};
    QString hex = text.trimmed();
    hex.remove(QLatin1Char(':')).remove(QLatin1Char('-'));
    const QByteArray mac = QByteArray::fromHex(hex.toLatin1());
    return mac == QByteArray(6, '\0') ? QByteArray() : mac;
}

QString pairFailure(const QString &line)
{
    if (line.contains(QLatin1String("Incorrect PIN")))
        return QStringLiteral("The PIN didn’t match. Try again.");
    if (line.contains(QLatin1String("already pairing")) || line.contains(QLatin1String("already in progress")))
        return QStringLiteral("The host is already pairing with another device.");
    if (line.contains(QLatin1String("MITM")))
        return QStringLiteral("The host’s certificate didn’t match.");
    if (line.contains(QLatin1String("Failed pairing")) || line.contains(QLatin1String("Failed to parse plaincert")))
        return QStringLiteral("Pairing failed. Try again.");
    if (line.contains(QLatin1String("Failed to connect")))
        return QStringLiteral("Couldn’t reach the host.");
    return {};
}

bool parseAddress(const QString &text, Address *out)
{
    QString t = text.trimmed();
    if (t.isEmpty() || t.contains(QLatin1Char(' ')) || t.contains(QLatin1Char('/')))
        return false;
    int port = 47989;
    QString host = t;
    if (t.startsWith(QLatin1Char('['))) {
        const int end = t.indexOf(QLatin1Char(']'));
        if (end < 0)
            return false;
        host = t.mid(1, end - 1);
        const QString rest = t.mid(end + 1);
        if (rest.startsWith(QLatin1Char(':')))
            port = rest.mid(1).toInt();
        else if (!rest.isEmpty())
            return false;
    } else if (t.count(QLatin1Char(':')) == 1) {
        host = t.section(QLatin1Char(':'), 0, 0);
        bool ok = false;
        port = t.section(QLatin1Char(':'), 1).toInt(&ok);
        if (!ok)
            return false;
    }
    if (host.isEmpty() || port <= 0 || port > 65535)
        return false;
    static const QRegularExpression name(QStringLiteral("^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$"));
    if (QHostAddress(host).isNull() && !name.match(host).hasMatch())
        return false;
    if (out)
        *out = {host, port};
    return true;
}

} // namespace Moonlight
