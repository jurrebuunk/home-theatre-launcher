#include "IdleMonitor.h"

#include "Config.h"

#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QUrl>

#include <algorithm>

Q_LOGGING_CATEGORY(lcIdle, "htl.idle")

IdleMonitor::IdleMonitor(const Config *config, QObject *parent)
    : QObject(parent)
{
    const QVariantMap c = config->idle();
    m_enabled = c.value("enable").toBool();
    m_background = c.value("background").toMap();
    if (m_background.value("color").toString().isEmpty())
        m_background.insert("color", config->colors().value("background"));
    m_timer.setSingleShot(true);
    m_timer.setInterval(c.value("timeout").toInt() * 1000);
    connect(&m_timer, &QTimer::timeout, this, &IdleMonitor::enter);
    restart();
}

void IdleMonitor::setBackground(const QVariantMap &background)
{
    for (auto it = background.cbegin(); it != background.cend(); ++it)
        m_background.insert(it.key(), it.value());
    m_images = resolveImages(m_background);
    Q_EMIT backgroundChanged();
    Q_EMIT imagesChanged();
}

void IdleMonitor::restart()
{
    if (m_enabled && !m_inhibited && !m_idle)
        m_timer.start();
    else
        m_timer.stop();
}

void IdleMonitor::setInhibited(bool inhibited)
{
    if (inhibited == m_inhibited)
        return;
    m_inhibited = inhibited;
    if (inhibited)
        setIdle(false);
    restart();
    Q_EMIT inhibitedChanged();
}

void IdleMonitor::activity()
{
    if (m_idle)
        setIdle(false);
    restart();
}

void IdleMonitor::enter()
{
    if (m_inhibited)
        return;
    setIdle(true);
}

void IdleMonitor::setIdle(bool idle)
{
    if (idle == m_idle)
        return;
    m_idle = idle;
    if (idle) {
        // Re-read the directory each time so new pictures show up.
        const QStringList images = resolveImages(m_background);
        if (images != m_images) {
            m_images = images;
            Q_EMIT imagesChanged();
        }
    }
    qCInfo(lcIdle) << (idle ? "idle" : "awake");
    restart();
    Q_EMIT idleChanged();
}

bool IdleMonitor::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
        if (m_idle) {
            activity();
            m_swallowRelease = true;
            return true;
        }
        activity();
        break;
    case QEvent::KeyRelease:
        if (m_swallowRelease) {
            if (!static_cast<QKeyEvent *>(event)->isAutoRepeat())
                m_swallowRelease = false;
            return true;
        }
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        if (m_idle) {
            activity();
            return true;
        }
        activity();
        break;
    case QEvent::MouseMove:
        activity();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

QStringList IdleMonitor::resolveImages(const QVariantMap &bg)
{
    const QString type = bg.value("type").toString();
    QStringList paths;
    if (type == QLatin1String("image"))
        paths << bg.value("image").toString();
    else if (type == QLatin1String("slideshow")) {
        paths << bg.value("images").toStringList();
        const QString dir = bg.value("directory").toString();
        if (!dir.isEmpty()) {
            const QStringList files = QDir(dir).entryList(
                {"*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp", "*.svg", "*.JPG", "*.JPEG", "*.PNG", "*.WEBP"},
                QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
            for (const QString &f : files)
                paths << QDir(dir).filePath(f);
        }
        if (bg.value("shuffle").toBool())
            std::shuffle(paths.begin(), paths.end(), *QRandomGenerator::global());
    }
    QStringList urls;
    for (const QString &p : paths) {
        if (p.startsWith(QLatin1String("builtin:")))
            urls << QStringLiteral("image://icon/background/") + p.mid(8);
        else if (!p.isEmpty())
            urls << QUrl::fromLocalFile(p).toString();
    }
    return urls;
}
