#include "NetworkStatus.h"

#include "Config.h"

#include <QFile>
#include <QHostInfo>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

Q_LOGGING_CATEGORY(lcNet, "htl.network")

NetworkStatus::NetworkStatus(const Config *config, QObject *parent)
    : QObject(parent)
{
    const QVariantMap c = config->internet();
    m_interface = c.value("interface").toString();
    m_url = QUrl(c.value("url").toString());
    m_dnsHost = c.value("dnsHost").toString();
    if (m_dnsHost.isEmpty())
        m_dnsHost = m_url.host();
    m_expectStatus = c.value("expectStatus").toInt();
    m_timeoutMs = c.value("timeout").toInt() * 1000;
    m_intervalMs = c.value("interval").toInt() * 1000;
    m_offlineIntervalMs = c.value("offlineInterval").toInt() * 1000;

    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &NetworkStatus::check);
    m_dnsTimeout.setSingleShot(true);
    connect(&m_dnsTimeout, &QTimer::timeout, this, [this] {
        if (m_lookupId >= 0) {
            QHostInfo::abortHostLookup(m_lookupId);
            m_lookupId = -1;
            finish(false, QStringLiteral("dns"));
        }
    });
}

void NetworkStatus::start()
{
    if (!m_demo)
        check();
}

void NetworkStatus::setDemo(const QString &state, const QString &reason)
{
    m_demo = true;
    m_timer.stop();
    m_state = state;
    m_reason = reason;
    Q_EMIT changed();
}

bool NetworkStatus::hasDefaultRoute(const QByteArray &table, const QString &interface)
{
    const QList<QByteArray> lines = table.split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> f = lines.at(i).simplified().split(' ');
        // Iface Destination Gateway Flags ... Mask
        if (f.size() >= 8 && QString::fromLatin1(f.at(0)) == interface && f.at(1) == "00000000"
            && f.at(7) == "00000000")
            return true;
    }
    return false;
}

void NetworkStatus::check()
{
    if (m_busy || m_demo)
        return;
    QFile carrier(QStringLiteral("/sys/class/net/%1/carrier").arg(m_interface));
    if (!carrier.open(QIODevice::ReadOnly) || carrier.readAll().trimmed() != "1") {
        finish(false, QStringLiteral("nolink"));
        return;
    }
    QFile route(QStringLiteral("/proc/net/route"));
    if (route.open(QIODevice::ReadOnly) && !hasDefaultRoute(route.readAll(), m_interface)) {
        // IPv4 default route missing; IPv6-only setups still get a chance via DNS/HTTPS.
        QFile route6(QStringLiteral("/proc/net/ipv6_route"));
        const bool v6 = route6.open(QIODevice::ReadOnly)
            && route6.readAll().contains("00000000000000000000000000000000 00 ");
        if (!v6) {
            finish(false, QStringLiteral("noroute"));
            return;
        }
    }

    // The previous state stays visible while this check runs (no flicker).
    m_busy = true;
    m_dnsTimeout.start(m_timeoutMs);
    m_lookupId = QHostInfo::lookupHost(m_dnsHost, this, [this](const QHostInfo &info) {
        if (m_lookupId < 0)
            return; // timed out already
        m_lookupId = -1;
        m_dnsTimeout.stop();
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            m_busy = false;
            finish(false, QStringLiteral("dns"));
            return;
        }
        if (!m_nam)
            m_nam = new QNetworkAccessManager(this);
        QNetworkRequest req(m_url);
        req.setTransferTimeout(m_timeoutMs);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        QNetworkReply *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            m_busy = false;
            finish(status == m_expectStatus, status == m_expectStatus ? QString() : QStringLiteral("http"));
        });
    });
}

void NetworkStatus::finish(bool online, const QString &reason)
{
    m_busy = false;
    const QString state = online ? QStringLiteral("online") : QStringLiteral("offline");
    if (state != m_state || reason != m_reason) {
        qCInfo(lcNet) << "internet" << state << reason;
        m_state = state;
        m_reason = reason;
        Q_EMIT changed();
    }
    m_timer.start(online ? m_intervalMs : m_offlineIntervalMs);
}
