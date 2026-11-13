#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class Config;

// System notifications (top-right, max 3 stacked) and the volume indicator
// (bottom-centre). They sit above everything, never take input and never dim.
class Notifier : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(bool volumeShown READ volumeShown NOTIFY volumeShownChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    explicit Notifier(const Config *config, QObject *parent = nullptr);

    QVariantList items() const { return m_items; }
    bool volumeShown() const { return m_volumeShown; }
    bool active() const { return !m_items.isEmpty() || m_volumeShown; }
    QStringList titles() const;

    // icon, title, value (-1 none), trailing (link, link-off, down, ""), tone (normal, warning, critical),
    // style (plain, sound)
    void notify(const QVariantMap &item);
    void showVolume();

    // Event sources
    void deviceConnection(const QString &path, const QString &name, const QString &kind, bool connected, int battery);
    void deviceBattery(const QString &path, const QString &name, const QString &kind, int percent);
    void wifi(const QString &ssid, bool up);
    void ethernet(bool up);
    void outputSwitched(const QString &label, const QString &kind);

    static QString iconForKind(const QString &kind);
    // Battery rules: "" (nothing), "low" (once), "critical" (repeats).
    static QString batteryLevel(int percent, int low, int critical);

    void demo(const QString &view);

Q_SIGNALS:
    void itemsChanged();
    void volumeShownChanged();
    void activeChanged();

private:
    void remove(int id);
    void setVolumeShown(bool shown);
    void checkCritical();

    bool m_enabled = true;
    int m_timeoutMs = 4000;
    int m_low = 20;
    int m_critical = 10;
    int m_repeatMs = 5 * 60 * 1000;
    QVariantList m_items;
    int m_nextId = 1;
    bool m_volumeShown = false;
    QTimer m_volumeTimer;
    QTimer m_criticalTimer;
    struct BatteryState {
        bool lowShown = false;
        qint64 lastCritical = 0;
        int percent = -1;
        QString name, kind;
    };
    QHash<QString, BatteryState> m_battery;
};
