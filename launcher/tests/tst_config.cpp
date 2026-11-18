#include "Config.h"

#include <QCoreApplication>
#include <QTest>

class TestConfig : public QObject
{
    Q_OBJECT

    static QByteArray wrap(const QByteArray &apps, const QByteArray &extra = {})
    {
        return "{\"version\":1,\"apps\":[" + apps + "]" + (extra.isEmpty() ? QByteArray() : "," + extra) + "}";
    }

private Q_SLOTS:
    void defaults()
    {
        Config c;
        QVERIFY(c.valid());
        QCOMPARE(c.colors().value("background").toString(), QStringLiteral("#1b1818"));
        QCOMPARE(c.layout().value("columns").toInt(), 4);
        QVERIFY(c.selectKeys().contains(Qt::Key_Return));
        QVERIFY(c.backKeys().contains(Qt::Key_Escape));
        const QVariantMap bg = c.layout().value("background").toMap();
        QCOMPARE(bg.value("type").toString(), QStringLiteral("color"));
        QCOMPARE(bg.value("panels").toMap().value("blur").toInt(), -1);
        QCOMPARE(c.layout().value("moonlight").toMap().value("cardHeight").toInt(), 480);
    }

    void customDevice()
    {
        // Any existing absolute file will do as a picture.
        const QByteArray file = QCoreApplication::applicationFilePath().toUtf8();
        Config c;
        QVERIFY2(c.loadJson(wrap({}, R"("behavior":{"input":{"devices":[{"id":"air","vendor":"1915","product":"1025",
            "invert":true,"glyphs":{"select":")" + file + R"("}}]}})")),
                 qPrintable(c.errors().join('\n')));
        const QVariantMap d = c.input().value("devices").toList().value(0).toMap();
        QCOMPARE(d.value("id").toString(), QStringLiteral("air"));
        QCOMPARE(d.value("glyphs").toMap().value("select").toString(), QString::fromUtf8(file));
    }
    void menuBackground()
    {
        Config c;
        QVERIFY2(c.loadJson(wrap({}, R"("appearance":{"background":{"type":"image","image":"/srv/wall.jpg",
            "dim":0.3,"blur":24,"panels":{"dim":0.6}},"moonlight":{"cardHeight":520,
            "boxArtBackground":{"enable":true,"blur":32}}},
            "behavior":{"idle":{"background":{"blur":12}}})")),
                 qPrintable(c.errors().join('\n')));
        const QVariantMap bg = c.layout().value("background").toMap();
        QCOMPARE(bg.value("blur").toInt(), 24);
        QCOMPARE(bg.value("panels").toMap().value("dim").toDouble(), 0.6);
        QCOMPARE(bg.value("panels").toMap().value("blur").toInt(), -1); // as home
        QCOMPARE(bg.value("fill").toString(), QStringLiteral("cover"));
        const QVariantMap art = c.layout().value("moonlight").toMap().value("boxArtBackground").toMap();
        QVERIFY(art.value("enable").toBool());
        QCOMPARE(art.value("dim").toDouble(), 0.6);
        QCOMPARE(c.idle().value("background").toMap().value("blur").toInt(), 12);
    }

    void fullApp()
    {
        Config c;
        QVERIFY2(c.loadJson(wrap(R"({"id":"moonlight","label":"Moonlight","command":["/bin/moonlight","--x"],
            "environment":{"A":"1"},"systemUnits":["foo.service"],"stopCommand":["/bin/stop"],
            "ready":{"window":{"appId":"com.moonlight_stream.Moonlight"},"dbusName":"x.y"},
            "timeouts":{"start":5,"stop":3},"hooks":{"preStart":["/bin/pre"],"timeout":2}})")),
                 qPrintable(c.errors().join('\n')));
        const AppSpec &a = c.apps().first();
        QCOMPARE(a.command, (QStringList{"/bin/moonlight", "--x"}));
        QCOMPARE(a.environment, QStringList{"A=1"});
        QCOMPARE(a.systemUnits, QStringList{"foo.service"});
        QCOMPARE(a.readyAppId, QStringLiteral("com.moonlight_stream.Moonlight"));
        QVERIFY(a.exitOnWindowClosed);
        QVERIFY(a.exitOnDbusNameLost);
        QCOMPARE(a.startTimeout, 5);
        QCOMPARE(a.unitName(), QStringLiteral("htl-app-moonlight.service"));
    }

    void overridesMerge()
    {
        Config c;
        QVERIFY(c.loadJson(wrap({}, R"("appearance":{"colors":{"accent":"#112233"},"tile":{"width":300},"uiScale":1.25})")));
        QCOMPARE(c.colors().value("accent").toString(), QStringLiteral("#112233"));
        QCOMPARE(c.colors().value("focus").toString(), QStringLiteral("#a5bbc2"));
        QCOMPARE(c.layout().value("tile").toMap().value("width").toInt(), 300);
        QCOMPARE(c.layout().value("tile").toMap().value("height").toInt(), 360);
        QCOMPARE(c.layout().value("uiScale").toDouble(), 1.25);
    }

    void rejects_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("needle");
        QTest::newRow("version") << QByteArray(R"({"version":2})") << "version";
        QTest::newRow("duplicate") << wrap(R"({"id":"a","label":"A","command":["/x"]},{"id":"a","label":"B","command":["/y"]})") << "duplicate";
        QTest::newRow("empty command") << wrap(R"({"id":"a","label":"A","command":[]})") << "empty command";
        QTest::newRow("relative command") << wrap(R"({"id":"a","label":"A","command":["moonlight"]})") << "absolute";
        QTest::newRow("bad id") << wrap(R"({"id":"A B","label":"A","command":["/x"]})") << "invalid id";
        QTest::newRow("timeout") << wrap(R"({"id":"a","label":"A","command":["/x"],"timeouts":{"start":0}})") << "timeouts";
        QTest::newRow("colour") << wrap({}, R"("appearance":{"colors":{"accent":"blue"}})") << "colour";
        QTest::newRow("tile") << wrap({}, R"("appearance":{"tile":{"width":0}})") << "tile.width";
        QTest::newRow("columns") << wrap({}, R"("appearance":{"columns":0})") << "columns";
        QTest::newRow("key") << wrap({}, R"("behavior":{"input":{"keys":{"select":["Nope"]}}})") << "unknown key";
        QTest::newRow("icon") << wrap(R"({"id":"a","label":"A","command":["/x"],"icon":"rel.svg"})") << "icon";
        QTest::newRow("idle type") << wrap({}, R"("behavior":{"idle":{"background":{"type":"gif"}}})") << "background.type";
        QTest::newRow("idle image") << wrap({}, R"("behavior":{"idle":{"background":{"type":"image"}}})") << "image is required";
        QTest::newRow("idle builtin") << wrap({}, R"("behavior":{"idle":{"background":{"type":"image","image":"builtin:nope"}}})") << "unknown builtin background";
        QTest::newRow("idle slideshow") << wrap({}, R"("behavior":{"idle":{"background":{"type":"slideshow"}}})") << "needs images";
        QTest::newRow("idle dim") << wrap({}, R"("behavior":{"idle":{"background":{"dim":2}}})") << "dim";
        QTest::newRow("idle timeout") << wrap({}, R"("behavior":{"idle":{"timeout":0}})") << "idle.timeout";
        QTest::newRow("idle blur") << wrap({}, R"("behavior":{"idle":{"background":{"blur":65}}})") << "idle.background.blur";
        QTest::newRow("menu type") << wrap({}, R"("appearance":{"background":{"type":"gif"}})") << "appearance.background.type";
        QTest::newRow("menu image") << wrap({}, R"("appearance":{"background":{"type":"image"}})") << "appearance.background.image";
        QTest::newRow("menu blur") << wrap({}, R"("appearance":{"background":{"blur":-1}})") << "appearance.background.blur";
        QTest::newRow("panel dim") << wrap({}, R"("appearance":{"background":{"panels":{"dim":3}}})") << "panels";
        QTest::newRow("card height") << wrap({}, R"("appearance":{"moonlight":{"cardHeight":20}})") << "cardHeight";
        QTest::newRow("box art dim") << wrap({}, R"("appearance":{"moonlight":{"boxArtBackground":{"dim":2}}})") << "boxArtBackground.dim";
        QTest::newRow("overlay dim") << wrap({}, R"("behavior":{"overlay":{"dim":1.5}})") << "overlay.dim";
        QTest::newRow("device id") << wrap({}, R"("behavior":{"input":{"devices":[{"id":"xbox","vendor":"1915"}]}})") << "built in";
        QTest::newRow("device match") << wrap({}, R"("behavior":{"input":{"devices":[{"id":"r"}]}})") << "match on vendor";
        QTest::newRow("device vendor") << wrap({}, R"("behavior":{"input":{"devices":[{"id":"r","vendor":"19"}]}})") << "4 hex digits";
        QTest::newRow("device base") << wrap({}, R"("behavior":{"input":{"devices":[{"id":"r","name":"x","base":"gamecube"}]}})") << "devices.r.base";
        QTest::newRow("device action") << wrap({}, R"("behavior":{"input":{"devices":[{"id":"r","name":"x","glyphs":{"jump":"/x.png"}}]}})") << "unknown action";
        QTest::newRow("device glyph") << wrap({}, R"("behavior":{"input":{"devices":[{"id":"r","name":"x","glyphs":{"select":"/nonexistent.png"}}]}})") << "does not exist";
        QTest::newRow("missing icon") << wrap(R"({"id":"a","label":"A","command":["/x"],"icon":"/nonexistent.svg"})") << "does not exist";
    }
    void rejects()
    {
        QFETCH(QByteArray, json);
        QFETCH(QString, needle);
        Config c;
        QVERIFY(!c.loadJson(json));
        QVERIFY2(c.errors().join('\n').contains(needle), qPrintable(c.errors().join('\n')));
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "tst_config.moc"
