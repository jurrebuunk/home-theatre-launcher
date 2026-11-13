#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class Config;

// Shows the idle screen after `timeout` seconds without input while the
// launcher is in front. Any input wakes it; the waking key press (and its
// release) is swallowed so it never also activates something.
class IdleMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)
    Q_PROPERTY(bool enabled READ enabled CONSTANT)
    Q_PROPERTY(bool inhibited READ inhibited WRITE setInhibited NOTIFY inhibitedChanged)
    Q_PROPERTY(QVariantMap background READ background NOTIFY backgroundChanged)
    Q_PROPERTY(QStringList images READ images NOTIFY imagesChanged)

public:
    explicit IdleMonitor(const Config *config, QObject *parent = nullptr);

    bool idle() const { return m_idle; }
    bool enabled() const { return m_enabled; }
    bool inhibited() const { return m_inhibited; }
    void setInhibited(bool inhibited);
    QVariantMap background() const { return m_background; }
    void setBackground(const QVariantMap &background); // demo/screenshots
    QStringList images() const { return m_images; }
    int timeoutMs() const { return m_timer.interval(); }
    void setTimeoutMs(int ms) { m_timer.setInterval(ms); }

    // User input happened (from the window event filter or evdev).
    Q_INVOKABLE void activity();
    // Enter the idle screen now (control API, screenshots).
    Q_INVOKABLE void enter();

    bool eventFilter(QObject *watched, QEvent *event) override;

    // Image URLs for the configured background: explicit images, then the
    // directory's pictures (sorted, or shuffled).
    static QStringList resolveImages(const QVariantMap &background);

Q_SIGNALS:
    void idleChanged();
    void inhibitedChanged();
    void imagesChanged();
    void backgroundChanged();

private:
    void setIdle(bool idle);
    void restart();

    bool m_enabled = true;
    bool m_idle = false;
    bool m_inhibited = false;
    bool m_swallowRelease = false;
    QVariantMap m_background;
    QStringList m_images;
    QTimer m_timer;
};
