#pragma once

#include <QHash>
#include <QQuickImageProvider>

// image://icon/<name>?c=RRGGBB renders assets/icons/<name>.svg in that colour
// (`currentColor`, or the default fill of single-colour icons such as Material Symbols); image://icon/file/<abs path>?c=… does the same for
// app icons supplied through the config; image://icon/builtin/<name> uses
// the bundled app icons (stream, tv).
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QByteArray source(const QString &name);
    QHash<QString, QByteArray> m_cache;
};
