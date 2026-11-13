#include "Config.h"
#include "lifecycle/Fakes.h"
#include "lifecycle/LifecycleController.h"

#include <QSignalSpy>
#include <QTest>

// Exercises the state machine against in-memory systemd/window/bus fakes.
class TestLifecycle : public QObject
{
    Q_OBJECT

    Config config;
    FakeUnitBackend *units = nullptr;
    FakeWindowSource *windows = nullptr;
    FakeNameSource *names = nullptr;
    LifecycleController *life = nullptr;

    static constexpr auto U = UnitBackend::User;
    static constexpr auto S = UnitBackend::System;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY2(config.loadJson(R"({"version":1,"apps":[
            {"id":"stream","label":"Stream","command":["/bin/stream"],
             "ready":{"window":{"appId":"stream.app"}},"timeouts":{"start":1,"stop":1}},
            {"id":"android","label":"Android","command":["/bin/android","show"],
             "systemUnits":["container.service"],"stopCommand":["/bin/android-stop"],
             "ready":{"dbusName":"android.Session"},"exitWhen":{"windowClosed":false},
             "timeouts":{"start":1,"stop":1}},
            {"id":"plain","label":"Plain","command":["/bin/plain"],"timeouts":{"start":1,"stop":1}}
        ]})"),
                 qPrintable(config.errors().join('\n')));
    }

    void init()
    {
        units = new FakeUnitBackend(this);
        windows = new FakeWindowSource(this);
        names = new FakeNameSource(this);
        life = new LifecycleController(&config, units, windows, names, this);
    }
    void cleanup()
    {
        delete life;
        delete units;
        delete windows;
        delete names;
    }

    void happyPathWindowReadiness()
    {
        life->launch("stream");
        QCOMPARE(life->stateName(), QStringLiteral("launching"));
        QTRY_COMPARE(units->activeState(U, "htl-app-stream.service"), QStringLiteral("active"));
        // process is up, but no window yet → still launching
        QTest::qWait(20);
        QCOMPARE(life->stateName(), QStringLiteral("launching"));
        const quint32 w = windows->add("stream.app");
        QTRY_COMPARE(life->stateName(), QStringLiteral("running"));
        QCOMPARE(windows->activated, QList<quint32>{w});

        // closing the window ends the session and cleans up
        windows->remove(w);
        QTRY_COMPARE(life->stateName(), QStringLiteral("launcher"));
        QCOMPARE(units->activeState(U, "htl-app-stream.service"), QStringLiteral("inactive"));
    }

    void doubleActivationIgnored()
    {
        life->launch("stream");
        life->launch("stream");
        life->launch("plain");
        QTRY_COMPARE(units->activeState(U, "htl-app-stream.service"), QStringLiteral("active"));
        QTest::qWait(20);
        QCOMPARE(units->startedCount, 1);
        QCOMPARE(life->appId(), QStringLiteral("stream"));
    }

    void processOnlyReadinessAndCrash()
    {
        life->launch("plain");
        QTRY_COMPARE(life->stateName(), QStringLiteral("running"));
        units->setState(U, "htl-app-plain.service", "failed", 2, 11); // SIGSEGV
        QTRY_COMPARE(life->stateName(), QStringLiteral("launcher"));
    }

    void startTimeout()
    {
        life->launch("stream"); // never shows a window
        QTRY_COMPARE_WITH_TIMEOUT(life->stateName(), QStringLiteral("error"), 3000);
        QCOMPARE(life->errorKind(), QStringLiteral("start"));
        QVERIFY(life->errorDetails().contains("did not become ready"));
        QCOMPARE(units->activeState(U, "htl-app-stream.service"), QStringLiteral("inactive"));
        // Retry launches again
        life->retry();
        QCOMPARE(life->stateName(), QStringLiteral("launching"));
    }

    void exitDuringStartIsError()
    {
        life->launch("stream");
        QTRY_COMPARE(units->activeState(U, "htl-app-stream.service"), QStringLiteral("active"));
        units->setState(U, "htl-app-stream.service", "failed", 1, 3);
        QTRY_COMPARE(life->stateName(), QStringLiteral("error"));
        QVERIFY(life->errorDetails().contains("exited with status 3"));
        life->dismiss();
        QCOMPARE(life->stateName(), QStringLiteral("launcher"));
    }

    void unitFailsToStart()
    {
        units->failToStart.insert("htl-app-plain.service");
        life->launch("plain");
        QTRY_COMPARE(life->stateName(), QStringLiteral("error"));
        QCOMPARE(life->errorKind(), QStringLiteral("start"));
    }

    void homeWhileLaunchingAndRunning()
    {
        life->launch("stream");
        QTRY_COMPARE(units->activeState(U, "htl-app-stream.service"), QStringLiteral("active"));
        life->home();
        QCOMPARE(life->stateName(), QStringLiteral("closing"));
        QTRY_COMPARE(life->stateName(), QStringLiteral("launcher"));

        life->launch("stream");
        windows->add("stream.app");
        QTRY_COMPARE(life->stateName(), QStringLiteral("running"));
        life->home();
        QTRY_COMPARE(life->stateName(), QStringLiteral("launcher"));
        QCOMPARE(units->activeState(U, "htl-app-stream.service"), QStringLiteral("inactive"));
    }

    void systemUnitsAndStopCommand()
    {
        life->launch("android");
        QTRY_COMPARE(units->activeState(S, "container.service"), QStringLiteral("active"));
        QTRY_COMPARE(units->activeState(U, "htl-app-android.service"), QStringLiteral("active"));
        QCOMPARE(life->stateName(), QStringLiteral("launching"));
        names->set("android.Session", true);
        QTRY_COMPARE(life->stateName(), QStringLiteral("running"));

        // Home: stop command, app unit, system unit, verify bus name gone.
        life->home();
        QTest::qWait(50);
        QCOMPARE(life->stateName(), QStringLiteral("closing")); // session name still present
        names->set("android.Session", false);
        QTRY_COMPARE(life->stateName(), QStringLiteral("launcher"));
        QCOMPARE(units->activeState(S, "container.service"), QStringLiteral("inactive"));
        const QString log = units->log.join('\n');
        QVERIFY(log.indexOf("run /bin/android-stop") < log.indexOf("stop htl-app-android.service"));
        QVERIFY(log.indexOf("stop htl-app-android.service") < log.indexOf("stop container.service"));
    }

    void busNameLossEndsSession()
    {
        life->launch("android");
        QTRY_COMPARE(units->activeState(U, "htl-app-android.service"), QStringLiteral("active"));
        names->set("android.Session", true);
        QTRY_COMPARE(life->stateName(), QStringLiteral("running"));
        names->set("android.Session", false);
        QTRY_COMPARE(life->stateName(), QStringLiteral("launcher"));
        QCOMPARE(units->activeState(S, "container.service"), QStringLiteral("inactive"));
    }

    void stuckStopBlocksLaunches()
    {
        units->ignoreStop.insert("htl-app-plain.service");
        life->launch("plain");
        QTRY_COMPARE(life->stateName(), QStringLiteral("running"));
        life->home();
        // stop budget = stop*2 + 5 s
        QTRY_COMPARE_WITH_TIMEOUT(life->stateName(), QStringLiteral("error"), 9000);
        QCOMPARE(life->errorKind(), QStringLiteral("stop"));
        QVERIFY(life->cleanupIncomplete());

        life->dismiss();
        QCOMPARE(life->stateName(), QStringLiteral("launcher"));
        life->launch("stream"); // blocked → shows the stop error again
        QCOMPARE(life->stateName(), QStringLiteral("error"));
        QCOMPARE(life->errorKind(), QStringLiteral("stop"));
        QCOMPARE(life->appId(), QStringLiteral("plain"));
        QCOMPARE(units->startedCount, 1);

        life->stopAgain(); // SIGKILL clears it
        QTRY_COMPARE(life->stateName(), QStringLiteral("launcher"));
        QVERIFY(!life->cleanupIncomplete());
        QVERIFY(units->log.join('\n').contains("kill htl-app-plain.service 9"));
    }

    void adoptionAfterRestart()
    {
        units->setState(U, "htl-app-plain.service", "active");
        units->setState(U, "htl-app-unknown.service", "active");
        units->setState(S, "container.service", "active");
        life->startup();
        QTRY_COMPARE(life->stateName(), QStringLiteral("running"));
        QCOMPARE(life->appId(), QStringLiteral("plain"));
        QTRY_COMPARE(units->activeState(U, "htl-app-unknown.service"), QStringLiteral("inactive"));
        units->setState(U, "htl-app-plain.service", "inactive");
        QTRY_COMPARE(life->stateName(), QStringLiteral("launcher"));
        // nothing running → stray system units of apps are stopped
        QTRY_COMPARE(units->activeState(S, "container.service"), QStringLiteral("inactive"));
    }

    void preStartHookFailure()
    {
        Config c;
        QVERIFY(c.loadJson(R"({"version":1,"apps":[{"id":"h","label":"H","command":["/bin/h"],
            "hooks":{"preStart":["/bin/fail"]}}]})"));
        LifecycleController l(&c, units, windows, names);
        units->failingCommands.insert("/bin/fail");
        l.launch("h");
        QTRY_COMPARE(l.stateName(), QStringLiteral("error"));
        QVERIFY(l.errorDetails().contains("preStart"));
        QCOMPARE(units->startedCount, 0);
    }
};

QTEST_GUILESS_MAIN(TestLifecycle)
#include "tst_lifecycle.moc"
