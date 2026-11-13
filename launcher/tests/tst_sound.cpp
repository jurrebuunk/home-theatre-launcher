#include "Config.h"
#include "audio/SoundManager.h"
#include "notify/Notifier.h"

#include <QSignalSpy>
#include <QTest>

class TestSound : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void parsesSinks()
    {
        const QByteArray json = R"J([
          {"index":1,"name":"alsa_output.pci-0000_00_03.0.hdmi-stereo","description":"Built-in Audio Digital Stereo (HDMI)",
           "mute":false,"volume":{"front-left":{"value":39321,"value_percent":"60%"},"front-right":{"value":39321,"value_percent":"60%"}},
           "properties":{"device.bus":"pci","device.profile.name":"hdmi-stereo"}},
          {"index":2,"name":"bluez_output.AA_BB.1","description":"Living Room Soundbar","mute":true,
           "volume":{"mono":{"value":0,"value_percent":"45%"}},
           "properties":{"device.bus":"bluetooth","api.bluez5.address":"AA:BB:CC:DD:EE:FF","device.form_factor":"speaker"}},
          {"index":3,"name":"bluez_output.CC_DD.1","description":"Over-ear Headphones","mute":false,
           "volume":{"front-left":{"value_percent":"40%"},"front-right":{"value_percent":"50%"}},
           "properties":{"device.bus":"bluetooth","device.form_factor":"headphone"}},
          {"index":4,"name":"auto_null","description":"Dummy Output","properties":{}},
          {"index":5,"name":"alsa_output.usb-loopback","description":"Loopback","properties":{}}
        ])J";
        const QList<Sink> s = SoundManager::parseSinks(json, {{"bluez_output.CC_DD.1", "Cans"}}, {"loopback"});
        QCOMPARE(s.size(), 3);
        QCOMPARE(s[0].label, QStringLiteral("TV"));
        QCOMPARE(s[0].kind, QStringLiteral("tv"));
        QCOMPARE(s[0].volume, 60);
        QCOMPARE(s[1].kind, QStringLiteral("speaker"));
        QVERIFY(s[1].bluetooth);
        QVERIFY(s[1].muted);
        QCOMPARE(s[1].address, QStringLiteral("AA:BB:CC:DD:EE:FF"));
        QCOMPARE(s[2].kind, QStringLiteral("headphones"));
        QCOMPARE(s[2].label, QStringLiteral("Cans"));
        QCOMPARE(s[2].volume, 45);
    }

    void demoSwitchAndVolume()
    {
        Config c;
        SoundManager m(&c, true);
        QSignalSpy switched(&m, &SoundManager::outputSwitched);
        QSignalSpy volume(&m, &SoundManager::volumeChanged);
        QCOMPARE(m.currentKind(), QStringLiteral("speaker"));
        m.step(1);
        QCOMPARE(m.volume(), 65);
        QCOMPARE(volume.count(), 1);
        m.setVolume(500);
        QCOMPARE(m.volume(), 100);
        m.toggleMute();
        QVERIFY(m.muted());
        m.setOutput("alsa_output.hdmi");
        QCOMPARE(switched.count(), 1);
        QCOMPARE(switched.first().first().toString(), QStringLiteral("TV"));
    }

    void batteryRules()
    {
        QCOMPARE(Notifier::batteryLevel(80, 20, 10), QString());
        QCOMPARE(Notifier::batteryLevel(20, 20, 10), QStringLiteral("low"));
        QCOMPARE(Notifier::batteryLevel(10, 20, 10), QStringLiteral("critical"));
        QCOMPARE(Notifier::batteryLevel(-1, 20, 10), QString());

        Config c;
        Notifier n(&c);
        n.deviceBattery("/d", "Pad", "gamepad", 50);
        QCOMPARE(n.items().size(), 0);
        n.deviceBattery("/d", "Pad", "gamepad", 18);
        QCOMPARE(n.items().size(), 1);
        QCOMPARE(n.items().first().toMap().value("tone").toString(), QStringLiteral("warning"));
        n.deviceBattery("/d", "Pad", "gamepad", 17); // low is shown once
        QCOMPARE(n.items().size(), 1);
        n.deviceBattery("/d", "Pad", "gamepad", 8); // critical replaces it
        QCOMPARE(n.items().size(), 1);
        QCOMPARE(n.items().first().toMap().value("tone").toString(), QStringLiteral("critical"));
        n.deviceBattery("/h", "Headset", "headphones", 5); // not a controller
        QCOMPARE(n.items().size(), 1);
    }

    void maxThreeNewestFirst()
    {
        Config c;
        Notifier n(&c);
        QSignalSpy active(&n, &Notifier::activeChanged);
        n.wifi("A", true);
        n.ethernet(false);
        n.outputSwitched("TV", "tv");
        n.deviceConnection("/x", "Pad", "gamepad", true, 80);
        QCOMPARE(n.titles(), (QStringList{"Pad", "TV", "Ethernet"}));
        QCOMPARE(active.count(), 1);
        QVERIFY(n.active());
    }
};

QTEST_GUILESS_MAIN(TestSound)
#include "tst_sound.moc"
