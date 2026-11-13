#include "Config.h"

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
