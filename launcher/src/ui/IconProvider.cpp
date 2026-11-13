#include "IconProvider.h"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>
#include <QUrlQuery>

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QByteArray IconProvider::source(const QString &name)
{
    if (auto it = m_cache.constFind(name); it != m_cache.cend())
        return it.value();
    QString path;
    if (name.startsWith(QLatin1String("file/")))
        path = name.mid(4); // keeps the leading '/'
    else if (name.startsWith(QLatin1String("glyph/")))
        path = QStringLiteral(":/glyphs/%1.svg").arg(name.mid(6));
    else if (name.startsWith(QLatin1String("builtin/")))
        path = QStringLiteral(":/apps/%1.svg").arg(name.mid(8));
    else if (name.startsWith(QLatin1String("background/")))
        path = QStringLiteral(":/backgrounds/%1.svg").arg(name.mid(11));
    else
        path = QStringLiteral(":/icons/%1.svg").arg(name);
    QFile f(path);
    QByteArray data;
    if (path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive) && f.open(QIODevice::ReadOnly))
        data = f.readAll();
    m_cache.insert(name, data);
    return data;
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int q = int(id.indexOf(QLatin1Char('?')));
    const QString name = q < 0 ? id : id.left(q);
    const QUrlQuery query(q < 0 ? QString() : id.mid(q + 1));
    const QString color = query.queryItemValue(QStringLiteral("c"));

    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(64, 64);
    QImage img(target, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QByteArray svg = source(name);
    if (svg.isEmpty()) {
        // Raster app icons (png/jpg) are drawn as-is.
        const QString path = name.startsWith(QLatin1String("file/")) ? name.mid(4) : name;
        QImage raster(path);
        if (!raster.isNull())
            img = raster.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        if (!color.isEmpty()) {
            const QByteArray hex = "#" + color.toLatin1();
            if (svg.contains("currentColor"))
                svg.replace("currentColor", hex);
            else // single-colour icon sets (e.g. Material Symbols) rely on the default fill
                svg.replace("<svg ", "<svg fill=\"" + hex + "\" ");
        }
        QSvgRenderer renderer(svg);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        QSizeF s = renderer.defaultSize();
        // Backgrounds cover the whole target; icons fit inside it.
        s.scale(target, name.startsWith(QLatin1String("background/")) ? Qt::KeepAspectRatioByExpanding
                                                                       : Qt::KeepAspectRatio);
        const QRectF r((target.width() - s.width()) / 2, (target.height() - s.height()) / 2, s.width(), s.height());
        renderer.render(&p, r);
    }
    if (size)
        *size = img.size();
    return img;
}
