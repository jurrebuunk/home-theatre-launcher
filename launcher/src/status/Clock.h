#pragma once

#include <QLocale>
#include <QObject>
#include <QTimeZone>
#include <QTimer>

class Config;

// System time, configurable format and time zone; wakes once per minute
// (or per second when the format shows seconds).
class Clock : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QString dateText READ dateText NOTIFY textChanged)

public:
    explicit Clock(const Config *config, QObject *parent = nullptr);
    QString text() const { return m_text; }
    QString dateText() const { return m_dateText; }
    void setFixed(const QString &text, const QString &dateText = {});

Q_SIGNALS:
    void textChanged();

private:
    void tick();

    QString m_format;
    QString m_dateFormat;
    QLocale m_locale;
    QString m_dateText;
    QTimeZone m_zone;
    QString m_text;
    bool m_seconds = false;
    bool m_fixed = false;
    QTimer m_timer;
};
