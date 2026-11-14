#include "Config.h"
#include "moonlight/MoonlightProtocol.h"

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace Moonlight;

class TestMoonlight : public QObject
{
    Q_OBJECT

    // Two hosts as moonlight-qt writes them (QSettings arrays, raw MAC bytes).
    static void writeConf(const QString &path)
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("certificate", QByteArray("-----BEGIN CERTIFICATE-----\nabc\n-----END CERTIFICATE-----\n"));
        s.setValue("key", QByteArray("-----BEGIN PRIVATE KEY-----\ndef\n-----END PRIVATE KEY-----\n"));
        s.beginWriteArray("hosts");
        s.setArrayIndex(0);
        s.setValue("hostname", "Gaming PC");
        s.setValue("uuid", "AAAA-1");
        s.setValue("mac", QByteArray::fromHex("001122334455"));
        s.setValue("localaddress", "192.168.1.20");
        s.setValue("localport", 47989);
        s.setValue("manualaddress", "192.168.1.20");
        s.setValue("manualport", 47989);
        s.setValue("remoteaddress", "");
        s.setValue("srvcert", QByteArray("PEM"));
        s.beginWriteArray("apps");
        s.setArrayIndex(0);
        s.setValue("name", "Desktop");
        s.setValue("id", 881448767);
        s.setValue("hidden", false);
        s.setArrayIndex(1);
        s.setValue("name", "Steam Big Picture");
        s.setValue("id", 1093255277);
        s.setValue("hidden", true);
        s.endArray();
        s.setArrayIndex(1);
        s.setValue("hostname", "Office PC");
        s.setValue("uuid", "BBBB-2");
        s.setValue("manualaddress", "pc.lan");
        s.setValue("manualport", 48989);
        s.setValue("srvcert", QByteArray());
        s.endArray();
    }

