#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>

class Config;
class QNetworkAccessManager;
class QNetworkReply;

// Real reachability over the configured Ethernet interface:
// carrier → default route via that interface → DNS → HTTPS with expected status.
// Timer driven, fully asynchronous, never blocks the UI or launches.
class NetworkStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)   // checking | online | offline
    Q_PROPERTY(QString reason READ reason NOTIFY changed) // nolink | noroute | dns | http | ""
    Q_PROPERTY(QString interfaceName READ interfaceName CONSTANT)

public:
    explicit NetworkStatus(const Config *config, QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString reason() const { return m_reason; }
    QString interfaceName() const { return m_interface; }

    void start();
    void setDemo(const QString &state, const QString &reason);

    // Exposed for tests: parse /proc/net/route content.
    static bool hasDefaultRoute(const QByteArray &procNetRoute, const QString &interface);

Q_SIGNALS:
    void changed();

private:
    void check();
    void finish(bool online, const QString &reason);

    QString m_interface;
    QUrl m_url;
    QString m_dnsHost;
    int m_expectStatus = 204;
    int m_timeoutMs = 5000;
    int m_intervalMs = 30000;
    int m_offlineIntervalMs = 5000;

    QString m_state = QStringLiteral("checking");
    QString m_reason;
    bool m_busy = false;
    bool m_demo = false;
    int m_lookupId = -1;
    QTimer m_timer;
    QTimer m_dnsTimeout;
    QNetworkAccessManager *m_nam = nullptr;
};
