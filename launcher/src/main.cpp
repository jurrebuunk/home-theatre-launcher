#include "Config.h"
#include "ControlService.h"
#include "bluetooth/BluetoothManager.h"
#include "idle/IdleMonitor.h"
#include "input/InputRouter.h"
#include "lifecycle/BusNameWatcher.h"
#include "lifecycle/Fakes.h"
#include "lifecycle/LifecycleController.h"
#include "lifecycle/SystemdBackend.h"
#include "network/NetworkSettings.h"
#include "status/Clock.h"
#include "status/NetworkStatus.h"
#include "ui/IconProvider.h"
#include "ui/WindowController.h"
#include "wayland/ToplevelWatcher.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

Q_LOGGING_CATEGORY(lcMain, "htl.main")

namespace {

// Demo mode: apps "start", show a window after 1.5 s and quit after 4 s.
void wireDemo(FakeUnitBackend *units, FakeWindowSource *windows)
{
    QObject::connect(units, &UnitBackend::unitChanged, units, [units, windows](int bus, const QString &name) {
        if (bus != UnitBackend::User || units->activeState(UnitBackend::User, name) != QLatin1String("active"))
            return;
        QTimer::singleShot(1500, windows, [units, windows, name] {
            const quint32 id = windows->add(QStringLiteral("demo-") + name);
            QTimer::singleShot(4000, windows, [units, windows, id, name] {
                windows->remove(id);
                units->setState(UnitBackend::User, name, QStringLiteral("inactive"));
            });
        });
    });
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("home-theatre-launcher"));
    QGuiApplication::setDesktopFileName(QStringLiteral("home-theatre-launcher"));
    QGuiApplication::setApplicationVersion(QStringLiteral(HTL_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Home Theatre TV launcher"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOpt(QStringLiteral("config"), QStringLiteral("JSON configuration file."), QStringLiteral("path"));
    QCommandLineOption windowedOpt(QStringLiteral("windowed"), QStringLiteral("Normal window instead of a layer-shell overlay."));
    QCommandLineOption demoOpt(QStringLiteral("demo"), QStringLiteral("Fake apps, Bluetooth and network (development)."));
    QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                               QStringLiteral("Render a view (home, starting, closing, error-start, error-stop, bluetooth, "
                                              "pairing, bt-no-adapter, network-wifi, network-password, "
                                              "network-ethernet, idle, idle-image) to <file> and exit. "
                                              "Implies --demo."),
                               QStringLiteral("view:file"));
    QCommandLineOption glyphsOpt(QStringLiteral("glyphs"),
                                 QStringLiteral("Initial button prompts: xbox, playstation, keyboard or remote."),
                                 QStringLiteral("kind"));
    QCommandLineOption checkOpt(QStringLiteral("check-config"), QStringLiteral("Validate the configuration and exit."));
    parser.addOptions({configOpt, windowedOpt, demoOpt, shotOpt, glyphsOpt, checkOpt});
    parser.process(app);

    const QString shot = parser.value(shotOpt);
    const bool demo = parser.isSet(demoOpt) || !shot.isEmpty();
    const bool layerShell = !parser.isSet(windowedOpt) && shot.isEmpty()
        && QGuiApplication::platformName().startsWith(QLatin1String("wayland"));

    Config config;
    if (parser.isSet(configOpt))
        config.loadFile(parser.value(configOpt));
    if (demo && config.apps().isEmpty())
        config.loadDemoApps();
    if (parser.isSet(checkOpt)) {
        for (const QString &e : config.errors())
            qCritical().noquote() << e;
        return config.valid() ? 0 : 1;
    }
    for (const QString &e : config.errors())
        qCCritical(lcMain).noquote() << "config:" << e;

    // Backends
    UnitBackend *units = nullptr;
    WindowSource *windows = nullptr;
    NameSource *names = nullptr;
    if (demo) {
        auto *fu = new FakeUnitBackend(&app);
        auto *fw = new FakeWindowSource(&app);
        wireDemo(fu, fw);
        units = fu;
        windows = fw;
        names = new FakeNameSource(&app);
    } else {
        units = new SystemdBackend(&app);
        windows = new ToplevelWatcher(&app);
        names = new BusNameWatcher(&app);
    }

    LifecycleController life(&config, units, windows, names);
    BluetoothManager bluetooth(&config, demo);
    NetworkStatus network(&config);
    NetworkSettings networkSettings(&config, demo);
    Clock clock(&config);
    InputRouter input(&config);
    IdleMonitor idle(&config);
    ControlService control(&life, &networkSettings, &idle);
    if (parser.isSet(glyphsOpt))
        input.setLastDevice(parser.value(glyphsOpt));

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("Cfg"), &config);
    ctx->setContextProperty(QStringLiteral("Life"), &life);
    ctx->setContextProperty(QStringLiteral("Bt"), &bluetooth);
    ctx->setContextProperty(QStringLiteral("Net"), &network);
    ctx->setContextProperty(QStringLiteral("Nm"), &networkSettings);
    ctx->setContextProperty(QStringLiteral("Idle"), &idle);
    ctx->setContextProperty(QStringLiteral("Clock"), &clock);
    ctx->setContextProperty(QStringLiteral("Input"), &input);
    ctx->setContextProperty(QStringLiteral("Control"), &control);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("HomeTheatre", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

    if (!shot.isEmpty()) {
        const QString view = shot.section(QLatin1Char(':'), 0, 0);
        const QString file = shot.section(QLatin1Char(':'), 1);
        clock.setFixed(QStringLiteral("20:41"), QStringLiteral("Friday 13 November"));
        network.setDemo(QStringLiteral("online"), QString());
        if (view == QLatin1String("starting"))
            life.debugShow(QStringLiteral("launching"), QStringLiteral("moonlight"), {});
        else if (view == QLatin1String("closing"))
            life.debugShow(QStringLiteral("closing"), QStringLiteral("android-tv"), {});
        else if (view == QLatin1String("error-start"))
            life.debugShow(QStringLiteral("error"), QStringLiteral("moonlight"), QStringLiteral("start"));
        else if (view == QLatin1String("error-stop"))
            life.debugShow(QStringLiteral("error"), QStringLiteral("android-tv"), QStringLiteral("stop"));
        else if (view == QLatin1String("bluetooth") || view == QLatin1String("pairing") || view == QLatin1String("bt-no-adapter"))
            window->setProperty("debugView", view);
        if (view == QLatin1String("pairing"))
            bluetooth.demoShowPairing();
        if (view.startsWith(QLatin1String("network"))) {
            networkSettings.demoShow(view);
            window->setProperty("debugView", view);
        }
        if (view.startsWith(QLatin1String("idle"))) {
            if (view == QLatin1String("idle-image"))
                idle.setBackground({{"type", "image"}, {"image", "builtin:mountains"}, {"dim", 0.0}});
            idle.enter();
        }
        window->show();
        QTimer::singleShot(1200, &app, [window, file] {
            const QImage img = window->grabWindow();
            if (!img.save(file)) {
                qCritical() << "cannot write" << file;
                QCoreApplication::exit(1);
                return;
            }
            QCoreApplication::quit();
        });
        return app.exec();
    }

    WindowController windowController(window, &life, &input, layerShell);
    const int selectKey = config.selectKeys().value(0, Qt::Key_Return);
    const int backKey = config.backKeys().value(0, Qt::Key_Escape);
    QObject::connect(&input, &InputRouter::navigate, &windowController,
                     [&](int action) { windowController.injectNavigation(action, selectKey, backKey); });
    QObject::connect(&input, &InputRouter::homeRequested, &life, &LifecycleController::home);
    QObject::connect(&control, &ControlService::openBluetoothRequested, window,
                     [window] { QMetaObject::invokeMethod(window, "openBluetooth"); });
    QObject::connect(&control, &ControlService::openNetworkRequested, window,
                     [window] { QMetaObject::invokeMethod(window, "openNetwork"); });

    // Idle screen: only over the launcher itself; any input (window or evdev) wakes it.
    window->installEventFilter(&idle);
    QObject::connect(&input, &InputRouter::activity, &idle, &IdleMonitor::activity);
    QObject::connect(&input, &InputRouter::homeRequested, &idle, &IdleMonitor::activity);
    QObject::connect(&bluetooth, &BluetoothManager::requestChanged, &idle, &IdleMonitor::activity);
    auto inhibit = [&] { idle.setInhibited(life.state() != LifecycleController::State::Launcher); };
    QObject::connect(&life, &LifecycleController::changed, &idle, inhibit);
    inhibit();

    input.start();
    if (!demo) {
        control.registerOnBus();
        life.startup();
    }
    network.start();
    return app.exec();
}