private Q_SLOTS:
    void readHostsFromConf()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Moonlight.conf");
        writeConf(path);
        const QList<Host> hosts = readHosts(path);
        QCOMPARE(hosts.size(), 2);
        const Host &a = hosts.at(0);
        QCOMPARE(a.name, QStringLiteral("Gaming PC"));
        QVERIFY(a.paired());
        QCOMPARE(a.mac, QByteArray::fromHex("001122334455"));
        QCOMPARE(a.addresses.size(), 1); // local and manual are the same
        QCOMPARE(a.addresses.first().host, QStringLiteral("192.168.1.20"));
        QCOMPARE(a.apps.size(), 2);
        QCOMPARE(a.apps.at(1).name, QStringLiteral("Steam Big Picture"));
        QVERIFY(a.apps.at(1).hidden);
        const Host &b = hosts.at(1);
        QVERIFY(!b.paired());
        QCOMPARE(b.addresses.first().port, 48989);
        QVERIFY(readIdentity(path).valid());
    }

    // The format written by hand (as the VM test does) parses like moonlight's own.
    void handWrittenConf()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Moonlight.conf");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[General]\n"
                "certificate=\"@ByteArray(-----BEGIN CERTIFICATE-----\\nMIIB+/=\\n-----END CERTIFICATE-----\\n)\"\n"
                "key=\"@ByteArray(-----BEGIN PRIVATE KEY-----\\nabc\\n-----END PRIVATE KEY-----\\n)\"\n\n"
                "[hosts]\n1\\hostname=Gaming PC\n1\\uuid=F00D-CAFE\n1\\localaddress=127.0.0.1\n"
                "1\\localport=47989\n1\\srvcert=\"@ByteArray(-----BEGIN CERTIFICATE-----\\nxyz\\n-----END CERTIFICATE-----\\n)\"\n"
                "size=1\n");
        f.close();
        const QList<Host> hosts = readHosts(path);
        QCOMPARE(hosts.size(), 1);
        QCOMPARE(hosts.first().name, QStringLiteral("Gaming PC"));
        QCOMPARE(hosts.first().serverCert, QByteArray("-----BEGIN CERTIFICATE-----\nxyz\n-----END CERTIFICATE-----\n"));
        QCOMPARE(readIdentity(path).certificate, QByteArray("-----BEGIN CERTIFICATE-----\nMIIB+/=\n-----END CERTIFICATE-----\n"));
    }

    void removeHostKeepsTheRest()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Moonlight.conf");
        writeConf(path);
        QVERIFY(removeHost(path, "AAAA-1"));
        QVERIFY(!removeHost(path, "nope"));
        const QList<Host> hosts = readHosts(path);
        QCOMPARE(hosts.size(), 1);
        QCOMPARE(hosts.first().uuid, QStringLiteral("BBBB-2"));
        QCOMPARE(hosts.first().addresses.first(), (Address{"pc.lan", 48989}));
        QVERIFY(readIdentity(path).valid()); // client identity untouched
    }

    void serverInfo()
    {
        const ServerInfo i = parseServerInfo(R"(<?xml version="1.0" encoding="utf-8"?>
<root status_code="200"><hostname>Gaming PC</hostname><appversion>7.1.431.-1</appversion>
<uniqueid>AAAA-1</uniqueid><HttpsPort>47984</HttpsPort><mac>00:11:22:33:44:55</mac>
<LocalIP>192.168.1.20</LocalIP><PairStatus>1</PairStatus><currentgame>1093255277</currentgame>
<state>SUNSHINE_SERVER_BUSY</state></root>)");
        QVERIFY(i.ok);
        QCOMPARE(i.hostname, QStringLiteral("Gaming PC"));
        QCOMPARE(i.uuid, QStringLiteral("AAAA-1"));
        QCOMPARE(i.currentGame, 1093255277);
        QVERIFY(i.paired);
        QCOMPARE(i.httpsPort, 47984);

        const ServerInfo bad = parseServerInfo(R"(<root status_code="401" status_message="The client is not authorized. Certificate verification failed."/>)");
        QVERIFY(!bad.ok);
        QVERIFY(bad.error.contains("not authorized"));
        QVERIFY(!parseServerInfo("garbage").ok);
    }

    void appList()
    {
        bool ok = false;
        const QList<App> apps = parseAppList(R"(<root status_code="200">
<App><IsHdrSupported>0</IsHdrSupported><AppTitle>Desktop</AppTitle><ID>881448767</ID></App>
<App><IsHdrSupported>1</IsHdrSupported><AppTitle>Steam Big Picture</AppTitle><ID>1093255277</ID></App>
</root>)", &ok);
        QVERIFY(ok);
        QCOMPARE(apps.size(), 2);
        QCOMPARE(apps.at(1).id, 1093255277);
        QCOMPARE(apps.at(1).name, QStringLiteral("Steam Big Picture"));
        parseAppList(R"(<root status_code="503"/>)", &ok);
        QVERIFY(!ok);
    }

    void wakeOnLan()
    {
        const QByteArray mac = QByteArray::fromHex("001122334455");
        const QByteArray p = wakePacket(mac);
        QCOMPARE(p.size(), 102);
        QCOMPARE(p.left(6), QByteArray(6, char(0xff)));
        QCOMPARE(p.mid(6 + 15 * 6, 6), mac);
        QVERIFY(wakePacket("short").isEmpty());
        QCOMPARE(parseMac("00:11:22:33:44:55"), mac);
        QCOMPARE(parseMac("00-11-22-33-44-55"), mac);
        QVERIFY(parseMac("00:00:00:00:00:00").isEmpty());
        QVERIFY(parseMac("nope").isEmpty());
    }

    void addresses_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("ok");
        QTest::addColumn<QString>("host");
        QTest::addColumn<int>("port");
        QTest::newRow("ipv4") << "192.168.1.20" << true << "192.168.1.20" << 47989;
        QTest::newRow("port") << "192.168.1.20:48989" << true << "192.168.1.20" << 48989;
        QTest::newRow("name") << " gaming-pc.lan " << true << "gaming-pc.lan" << 47989;
        QTest::newRow("ipv6") << "[fe80::1]:47989" << true << "fe80::1" << 47989;
        QTest::newRow("bare ipv6") << "fe80::1" << true << "fe80::1" << 47989;
        QTest::newRow("empty") << "" << false << "" << 0;
        QTest::newRow("space") << "my pc" << false << "" << 0;
        QTest::newRow("bad port") << "pc:99999" << false << "" << 0;
        QTest::newRow("url") << "http://pc" << false << "" << 0;
    }
    void addresses()
    {
        QFETCH(QString, text);
        QFETCH(bool, ok);
        QFETCH(QString, host);
        QFETCH(int, port);
        Address a;
        QCOMPARE(parseAddress(text, &a), ok);
        if (ok) {
            QCOMPARE(a.host, host);
            QCOMPARE(a.port, port);
        }
    }

    void pairFailures()
    {
        QVERIFY(pairFailure("12:00:01 - Qt Critical: Incorrect PIN").contains("PIN"));
        QVERIFY(pairFailure("Qt Critical: Server likely already pairing").contains("already"));
        QVERIFY(pairFailure("Qt Critical: Failed pairing at stage #2").contains("failed"));
        QVERIFY(pairFailure("Qt Info: Pairing with server generation: 7").isEmpty());
    }

    void configType()
    {
        Config c;
        QVERIFY2(c.loadJson(R"({"version":1,"apps":[
            {"id":"tv","label":"TV","command":["/bin/tv"]},
            {"id":"moonlight","label":"Moonlight","type":"moonlight","command":["/bin/moonlight"],
             "streamArguments":["--fps","60"]}],
            "behavior":{"moonlight":{"discovery":false}}})"),
                 qPrintable(c.errors().join('\n')));
        QCOMPARE(c.apps().first().type, QStringLiteral("app"));
        QVERIFY(c.moonlightApp());
        QCOMPARE(c.moonlightApp()->id, QStringLiteral("moonlight"));
        QCOMPARE(c.moonlightApp()->streamArguments, (QStringList{"--fps", "60"}));
        QCOMPARE(c.moonlight().value("discovery").toBool(), false);
        QCOMPARE(c.moonlight().value("refreshSeconds").toInt(), 5);

        Config bad;
        QVERIFY(!bad.loadJson(R"({"version":1,"apps":[{"id":"a","label":"A","type":"game","command":["/x"]}]})"));
        QVERIFY(bad.errors().join('\n').contains("type"));
    }
};

QTEST_GUILESS_MAIN(TestMoonlight)
#include "tst_moonlight.moc"
