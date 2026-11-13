#pragma once

#include <QObject>

class LifecycleController;

// Session bus API nl.jurrebuunk.HomeTheatre1 at /nl/jurrebuunk/HomeTheatre,
// used by tests and automation (e.g. a compositor keybinding for Home).
class ControlService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "nl.jurrebuunk.HomeTheatre1")
    Q_PROPERTY(QString State READ state NOTIFY StateChanged SCRIPTABLE true)
    Q_PROPERTY(QString AppId READ appId SCRIPTABLE true)

public:
    explicit ControlService(LifecycleController *life, QObject *parent = nullptr);
    bool registerOnBus();

    QString state() const;
    QString appId() const;

public Q_SLOTS:
    Q_SCRIPTABLE void Launch(const QString &id);
    Q_SCRIPTABLE void Home();
    Q_SCRIPTABLE void OpenBluetooth();

Q_SIGNALS:
    Q_SCRIPTABLE void StateChanged(const QString &state);
    void openBluetoothRequested();

private:
    LifecycleController *m_life;
};
