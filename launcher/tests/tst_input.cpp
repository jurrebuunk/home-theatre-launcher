#include "Config.h"
#include "bluetooth/DeviceModel.h"
#include "input/InputRouter.h"
#include "lifecycle/SystemdBackend.h"

#include <QTest>

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
    void stick()
    {
        QCOMPARE(InputRouter::axisDirection(0.1, -0.2, 0.5), int(InputRouter::None));
        QCOMPARE(InputRouter::axisDirection(-0.9, 0.2, 0.5), int(InputRouter::Left));
        QCOMPARE(InputRouter::axisDirection(0.3, 0.8, 0.5), int(InputRouter::Down));
        QCOMPARE(InputRouter::axisDirection(0.2, -0.6, 0.5), int(InputRouter::Up));
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
