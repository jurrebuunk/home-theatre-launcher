#pragma once

#include <QObject>

class QQuickWindow;
class LifecycleController;
class InputRouter;
class OverlayState;
class Notifier;

// Keeps the launcher surface above everything (layer-shell overlay with
// exclusive keyboard) while it is needed, and unmaps it while an app runs
// unless the in-app menu is open (then the app shows through, dimmed).
class WindowController : public QObject
{
    Q_OBJECT
public:
    WindowController(QQuickWindow *window, LifecycleController *life, InputRouter *input, OverlayState *overlay,
                     bool layerShell, QObject *parent = nullptr);

    // Separate click-through surface for notifications / volume, above everything.
    void setIndicators(QQuickWindow *window, Notifier *notifier);

    // Deliver a navigation action from a game controller as a key event.
    void injectNavigation(int action, int selectKey, int backKey);

private:
    void update();

    QQuickWindow *m_window;
    LifecycleController *m_life;
    InputRouter *m_input;
    OverlayState *m_overlay;
    bool m_layerShell;
    QQuickWindow *m_indicators = nullptr;
    Notifier *m_notifier = nullptr;
    void updateIndicators(bool raise);
};
