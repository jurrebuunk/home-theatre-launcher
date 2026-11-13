#include "Clock.h"

#include "Config.h"

#include <QDateTime>

Clock::Clock(const Config *config, QObject *parent)
    : QObject(parent)
{
    const QVariantMap c = config->clock();
    m_format = c.value("format").toString();
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

void Clock::setFixed(const QString &text)
{
    m_fixed = true;
    m_timer.stop();
    m_text = text;
    Q_EMIT textChanged();
}

void Clock::tick()
{
    if (m_fixed)
        return;
    const QDateTime now = QDateTime::currentDateTime().toTimeZone(m_zone);
    const QString text = now.toString(m_format);
    if (text != m_text) {
        m_text = text;
        Q_EMIT textChanged();
    }
    const int ms = now.time().msec();
    const int next = m_seconds ? 1000 - ms : (60 - now.time().second()) * 1000 - ms;
    m_timer.start(qMax(50, next + 20));
}
