#include "DeviceModel.h"

DeviceModel::DeviceModel(Lookup lookup, QObject *parent)
    : QAbstractListModel(parent)
    , m_lookup(std::move(lookup))
{
}

int DeviceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_paths.size());
}

QString DeviceModel::kindForIcon(const QString &icon, const QString &name)
{
    if (name.contains(QLatin1String("remote"), Qt::CaseInsensitive))
        return QStringLiteral("remote");
    if (icon == QLatin1String("input-gaming"))
        return QStringLiteral("gamepad");
    if (icon == QLatin1String("input-keyboard"))
        return QStringLiteral("keyboard");
    if (icon == QLatin1String("input-mouse") || icon == QLatin1String("input-tablet"))
        return QStringLiteral("mouse");
    if (icon == QLatin1String("audio-headset") || icon == QLatin1String("audio-headphones"))
        return QStringLiteral("headphones");
    if (icon.startsWith(QLatin1String("audio")))
        return QStringLiteral("speaker");
    if (icon == QLatin1String("phone"))
        return QStringLiteral("phone");
    return QStringLiteral("device");
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_paths.size())
        return {};
    const QString path = m_paths.at(index.row());
    const QVariantMap d = m_lookup(path);
    const QString name = d.value(QStringLiteral("Alias"), d.value(QStringLiteral("Name"))).toString();
    switch (role) {
    case PathRole: return path;
    case NameRole: return name;
    case KindRole: return kindForIcon(d.value(QStringLiteral("Icon")).toString(), name);
    case ConnectedRole: return d.value(QStringLiteral("Connected")).toBool();
    case TrustedRole: return d.value(QStringLiteral("Trusted")).toBool();
    case PairedRole: return d.value(QStringLiteral("Paired")).toBool();
    case BusyRole: return d.value(QStringLiteral("_busy")).toBool();
    case BatteryRole: return d.value(QStringLiteral("Battery"), -1).toInt();
    default: return {};
    }
}

QHash<int, QByteArray> DeviceModel::roleNames() const
{
    return {{PathRole, "path"}, {NameRole, "name"}, {KindRole, "kind"}, {ConnectedRole, "connected"},
            {TrustedRole, "trusted"}, {PairedRole, "paired"}, {BusyRole, "busy"},
            {BatteryRole, "battery"}};
}

void DeviceModel::sync(const QStringList &wanted)
{
    const int before = int(m_paths.size());
    for (int i = int(m_paths.size()) - 1; i >= 0; --i) {
        if (!wanted.contains(m_paths.at(i))) {
            beginRemoveRows({}, i, i);
            m_paths.removeAt(i);
            endRemoveRows();
        }
    }
    for (const QString &p : wanted) {
        if (m_paths.contains(p))
            continue;
        const int row = int(m_paths.size());
        beginInsertRows({}, row, row);
        m_paths << p;
        endInsertRows();
    }
    if (!m_paths.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_paths.size()) - 1));
    if (before != m_paths.size())
        Q_EMIT countChanged();
}

void DeviceModel::refresh(const QString &path)
{
    const int row = int(m_paths.indexOf(path));
    if (row >= 0)
        Q_EMIT dataChanged(index(row), index(row));
}
