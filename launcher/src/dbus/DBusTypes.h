#pragma once

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QVariantMap>

// a{sa{sv}} (interfaces → properties, NetworkManager connection settings)
using InterfaceMap = QMap<QString, QVariantMap>;
// a{oa{sa{sv}}} (ObjectManager.GetManagedObjects)
using ObjectMap = QMap<QDBusObjectPath, InterfaceMap>;
// aa{sv}
using MapList = QList<QVariantMap>;

Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ObjectMap)

namespace DBusTypes {
void registerAll();

// Values nested inside a{sv} arrive as QDBusArgument; these unwrap them.
MapList mapList(const QVariant &v);
QStringList pathList(const QVariant &v);
QString path(const QVariant &v);
} // namespace DBusTypes
