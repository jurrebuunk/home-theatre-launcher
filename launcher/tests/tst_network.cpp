#include "Config.h"
#include "network/NetworkSettings.h"
#include "network/WifiModel.h"
#include "status/NetworkStatus.h"

#include <QTest>

class TestNetwork : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultRoute()
    {
        const QByteArray table =
            "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
            "ens18\t00000000\t0102A8C0\t0003\t0\t0\t100\t00000000\t0\t0\t0\n"
            "ens18\t0002A8C0\t00000000\t0001\t0\t0\t100\t00FFFFFF\t0\t0\t0\n";
        QVERIFY(NetworkStatus::hasDefaultRoute(table, "ens18"));
        QVERIFY(!NetworkStatus::hasDefaultRoute(table, "wlan0"));
        const QByteArray noDefault =
            "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
            "ens18\t0002A8C0\t00000000\t0001\t0\t0\t100\t00FFFFFF\t0\t0\t0\n";
        QVERIFY(!NetworkStatus::hasDefaultRoute(noDefault, "ens18"));
    }

    void routeHelpers()
    {
        const QByteArray table =
            "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
            "ens18\t0002A8C0\t00000000\t0001\t0\t0\t100\t00FFFFFF\t0\t0\t0\n"
            "wlan0\t00000000\t0101A8C0\t0003\t0\t0\t600\t00000000\t0\t0\t0\n";
        QCOMPARE(NetworkSettings::defaultRouteInterface(table), QStringLiteral("wlan0"));
        QCOMPARE(NetworkSettings::defaultGateway(table, "wlan0"), QStringLiteral("192.168.1.1"));
        QCOMPARE(NetworkSettings::defaultGateway(table, "ens18"), QString());
    }

    void resolvConf()
    {
        QCOMPARE(NetworkSettings::nameservers("# x\nnameserver 192.168.1.1\nsearch lan\nnameserver  1.1.1.1\n"),
                 (QStringList{"192.168.1.1", "1.1.1.1"}));
    }

    void cidr()
    {
        QHostAddress a;
        int p = 0;
        QVERIFY(NetworkSettings::parseCidr("192.168.1.42/24", &a, &p));
        QCOMPARE(a.toString(), QStringLiteral("192.168.1.42"));
        QCOMPARE(p, 24);
        QVERIFY(NetworkSettings::parseCidr(" 10.0.0.2 ", &a, &p));
        QCOMPARE(p, 24);
        QVERIFY(!NetworkSettings::parseCidr("10.0.0/8", &a, &p));
        QVERIFY(!NetworkSettings::parseCidr("10.0.0.2/40", &a, &p));
        QVERIFY(!NetworkSettings::parseCidr("fe80::1/64", &a, &p));
    }

    void dnsWireOrder()
    {
        // NetworkManager: bytes in network order inside the uint32.
        const quint32 v = NetworkSettings::dnsWire(QHostAddress("192.168.1.1"));
        const auto *b = reinterpret_cast<const unsigned char *>(&v);
        QCOMPARE(int(b[0]), 192);
        QCOMPARE(int(b[1]), 168);
        QCOMPARE(int(b[2]), 1);
        QCOMPARE(int(b[3]), 1);
    }

    void security()
    {
        QCOMPARE(NetworkSettings::securityFor(0, 0, 0), QStringLiteral("open"));
        QCOMPARE(NetworkSettings::securityFor(1, 0, 0), QStringLiteral("wep"));
        QCOMPARE(NetworkSettings::securityFor(1, 0, 0x188), QStringLiteral("psk"));
        QCOMPARE(NetworkSettings::securityFor(1, 0, 0x588), QStringLiteral("psk")); // WPA2/WPA3 transition
        QCOMPARE(NetworkSettings::securityFor(1, 0, 0x488), QStringLiteral("sae"));
        QCOMPARE(NetworkSettings::securityFor(1, 0, 0x288), QStringLiteral("enterprise"));
        QCOMPARE(NetworkSettings::securityFor(1, 0, 0x888), QStringLiteral("owe"));
    }

    void speed()
    {
        QCOMPARE(NetworkSettings::speedText(1000), QStringLiteral("1 Gb/s"));
        QCOMPARE(NetworkSettings::speedText(2500), QStringLiteral("2.5 Gb/s"));
        QCOMPARE(NetworkSettings::speedText(100), QStringLiteral("100 Mb/s"));
        QCOMPARE(NetworkSettings::speedText(-1), QString());
    }

    void wifiModelKeepsOrder()
    {
        WifiModel m;
        m.sync({{"b", 40, "psk", false, false, {}}, {"a", 90, "psk", false, false, {}}}, true);
        QCOMPARE(m.ssidAt(0), QStringLiteral("a"));
        QCOMPARE(m.ssidAt(1), QStringLiteral("b"));
        // In-place update: b gets stronger but keeps its row; c is appended; a vanishes.
        m.sync({{"b", 95, "psk", false, false, {}}, {"c", 50, "open", false, false, {}}}, false);
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.ssidAt(0), QStringLiteral("b"));
        QCOMPARE(m.ssidAt(1), QStringLiteral("c"));
        // Resort: active first.
        m.sync({{"b", 95, "psk", false, false, {}}, {"c", 50, "open", true, true, {}}}, true);
        QCOMPARE(m.ssidAt(0), QStringLiteral("c"));
        QCOMPARE(WifiModel::bars(80), 3);
        QCOMPARE(WifiModel::bars(40), 2);
        QCOMPARE(WifiModel::bars(10), 1);
    }

    void demoConnect()
    {
        Config c;
        NetworkSettings n(&c, true);
        QVERIFY(n.needsPassword("Neighbours"));
        QVERIFY(!n.needsPassword("Living Room 5G")); // saved
        QVERIFY(!n.needsPassword("Café Guest"));     // open
        QCOMPARE(n.connectNetwork("Neighbours", "short"), QStringLiteral("The password must be 8 to 63 characters"));
        QCOMPARE(n.connectNetwork("Neighbours", "wrongpassword").isEmpty(), true);
        QTRY_COMPARE_WITH_TIMEOUT(n.connecting().value("stage").toString(), QString(), 4000);
        QVERIFY(n.networks()->find("Neighbours")->active);
        QCOMPARE(n.setIpv4("eth0", "manual", "10.0.0.5", "10.0.0.1", "1.1.1.1, 9.9.9.9"), QString());
        n.select("eth0");
        QCOMPARE(n.details().value("address").toString(), QStringLiteral("10.0.0.5/24"));
        QCOMPARE(n.setIpv4("eth0", "manual", "nope", "", ""), QStringLiteral("Address must look like 192.168.1.42/24"));
    }
};

QTEST_GUILESS_MAIN(TestNetwork)
#include "tst_network.moc"
