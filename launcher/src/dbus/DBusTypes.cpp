#include "DBusTypes.h"

#include <QDBusMetaType>

void DBusTypes::registerAll()
{
    qDBusRegisterMetaType<InterfaceMap>();
    qDBusRegisterMetaType<ObjectMap>();
    qDBusRegisterMetaType<MapList>();
}

MapList DBusTypes::mapList(const QVariant &v)
{
    if (v.metaType() == QMetaType::fromType<MapList>())
        return v.value<MapList>();
    if (v.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<MapList>(v.value<QDBusArgument>());
    return {};
}

QStringList DBusTypes::pathList(const QVariant &v)
{
    QList<QDBusObjectPath> paths;
    if (v.metaType() == QMetaType::fromType<QList<QDBusObjectPath>>())
        paths = v.value<QList<QDBusObjectPath>>();
    else if (v.metaType() == QMetaType::fromType<QDBusArgument>())
        paths = qdbus_cast<QList<QDBusObjectPath>>(v.value<QDBusArgument>());
    QStringList out;
    for (const auto &p : paths)
        out << p.path();
    return out;
}

QString DBusTypes::path(const QVariant &v)
{
    if (v.metaType() == QMetaType::fromType<QDBusObjectPath>())
        return v.value<QDBusObjectPath>().path();
    return v.toString();
}
