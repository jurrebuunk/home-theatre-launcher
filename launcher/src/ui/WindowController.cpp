#include "WindowController.h"

#include "input/InputRouter.h"
#include "lifecycle/LifecycleController.h"

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickWindow>

WindowController::WindowController(QQuickWindow *window, LifecycleController *life, InputRouter *input,
                                   bool layerShell, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_life(life)
    , m_input(input)
    , m_layerShell(layerShell)
{
    if (m_layerShell) {
        auto *layer = LayerShellQt::Window::get(m_window);
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                        | LayerShellQt::Window::AnchorLeft
                                                        | LayerShellQt::Window::AnchorRight));
        layer->setExclusiveZone(-1);
        layer->setScope(QStringLiteral("home-theatre-launcher"));
    }
    connect(m_life, &LifecycleController::changed, this, &WindowController::update);
    update();
}

void WindowController::update()
{
    const bool running = m_life->state() == LifecycleController::State::Running;
    m_input->setNavigationEnabled(!running);
    if (running) {
        if (m_window->isVisible())
            m_window->hide();
        return;
    }
    if (!m_window->isVisible()) {
        if (m_layerShell)
            m_window->show();
        else
            m_window->showFullScreen();
    }
    m_window->requestActivate();
}

void WindowController::injectNavigation(int action, int selectKey, int backKey)
{
    if (!m_window->isVisible())
        return;
    int key = 0;
    switch (action) {
    case InputRouter::Up: key = Qt::Key_Up; break;
    case InputRouter::Down: key = Qt::Key_Down; break;
    case InputRouter::Left: key = Qt::Key_Left; break;
    case InputRouter::Right: key = Qt::Key_Right; break;
    case InputRouter::Select: key = selectKey; break;
    case InputRouter::Back: key = backKey; break;
    default: return;
    }
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
    QGuiApplication::sendEvent(m_window, &press);
    QGuiApplication::sendEvent(m_window, &release);
}
