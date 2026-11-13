#pragma once

#include <QList>
#include <QObject>
#include <QString>

struct WindowInfo {
    quint32 id = 0;
    QString appId;
    QString title;
};

// Toplevel windows of other clients, as seen by the compositor.
class WindowSource : public QObject
{
    Q_OBJECT
public:
    explicit WindowSource(QObject *parent = nullptr) : QObject(parent) {}
    virtual bool available() const = 0;
    virtual QList<WindowInfo> windows() const = 0;
    virtual void activate(quint32 id) = 0;
Q_SIGNALS:
    void windowsChanged();
};

// Presence of well-known names on the session bus.
class NameSource : public QObject
{
    Q_OBJECT
public:
    explicit NameSource(QObject *parent = nullptr) : QObject(parent) {}
    virtual void watchName(const QString &name) = 0;
    virtual bool hasName(const QString &name) const = 0;
Q_SIGNALS:
    void namesChanged();
};
