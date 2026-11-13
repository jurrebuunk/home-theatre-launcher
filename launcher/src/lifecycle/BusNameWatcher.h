#pragma once

#include "Sources.h"

#include <QDBusServiceWatcher>
#include <QSet>

class BusNameWatcher : public NameSource
{
    Q_OBJECT
public:
    explicit BusNameWatcher(QObject *parent = nullptr);
    void watchName(const QString &name) override;
    bool hasName(const QString &name) const override { return m_present.contains(name); }

private:
    QDBusServiceWatcher m_watcher;
    QSet<QString> m_present;
};
