#include "ToplevelWatcher.h"

#include <QLoggingCategory>
#include <QSocketNotifier>

#include <cstring>
#include <wayland-client.h>

#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

Q_LOGGING_CATEGORY(lcToplevel, "htl.toplevel")

namespace {

void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    static_cast<ToplevelWatcher *>(data)->onGlobal(registry, name, interface, version);
}
void registryGlobalRemove(void *, wl_registry *, uint32_t) {}
const wl_registry_listener kRegistryListener = {registryGlobal, registryGlobalRemove};

using Handle = ToplevelWatcher::Handle;

void handleTitle(void *data, zwlr_foreign_toplevel_handle_v1 *, const char *title)
{
    static_cast<Handle *>(data)->title = QString::fromUtf8(title);
}
void handleAppId(void *data, zwlr_foreign_toplevel_handle_v1 *, const char *appId)
{
    static_cast<Handle *>(data)->appId = QString::fromUtf8(appId);
}
void handleOutputEnter(void *, zwlr_foreign_toplevel_handle_v1 *, wl_output *) {}
void handleOutputLeave(void *, zwlr_foreign_toplevel_handle_v1 *, wl_output *) {}
void handleState(void *, zwlr_foreign_toplevel_handle_v1 *, wl_array *) {}
void handleDone(void *data, zwlr_foreign_toplevel_handle_v1 *)
{
    auto *h = static_cast<Handle *>(data);
    h->owner->onHandleDone(h);
}
void handleClosed(void *data, zwlr_foreign_toplevel_handle_v1 *)
{
    auto *h = static_cast<Handle *>(data);
    h->owner->onHandleClosed(h);
}
void handleParent(void *, zwlr_foreign_toplevel_handle_v1 *, zwlr_foreign_toplevel_handle_v1 *) {}
const zwlr_foreign_toplevel_handle_v1_listener kHandleListener = {
    handleTitle, handleAppId, handleOutputEnter, handleOutputLeave,
    handleState, handleDone, handleClosed, handleParent,
};

void managerToplevel(void *data, zwlr_foreign_toplevel_manager_v1 *, zwlr_foreign_toplevel_handle_v1 *handle)
{
    static_cast<ToplevelWatcher *>(data)->onToplevel(handle);
}
void managerFinished(void *data, zwlr_foreign_toplevel_manager_v1 *)
{
    static_cast<ToplevelWatcher *>(data)->onManagerFinished();
}
const zwlr_foreign_toplevel_manager_v1_listener kManagerListener = {managerToplevel, managerFinished};

} // namespace

ToplevelWatcher::ToplevelWatcher(QObject *parent)
    : WindowSource(parent)
{
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        qCWarning(lcToplevel) << "cannot connect to Wayland display; window readiness unavailable";
        return;
    }
    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &kRegistryListener, this);
    wl_display_roundtrip(m_display); // globals
    if (!m_manager) {
        qCWarning(lcToplevel) << "compositor lacks zwlr_foreign_toplevel_manager_v1; window readiness unavailable";
    }
    wl_display_roundtrip(m_display); // initial toplevels

    m_notifier = new QSocketNotifier(wl_display_get_fd(m_display), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &ToplevelWatcher::dispatch);
    flush();
}

ToplevelWatcher::~ToplevelWatcher()
{
    qDeleteAll(m_handles);
    if (m_display)
        wl_display_disconnect(m_display);
}

void ToplevelWatcher::onGlobal(wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    if (std::strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0 && !m_manager) {
        m_manager = static_cast<zwlr_foreign_toplevel_manager_v1 *>(
            wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, qMin<uint32_t>(version, 3)));
        zwlr_foreign_toplevel_manager_v1_add_listener(m_manager, &kManagerListener, this);
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0 && !m_seat) {
        m_seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
    }
}

void ToplevelWatcher::onToplevel(zwlr_foreign_toplevel_handle_v1 *handle)
{
    auto *h = new Handle;
    h->owner = this;
    h->handle = handle;
    h->id = m_nextId++;
    m_handles.insert(h->id, h);
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &kHandleListener, h);
}

void ToplevelWatcher::onHandleDone(Handle *h)
{
    h->done = true;
    Q_EMIT windowsChanged();
}

void ToplevelWatcher::onHandleClosed(Handle *h)
{
    m_handles.remove(h->id);
    zwlr_foreign_toplevel_handle_v1_destroy(h->handle);
    delete h;
    Q_EMIT windowsChanged();
}

void ToplevelWatcher::onManagerFinished()
{
    m_manager = nullptr;
}

QList<WindowInfo> ToplevelWatcher::windows() const
{
    QList<WindowInfo> out;
    for (const Handle *h : m_handles)
        if (h->done)
            out << WindowInfo{h->id, h->appId, h->title};
    return out;
}

void ToplevelWatcher::activate(quint32 id)
{
    Handle *h = m_handles.value(id);
    if (!h || !m_seat)
        return;
    zwlr_foreign_toplevel_handle_v1_activate(h->handle, m_seat);
    flush();
}

void ToplevelWatcher::dispatch()
{
    if (wl_display_dispatch(m_display) < 0) {
        qCWarning(lcToplevel) << "Wayland connection lost";
        m_notifier->setEnabled(false);
        m_manager = nullptr;
        return;
    }
    flush();
}

void ToplevelWatcher::flush()
{
    if (m_display)
        wl_display_flush(m_display);
}
