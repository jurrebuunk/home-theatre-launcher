#pragma once

#include <QAbstractListModel>
#include <QList>

struct WifiRow {
    QString ssid;
    int strength = 0;         // 0–100
    QString security;         // open, owe, wep, psk, sae, enterprise
    bool active = false;
    bool saved = false;
    QString ap;               // strongest access point object path

    bool secured() const { return security != QLatin1String("open") && security != QLatin1String("owe"); }
    bool operator==(const WifiRow &o) const = default;
};

// Visible Wi-Fi networks, one row per SSID. Rows keep their position while
// signal strengths change (focus stays put); sort() reorders on demand.
class WifiModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles { SsidRole = Qt::UserRole + 1, StrengthRole, BarsRole, SecuredRole, SecurityRole, ActiveRole, SavedRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void sync(QList<WifiRow> rows, bool resort);
    const QList<WifiRow> &rows() const { return m_rows; }
    const WifiRow *find(const QString &ssid) const;
    Q_INVOKABLE QString ssidAt(int row) const { return m_rows.value(row).ssid; }

    static int bars(int strength);
    static bool lessThan(const WifiRow &a, const WifiRow &b);

Q_SIGNALS:
    void countChanged();

private:
    QList<WifiRow> m_rows;
};
