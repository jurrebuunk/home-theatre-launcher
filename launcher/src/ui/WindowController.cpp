#include "WindowController.h"

#include "input/InputRouter.h"
#include "lifecycle/LifecycleController.h"
#include "notify/Notifier.h"
#include "ui/OverlayState.h"

#include <QTimer>

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickWindow>

WindowController::WindowController(QQuickWindow *window, LifecycleController *life, InputRouter *input,
                                   OverlayState *overlay, bool layerShell, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_life(life)
    , m_input(input)
    , m_overlay(overlay)
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
    connect(m_overlay, &OverlayState::openChanged, this, &WindowController::update);
    update();
}

void WindowController::setIndicators(QQuickWindow *window, Notifier *notifier)
{
    m_indicators = window;
    m_notifier = notifier;
    if (!m_indicators || !m_layerShell)
        return; // windowed/dev mode: drawn inside the main window instead
    m_indicators->setFlag(Qt::WindowTransparentForInput, true);
    auto *layer = LayerShellQt::Window::get(m_indicators);
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                    | LayerShellQt::Window::AnchorLeft
                                                    | LayerShellQt::Window::AnchorRight));
    layer->setExclusiveZone(-1);
    layer->setScope(QStringLiteral("home-theatre-indicators"));
    connect(m_notifier, &Notifier::activeChanged, this, [this] { updateIndicators(false); });
}

void WindowController::updateIndicators(bool raise)
{
    if (!m_indicators || !m_layerShell)
        return;
    if (m_notifier->active()) {
        // Re-map to stay above a launcher surface that was just mapped.
        if (raise && m_indicators->isVisible())
            m_indicators->hide();
        if (!m_indicators->isVisible())
            m_indicators->show();
    } else if (m_indicators->isVisible()) {
        // Let the fade-out finish first.
        QTimer::singleShot(250, this, [this] {
            if (!m_notifier->active())
                m_indicators->hide();
        });
    }
}

void WindowController::update()
{
    const bool running = m_life->state() == LifecycleController::State::Running;
    const bool menu = running && m_overlay->isOpen();
    m_input->setNavigationEnabled(!running || menu);
    m_input->setGrabbed(menu);
    if (running && !menu) {
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
    updateIndicators(true);
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
    case InputRouter::Delete: key = Qt::Key_Delete; break;
    case InputRouter::Menu: key = Qt::Key_Menu; break;
    case InputRouter::Alt: key = Qt::Key_F2; break;
    default: return;
    }
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
    QGuiApplication::sendEvent(m_window, &press);
    QGuiApplication::sendEvent(m_window, &release);
}
