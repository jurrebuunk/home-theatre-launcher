#include "Config.h"
#include "bluetooth/DeviceModel.h"
#include "input/InputRouter.h"
#include "lifecycle/SystemdBackend.h"

#include <QTest>
#include <linux/input-event-codes.h>

class TestInput : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void classify()
    {
        QCOMPARE(InputRouter::classify(true, false, false), InputRouter::Kind::Gamepad);
        QCOMPARE(InputRouter::classify(false, true, true), InputRouter::Kind::Keyboard);
        QCOMPARE(InputRouter::classify(false, false, true), InputRouter::Kind::Remote);
        QCOMPARE(InputRouter::classify(false, false, false), InputRouter::Kind::Ignored);
    }
    void vendorGlyphs()
    {
        QCOMPARE(InputRouter::glyphsForVendor(0x045e, "xbox"), QStringLiteral("xbox"));
        QCOMPARE(InputRouter::glyphsForVendor(0x054c, "xbox"), QStringLiteral("playstation"));
        QCOMPARE(InputRouter::glyphsForVendor(0x1234, "playstation"), QStringLiteral("playstation"));
        QCOMPARE(InputRouter::glyphsForVendor(0x1234, "keyboard"), QStringLiteral("xbox"));
    }
    void customDevices()
    {
        const QVariantList devices{
            QVariantMap{{"id", "air"}, {"vendor", "1915"}, {"product", "1025"}},
            QVariantMap{{"id", "named"}, {"name", "media remote"}},
        };
        QCOMPARE(InputRouter::customDevice(devices, 0x1915, 0x1025, "XING WEI 2.4G"), QStringLiteral("air"));
        QCOMPARE(InputRouter::customDevice(devices, 0x1915, 0x1026, "x"), QString());
        QCOMPARE(InputRouter::customDevice(devices, 0x1234, 0x1, "Some Media Remote Keys"), QStringLiteral("named"));
        QCOMPARE(InputRouter::customDevice({}, 0x1915, 0x1025, "x"), QString());
    }
    void stick()
    {
        QCOMPARE(InputRouter::axisDirection(0.1, -0.2, 0.5), int(InputRouter::None));
        QCOMPARE(InputRouter::axisDirection(-0.9, 0.2, 0.5), int(InputRouter::Left));
        QCOMPARE(InputRouter::axisDirection(0.3, 0.8, 0.5), int(InputRouter::Down));
        QCOMPARE(InputRouter::axisDirection(0.2, -0.6, 0.5), int(InputRouter::Up));
    }
    void controllerUse()
    {
        QVERIFY(InputRouter::isUse(EV_KEY, BTN_SOUTH, 1, 0, 0.5));
        QVERIFY(!InputRouter::isUse(EV_KEY, BTN_SOUTH, 0, 0, 0.5)); // release
        QVERIFY(InputRouter::isUse(EV_ABS, ABS_RX, 0, -0.8, 0.5));
        QVERIFY(!InputRouter::isUse(EV_ABS, ABS_X, 0, 0.1, 0.5)); // stick noise
        QVERIFY(InputRouter::isUse(EV_ABS, ABS_HAT0Y, -1, -1, 0.5));
        QVERIFY(InputRouter::isUse(EV_ABS, ABS_RZ, 200, 0.6, 0.5));
        QVERIFY(!InputRouter::isUse(EV_ABS, ABS_Z, 10, -0.9, 0.5)); // trigger at rest
        QVERIFY(!InputRouter::isUse(EV_ABS, ABS_MT_POSITION_X, 300, 0.9, 0.5)); // touchpad, gyro, …
    }
    void overlayCombos()
    {
        const auto combos = InputRouter::parseCombos({"BTN_MODE", "BTN_SELECT+BTN_START", "NOPE", "KEY_F1+NOPE"});
        QCOMPARE(combos.size(), 2);
        QCOMPARE(combos.at(0).size(), 1);
        QCOMPARE(combos.at(1).size(), 2);
    }
    void deviceKinds()
    {
        QCOMPARE(DeviceModel::kindForIcon("input-gaming", "DualSense"), QStringLiteral("gamepad"));
        QCOMPARE(DeviceModel::kindForIcon("input-keyboard", "Media Remote"), QStringLiteral("remote"));
        QCOMPARE(DeviceModel::kindForIcon("audio-headphones", "x"), QStringLiteral("headphones"));
        QCOMPARE(DeviceModel::kindForIcon("audio-card", "Soundbar"), QStringLiteral("speaker"));
        QCOMPARE(DeviceModel::kindForIcon("", "x"), QStringLiteral("device"));
    }
    void unitPathEscaping()
    {
        QCOMPARE(SystemdBackend::unitPath("htl-app-moonlight.service"),
                 QStringLiteral("/org/freedesktop/systemd1/unit/htl_2dapp_2dmoonlight_2eservice"));
        QCOMPARE(SystemdBackend::unitPath("waydroid-container.service"),
                 QStringLiteral("/org/freedesktop/systemd1/unit/waydroid_2dcontainer_2eservice"));
    }
};

QTEST_GUILESS_MAIN(TestInput)
#include "tst_input.moc"
