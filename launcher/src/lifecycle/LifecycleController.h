#pragma once

#include "Config.h"
#include "Sources.h"
#include "UnitBackend.h"

#include <QObject>
#include <QSet>
#include <QTimer>

#include <optional>

// launcher → launching → running → closing → launcher, plus error recovery.
// Exactly one managed app at a time; every request is ignored unless the
// controller is idle, so repeated activation is harmless.
class LifecycleController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ stateName NOTIFY changed)
    Q_PROPERTY(QString appId READ appId NOTIFY changed)
    Q_PROPERTY(QString appLabel READ appLabel NOTIFY changed)
    Q_PROPERTY(QString appIcon READ appIcon NOTIFY changed)
    Q_PROPERTY(int lastIndex READ lastIndex NOTIFY changed)
    Q_PROPERTY(QString errorKind READ errorKind NOTIFY changed)
    Q_PROPERTY(QString errorTitle READ errorTitle NOTIFY changed)
    Q_PROPERTY(QString errorDetails READ errorDetails NOTIFY changed)
    Q_PROPERTY(bool cleanupIncomplete READ cleanupIncomplete NOTIFY changed)
    // ms since the epoch when the current app became ready (0 when none)
    Q_PROPERTY(qint64 runningSince READ runningSince NOTIFY changed)
    // Moonlight stream being played (host, hostName, appId, appName); empty for plain apps.
    Q_PROPERTY(QVariantMap session READ session NOTIFY changed)

public:
    enum class State { Launcher, Launching, Running, Closing, Error };
    enum class ErrorKind { None, StartFailed, StopIncomplete };
    // How long a running app may be without its window before that counts as closed.
    static constexpr int WindowHandoffMs = 2000;

    LifecycleController(const Config *config, UnitBackend *backend, WindowSource *windows, NameSource *names,
                        QObject *parent = nullptr);

    State state() const { return m_state; }
    QString stateName() const;
    QString appId() const { return m_app.id; }
    QString appLabel() const { return m_app.label; }
    QString appIcon() const { return m_app.icon; }
    int lastIndex() const { return m_lastIndex; }
    QString errorKind() const;
    QString errorTitle() const;
    QString errorDetails() const { return m_errorDetails; }
    bool cleanupIncomplete() const { return m_cleanupIncomplete; }
    qint64 runningSince() const { return m_runningSince; }
    QVariantMap session() const { return m_session; }

    // Adopt a still-running app after a launcher restart, stop strays.
    void startup();

    Q_INVOKABLE void launch(const QString &id);
    // Launch app `id` with extra arguments (e.g. `stream <host> <app>`) as a
    // session; `session.appName` replaces the label while it runs.
    void launchSession(const QString &id, const QStringList &arguments, const QVariantMap &session);
    Q_INVOKABLE void home();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void stopAgain();
    Q_INVOKABLE void dismiss();

    // Demo/screenshot support: show a state without touching any process.
    void debugShow(const QString &state, const QString &appId, const QString &errorKind);
    void debugSession(const QVariantMap &session);

Q_SIGNALS:
    void changed();
    void homeAtLauncher();

private:
    void setState(State s);
    void setSession(const QStringList &arguments, const QVariantMap &session);
    static QString sessionFile();
    bool alive(int op) const { return op == m_op; }
    void runHook(const QStringList &argv, int op, std::function<void(bool, const QString &)> next);
    void startSystemUnits(int index, int op);
    void startApp(int op);
    void refreshUnitState();
    void evaluate();
    bool windowMatches(const WindowInfo &w) const;
    std::optional<WindowInfo> matchingWindow() const;
    void failStart(const QString &reason);
    void beginClose();
    void stopSystemUnits(int index, int op);
    void verifyStopped(int op);
    void finishClose(int op);
    void onStopDeadline(int op);
    void fetchJournal();
    void stopStraySystemUnits();

    const Config *m_config;
    UnitBackend *m_backend;
    WindowSource *m_windows;
    NameSource *m_names;

    State m_state = State::Launcher;
    AppSpec m_app;
    int m_lastIndex = 0;
    int m_op = 0;
    qint64 m_runningSince = 0;
    QVariantMap m_session;
    QStringList m_sessionArguments;

    UnitBackend::UnitState m_unit;
    bool m_unitKnown = false;
    bool m_unitStarted = false;
    QSet<quint32> m_baseline;
    bool m_sawWindow = false;
    bool m_sawName = false;

    ErrorKind m_errorKind = ErrorKind::None;
    ErrorKind m_pendingError = ErrorKind::None;
    QString m_errorReason;
    QString m_errorDetails;
    bool m_cleanupIncomplete = false;
    AppSpec m_blockedApp;

    QTimer m_startTimer;
    QTimer m_stopTimer;
    // A matching window vanished while the process lives: wait for its next one
    // (e.g. Moonlight's menu window handing over to the stream window).
    QTimer m_windowGone;
};
