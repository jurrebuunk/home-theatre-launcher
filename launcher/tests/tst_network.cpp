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
};

QTEST_GUILESS_MAIN(TestNetwork)
#include "tst_network.moc"
