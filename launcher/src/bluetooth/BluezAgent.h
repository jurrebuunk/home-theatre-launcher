#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusObjectPath>

#include <functional>

// org.bluez.Agent1 — every request is answered later (delayed reply) once the
// user has decided on screen. The handler owns the pending message.
class BluezAgentAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")

public:
    using Handler = std::function<void(const QString &type, const QString &device, const QVariant &value,
                                       const QDBusMessage &msg)>;
    BluezAgentAdaptor(QObject *parent, Handler handler, std::function<void()> cancel)
        : QDBusAbstractAdaptor(parent)
        , m_handler(std::move(handler))
        , m_cancel(std::move(cancel))
    {
    }

public Q_SLOTS:
    void Release() { m_cancel(); }
    QString RequestPinCode(const QDBusObjectPath &device, const QDBusMessage &msg)
    {
        defer(msg);
        m_handler(QStringLiteral("requestPin"), device.path(), {}, msg);
        return {};
    }
    void DisplayPinCode(const QDBusObjectPath &device, const QString &pin, const QDBusMessage &msg)
    {
        m_handler(QStringLiteral("displayPin"), device.path(), pin, msg);
    }
    uint RequestPasskey(const QDBusObjectPath &device, const QDBusMessage &msg)
    {
        defer(msg);
        m_handler(QStringLiteral("requestPasskey"), device.path(), {}, msg);
        return 0;
    }
    void DisplayPasskey(const QDBusObjectPath &device, uint passkey, ushort entered, const QDBusMessage &msg)
    {
        Q_UNUSED(entered)
        m_handler(QStringLiteral("displayPasskey"), device.path(), passkey, msg);
    }
    void RequestConfirmation(const QDBusObjectPath &device, uint passkey, const QDBusMessage &msg)
    {
        defer(msg);
        m_handler(QStringLiteral("confirm"), device.path(), passkey, msg);
    }
    void RequestAuthorization(const QDBusObjectPath &device, const QDBusMessage &msg)
    {
        defer(msg);
        m_handler(QStringLiteral("authorize"), device.path(), {}, msg);
    }
    void AuthorizeService(const QDBusObjectPath &device, const QString &uuid, const QDBusMessage &msg)
    {
        defer(msg);
        m_handler(QStringLiteral("authorizeService"), device.path(), uuid, msg);
    }
    void Cancel() { m_cancel(); }

private:
    static void defer(const QDBusMessage &msg) { const_cast<QDBusMessage &>(msg).setDelayedReply(true); }

    Handler m_handler;
    std::function<void()> m_cancel;
};
