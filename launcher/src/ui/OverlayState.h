#pragma once

#include <QObject>

class LifecycleController;

// The in-app menu: shown above a running app (which keeps running and
// rendering behind it). Never opens on the launcher itself.
class OverlayState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool enabled READ enabled CONSTANT)
    Q_PROPERTY(double dim READ dim CONSTANT)

public:
    OverlayState(LifecycleController *life, bool enabled, double dim, QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    bool enabled() const { return m_enabled; }
    double dim() const { return m_dim; }

    Q_INVOKABLE void show();
    Q_INVOKABLE void close();
    Q_INVOKABLE void toggle();

Q_SIGNALS:
    void openChanged();

private:
    void setOpen(bool open);

    LifecycleController *m_life;
    bool m_enabled;
    double m_dim;
    bool m_open = false;
};
