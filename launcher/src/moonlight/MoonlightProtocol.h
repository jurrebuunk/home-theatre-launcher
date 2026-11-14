#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// Moonlight hosts as moonlight-qt stores them, and the GameStream / Sunshine
// HTTP API they speak. Pure functions only (unit tested).
namespace Moonlight {

struct App {
    int id = 0;
    QString name;
    bool hidden = false;
};

struct Address {
    QString host;
    int port = 47989; // HTTP port; HTTPS is announced by serverinfo
    bool operator==(const Address &o) const { return host == o.host && port == o.port; }
};

struct Host {
    QString uuid;
    QString name;
    QByteArray mac;           // 6 raw bytes, empty when unknown
    QList<Address> addresses; // local, manual, remote, IPv6 (duplicates removed)
    QByteArray serverCert;    // PEM; non-empty once paired
    QList<App> apps;          // moonlight's cached list
    bool paired() const { return !serverCert.isEmpty(); }
};

struct Identity {
    QByteArray certificate; // PEM
    QByteArray key;         // PEM
    bool valid() const { return !certificate.isEmpty() && !key.isEmpty(); }
};

struct ServerInfo {
    bool ok = false;
    QString error;
    QString hostname;
    QString uuid;
    QString mac;
    QString localIp;
    int httpsPort = 47984;
    int currentGame = 0;
    bool paired = false;
};

// ~/.config/Moonlight Game Streaming Project/Moonlight.conf
QString defaultConfPath();
QList<Host> readHosts(const QString &confPath);
Identity readIdentity(const QString &confPath);
// Removes a host (and its cached apps) from moonlight's list. Only call while
// moonlight-qt is not running: it rewrites the file on exit.
bool removeHost(const QString &confPath, const QString &uuid);

ServerInfo parseServerInfo(const QByteArray &xml);
// Apps from /applist; empty with ok=false on an error status.
QList<App> parseAppList(const QByteArray &xml, bool *ok = nullptr);

// Wake-on-LAN magic packet (6 × 0xFF, then the MAC 16 times); empty for a bad MAC.
QByteArray wakePacket(const QByteArray &mac);
QByteArray parseMac(const QString &text); // "aa:bb:cc:dd:ee:ff" → 6 bytes

// `moonlight pair` log line → user-facing failure, or empty if it is not one.
QString pairFailure(const QString &line);

// "192.168.1.20", "pc.lan:47989", "[fe80::1]:47989" → address (port defaults to 47989).
bool parseAddress(const QString &text, Address *out);

} // namespace Moonlight
