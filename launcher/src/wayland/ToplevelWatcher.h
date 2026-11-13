#pragma once

#include "lifecycle/Sources.h"

#include <QHash>

struct wl_display;
struct wl_registry;
struct wl_seat;
struct zwlr_foreign_toplevel_manager_v1;
struct zwlr_foreign_toplevel_handle_v1;
class QSocketNotifier;

// Tracks other clients' toplevels through wlr-foreign-toplevel-management on a
// private Wayland connection, event driven via the connection fd.
class ToplevelWatcher : public WindowSource
{
    Q_OBJECT
public:
    explicit ToplevelWatcher(QObject *parent = nullptr);
    ~ToplevelWatcher() override;

    bool available() const override { return m_manager != nullptr; }
    QList<WindowInfo> windows() const override;
    void activate(quint32 id) override;

    // wayland listener trampolines
    struct Handle {
        ToplevelWatcher *owner = nullptr;
        zwlr_foreign_toplevel_handle_v1 *handle = nullptr;
        quint32 id = 0;
        QString appId;
        QString title;
        bool done = false;
    };
    void onGlobal(wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    void onToplevel(zwlr_foreign_toplevel_handle_v1 *handle);
    void onHandleDone(Handle *h);
    void onHandleClosed(Handle *h);
    void onManagerFinished();

private:
    void dispatch();
    void flush();

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_seat *m_seat = nullptr;
    zwlr_foreign_toplevel_manager_v1 *m_manager = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    QHash<quint32, Handle *> m_handles;
    quint32 m_nextId = 1;
};
