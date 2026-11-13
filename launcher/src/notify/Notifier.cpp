#include "Notifier.h"

#include "Config.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcNotify, "htl.notify")

Notifier::Notifier(const Config *config, QObject *parent)
    : QObject(parent)
{
    const QVariantMap c = config->notifications();
    m_enabled = c.value("enable").toBool();
    m_timeoutMs = c.value("timeoutMs").toInt();
    m_low = c.value("batteryLow").toInt();
    m_critical = c.value("batteryCritical").toInt();
    m_repeatMs = c.value("criticalRepeatMinutes").toInt() * 60 * 1000;

    m_volumeTimer.setSingleShot(true);
    m_volumeTimer.setInterval(2000);
    connect(&m_volumeTimer, &QTimer::timeout, this, [this] { setVolumeShown(false); });
    m_criticalTimer.setInterval(60 * 1000);
    connect(&m_criticalTimer, &QTimer::timeout, this, &Notifier::checkCritical);
}

QStringList Notifier::titles() const
{
    QStringList out;
    for (const QVariant &v : m_items)
        out << v.toMap().value("title").toString();
    return out;
}

void Notifier::notify(const QVariantMap &item)
{
    if (!m_enabled)
        return;
    const bool wasActive = active();
    QVariantMap it = item;
    const int id = m_nextId++;
    it.insert("id", id);
    if (!it.contains("value"))
        it.insert("value", -1);
    if (!it.contains("tone"))
        it.insert("tone", "normal");
    if (!it.contains("style"))
        it.insert("style", "plain");
    // The same subject replaces its previous notification (e.g. connect → battery).
    for (int i = int(m_items.size()) - 1; i >= 0; --i)
        if (m_items.at(i).toMap().value("title") == it.value("title") && m_items.at(i).toMap().value("style") == it.value("style"))
            m_items.removeAt(i);
    m_items.prepend(it);
    while (m_items.size() > 3)
        m_items.removeLast();
    qCInfo(lcNotify) << it.value("title").toString() << it.value("trailing").toString() << it.value("tone").toString();
    const int ms = it.value("tone") == QLatin1String("critical") ? m_timeoutMs * 2 : m_timeoutMs;
    QTimer::singleShot(ms, this, [this, id] { remove(id); });
    Q_EMIT itemsChanged();
    if (!wasActive)
        Q_EMIT activeChanged();
}

void Notifier::remove(int id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).toMap().value("id").toInt() == id) {
            m_items.removeAt(i);
            Q_EMIT itemsChanged();
            if (!active())
                Q_EMIT activeChanged();
            return;
        }
    }
}

void Notifier::showVolume()
{
    setVolumeShown(true);
    m_volumeTimer.start();
}

void Notifier::setVolumeShown(bool shown)
{
    if (shown == m_volumeShown)
        return;
    const bool wasActive = active();
    m_volumeShown = shown;
    Q_EMIT volumeShownChanged();
    if (wasActive != active())
        Q_EMIT activeChanged();
}

QString Notifier::iconForKind(const QString &kind)
{
    static const QStringList known{"gamepad", "keyboard", "mouse", "headphones", "speaker", "phone", "remote", "tv"};
    return known.contains(kind) ? kind : QStringLiteral("bluetooth");
}

QString Notifier::batteryLevel(int percent, int low, int critical)
{
    if (percent < 0)
        return {};
    if (percent <= critical)
        return QStringLiteral("critical");
    if (percent <= low)
        return QStringLiteral("low");
    return {};
}

void Notifier::deviceConnection(const QString &path, const QString &name, const QString &kind, bool connected, int battery)
{
    notify({{"icon", iconForKind(kind)}, {"title", name}, {"value", connected ? battery : -1},
            {"trailing", connected ? "link" : "link-off"}});
    if (!connected)
        m_battery.remove(path);
}

void Notifier::deviceBattery(const QString &path, const QString &name, const QString &kind, int percent)
{
    if (kind != QLatin1String("gamepad") && kind != QLatin1String("remote"))
        return; // "controller battery"
    BatteryState &st = m_battery[path];
    st.percent = percent;
    st.name = name;
    st.kind = kind;
    if (percent > m_low + 5)
        st.lowShown = false; // charged again
    const QString level = batteryLevel(percent, m_low, m_critical);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (level == QLatin1String("critical")) {
        if (st.lastCritical == 0 || now - st.lastCritical >= m_repeatMs) {
            st.lastCritical = now;
            notify({{"icon", iconForKind(kind)}, {"title", name}, {"value", percent}, {"tone", "critical"}});
        }
        if (!m_criticalTimer.isActive())
            m_criticalTimer.start();
    } else {
        st.lastCritical = 0;
        if (level == QLatin1String("low") && !st.lowShown) {
            st.lowShown = true;
            notify({{"icon", iconForKind(kind)}, {"title", name}, {"value", percent}, {"tone", "warning"}});
        }
    }
}

void Notifier::checkCritical()
{
    bool any = false;
    for (auto it = m_battery.begin(); it != m_battery.end(); ++it)
        if (batteryLevel(it->percent, m_low, m_critical) == QLatin1String("critical")) {
            any = true;
            deviceBattery(it.key(), it->name, it->kind, it->percent);
        }
    if (!any)
        m_criticalTimer.stop();
}

void Notifier::wifi(const QString &ssid, bool up)
{
    notify({{"icon", "wifi"}, {"title", ssid}, {"trailing", up ? "link" : "down"}});
}

void Notifier::ethernet(bool up)
{
    notify({{"icon", "ethernet"}, {"title", "Ethernet"}, {"trailing", up ? "link" : "down"}});
}

void Notifier::outputSwitched(const QString &label, const QString &kind)
{
    notify({{"icon", iconForKind(kind)}, {"title", label}, {"style", "sound"}});
}

void Notifier::demo(const QString &view)
{
    if (view == QLatin1String("notifications")) {
        // Oldest first: the newest ends on top.
        notify({{"icon", "speaker"}, {"title", "Living Room Soundbar"}, {"style", "sound"}});
        notify({{"icon", "gamepad"}, {"title", "DualSense Wireless Controller"}, {"value", 15}, {"tone", "warning"}});
        notify({{"icon", "gamepad"}, {"title", "Xbox Wireless Controller"}, {"value", 80}, {"trailing", "link"}});
    }
    if (view == QLatin1String("notifications") || view == QLatin1String("volume"))
        showVolume();
    m_volumeTimer.stop();
}
