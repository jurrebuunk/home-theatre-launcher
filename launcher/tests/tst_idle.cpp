#include "Config.h"
#include "idle/IdleMonitor.h"

#include <QKeyEvent>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestIdle : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void timesOutAndWakes()
    {
        Config c;
        IdleMonitor idle(&c);
        idle.setTimeoutMs(50);
        idle.activity(); // restart with the short timeout
        QTRY_VERIFY_WITH_TIMEOUT(idle.idle(), 2000);

        // The waking key press and its release are swallowed.
        QObject target;
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
        QVERIFY(idle.eventFilter(&target, &press));
        QVERIFY(!idle.idle());
        QVERIFY(idle.eventFilter(&target, &release));
        // Later keys pass through and keep it awake.
        QVERIFY(!idle.eventFilter(&target, &press));
        QVERIFY(!idle.eventFilter(&target, &release));
    }

    void inhibitedWhileAppRuns()
    {
        Config c;
        IdleMonitor idle(&c);
        idle.setTimeoutMs(30);
        idle.setInhibited(true);
        idle.enter();
        QVERIFY(!idle.idle());
        QTest::qWait(120);
        QVERIFY(!idle.idle());
        idle.setInhibited(false);
        QTRY_VERIFY_WITH_TIMEOUT(idle.idle(), 2000);
        idle.setInhibited(true); // an app launched: idle ends
        QVERIFY(!idle.idle());
    }

    void activityResets()
    {
        Config c;
        IdleMonitor idle(&c);
        idle.setTimeoutMs(150);
        idle.activity();
        for (int i = 0; i < 5; ++i) {
            QTest::qWait(60);
            idle.activity();
        }
        QVERIFY(!idle.idle());
    }

    void resolvesImages()
    {
        QTemporaryDir dir;
        for (const char *n : {"b.jpg", "a.png", "notes.txt", "c.webp"}) {
            QFile f(dir.filePath(n));
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        const QStringList urls = IdleMonitor::resolveImages(
            {{"type", "slideshow"}, {"images", QStringList{"builtin:mountains"}}, {"directory", dir.path()}});
        QCOMPARE(urls.size(), 4);
        QCOMPARE(urls.at(0), QStringLiteral("image://icon/background/mountains"));
        QVERIFY(urls.at(1).endsWith("/a.png"));
        QVERIFY(urls.at(2).endsWith("/b.jpg"));
        QVERIFY(urls.at(3).endsWith("/c.webp"));
        QCOMPARE(IdleMonitor::resolveImages({{"type", "color"}}), QStringList());
    }

    void disabled()
    {
        Config c;
        QVERIFY(c.loadJson(R"({"version":1,"apps":[],"behavior":{"idle":{"enable":false,"timeout":1}}})"));
        IdleMonitor idle(&c);
        QVERIFY(!idle.enabled());
        QTest::qWait(1200);
        QVERIFY(!idle.idle());
    }
};

QTEST_GUILESS_MAIN(TestIdle)
#include "tst_idle.moc"
