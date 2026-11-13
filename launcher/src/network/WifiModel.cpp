#include "WifiModel.h"

#include <algorithm>

int WifiModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant WifiModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const WifiRow &r = m_rows.at(index.row());
    switch (role) {
    case SsidRole: return r.ssid;
    case StrengthRole: return r.strength;
    case BarsRole: return bars(r.strength);
    case SecuredRole: return r.secured();
    case SecurityRole: return r.security;
    case ActiveRole: return r.active;
    case SavedRole: return r.saved;
    default: return {};
    }
}

QHash<int, QByteArray> WifiModel::roleNames() const
{
    return {{SsidRole, "ssid"}, {StrengthRole, "strength"}, {BarsRole, "bars"}, {SecuredRole, "secured"},
            {SecurityRole, "security"}, {ActiveRole, "active"}, {SavedRole, "saved"}};
}

int WifiModel::bars(int strength)
{
    return strength >= 67 ? 3 : strength >= 34 ? 2 : strength > 0 ? 1 : 0;
}

bool WifiModel::lessThan(const WifiRow &a, const WifiRow &b)
{
    if (a.active != b.active)
        return a.active;
    if (a.saved != b.saved)
        return a.saved;
    if (bars(a.strength) != bars(b.strength))
        return a.strength > b.strength;
    return a.ssid.localeAwareCompare(b.ssid) < 0;
}

const WifiRow *WifiModel::find(const QString &ssid) const
{
    for (const WifiRow &r : m_rows)
        if (r.ssid == ssid)
            return &r;
    return nullptr;
}

void WifiModel::sync(QList<WifiRow> rows, bool resort)
{
    const int before = int(m_rows.size());
    if (resort) {
        std::sort(rows.begin(), rows.end(), lessThan);
        if (rows != m_rows) {
            beginResetModel();
            m_rows = rows;
            endResetModel();
        }
    } else {
        // Remove vanished rows (back to front), update the rest in place, append new ones.
        for (int i = int(m_rows.size()) - 1; i >= 0; --i) {
            const QString ssid = m_rows.at(i).ssid;
            if (std::none_of(rows.cbegin(), rows.cend(), [&](const WifiRow &r) { return r.ssid == ssid; })) {
                beginRemoveRows({}, i, i);
                m_rows.removeAt(i);
                endRemoveRows();
            }
        }
        QList<WifiRow> added;
        for (const WifiRow &r : rows) {
            auto it = std::find_if(m_rows.begin(), m_rows.end(), [&](const WifiRow &x) { return x.ssid == r.ssid; });
            if (it == m_rows.end()) {
                added << r;
            } else if (!(*it == r)) {
                *it = r;
                const QModelIndex idx = index(int(it - m_rows.begin()));
                Q_EMIT dataChanged(idx, idx);
            }
        }
        std::sort(added.begin(), added.end(), lessThan);
        if (!added.isEmpty()) {
            beginInsertRows({}, int(m_rows.size()), int(m_rows.size() + added.size() - 1));
            m_rows << added;
            endInsertRows();
        }
    }
    if (m_rows.size() != before)
        Q_EMIT countChanged();
}
