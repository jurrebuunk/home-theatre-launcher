#include "Clock.h"

#include "Config.h"

#include <QDateTime>

Clock::Clock(const Config *config, QObject *parent)
    : QObject(parent)
{
    const QVariantMap c = config->clock();
    m_format = c.value("format").toString();
    m_dateFormat = c.value("dateFormat").toString();
    m_locale = QLocale(c.value("locale").toString());
    const QString tz = c.value("timeZone").toString();
    m_zone = tz.isEmpty() ? QTimeZone::systemTimeZone() : QTimeZone(tz.toUtf8());
    if (!m_zone.isValid())
        m_zone = QTimeZone::systemTimeZone();
    m_seconds = m_format.contains(QLatin1Char('s'));
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &Clock::tick);
    tick();
}

void Clock::setFixed(const QString &text, const QString &dateText)
{
    m_fixed = true;
    m_timer.stop();
    m_text = text;
    m_dateText = dateText;
    Q_EMIT textChanged();
}

void Clock::tick()
{
    if (m_fixed)
        return;
    const QDateTime now = QDateTime::currentDateTime().toTimeZone(m_zone);
    const QString text = now.toString(m_format);
    const QString date = m_locale.toString(now, m_dateFormat);
    if (text != m_text || date != m_dateText) {
        m_text = text;
        m_dateText = date;
        Q_EMIT textChanged();
    }
    const int ms = now.time().msec();
    const int next = m_seconds ? 1000 - ms : (60 - now.time().second()) * 1000 - ms;
    m_timer.start(qMax(50, next + 20));
}
