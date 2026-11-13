#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantMap>
#include <functional>

// A stable-order list of BlueZ device paths; rows keep their position while
// properties change so controller focus is never lost during a scan.
class DeviceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles { PathRole = Qt::UserRole + 1, NameRole, KindRole, ConnectedRole, TrustedRole, PairedRole, BusyRole };

    using Lookup = std::function<QVariantMap(const QString &path)>;
    explicit DeviceModel(Lookup lookup, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Bring the rows in line with `wanted` (removals + appends), refresh the rest.
    void sync(const QStringList &wanted);
    void refresh(const QString &path);
    Q_INVOKABLE QString pathAt(int row) const { return m_paths.value(row); }

    static QString kindForIcon(const QString &bluezIcon, const QString &name);

Q_SIGNALS:
    void countChanged();

private:
    Lookup m_lookup;
    QStringList m_paths;
};
