#include "OverlayState.h"

#include "lifecycle/LifecycleController.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcOverlay, "htl.overlay")

OverlayState::OverlayState(LifecycleController *life, bool enabled, double dim, QObject *parent)
    : QObject(parent)
    , m_life(life)
    , m_enabled(enabled)
    , m_dim(dim)
{
    // Leaving "running" (app exited, Home held, closing) always closes the menu.
    connect(m_life, &LifecycleController::changed, this, [this] {
        if (m_life->state() != LifecycleController::State::Running)
            setOpen(false);
    });
}

void OverlayState::show()
{
    if (m_enabled && m_life->state() == LifecycleController::State::Running)
        setOpen(true);
}

void OverlayState::close()
{
    setOpen(false);
}

void OverlayState::toggle()
{
    if (m_open)
        close();
    else
        show();
}

void OverlayState::setOpen(bool open)
{
    if (open == m_open)
        return;
    m_open = open;
    qCInfo(lcOverlay) << (open ? "open" : "closed");
    Q_EMIT openChanged();
}
