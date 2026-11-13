#pragma once

#include <QObject>

class QQuickWindow;
class LifecycleController;
class InputRouter;

// Keeps the launcher surface above everything (layer-shell overlay with
// exclusive keyboard) while it is needed, and unmaps it while an app runs.
class WindowController : public QObject
{
    Q_OBJECT
public:
    WindowController(QQuickWindow *window, LifecycleController *life, InputRouter *input, bool layerShell,
                     QObject *parent = nullptr);

    // Deliver a navigation action from a game controller as a key event.
    void injectNavigation(int action, int selectKey, int backKey);

private:
    void update();

    QQuickWindow *m_window;
    LifecycleController *m_life;
    InputRouter *m_input;
    bool m_layerShell;
};
