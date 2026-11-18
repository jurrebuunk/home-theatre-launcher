#include "GameLibrary.h"

#include "Config.h"
#include "lifecycle/LifecycleController.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>

GameLibrary::GameLibrary(const Config *config, LifecycleController *life, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_life(life)
{
}

QString GameLibrary::titleFor(const QString &fileName)
{
    // Drop the extension and No-Intro / Redump tags: "(Europe)", "[!]".
    static const QRegularExpression tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
    QString t = QFileInfo(fileName).completeBaseName();
    if (t.endsWith(QLatin1String(".nkit"), Qt::CaseInsensitive))
        t.chop(5);
    const QString stripped = QString(t).remove(tags).simplified();
    return stripped.isEmpty() ? t : stripped;
}

QStringList GameLibrary::argumentsFor(const QStringList &templ, const QString &file)
{
    QStringList out;
    for (const QString &a : templ)
        out << QString(a).replace(QLatin1String("{file}"), file);
    return out;
}

QVariantList GameLibrary::scan(const QStringList &directories, const QStringList &extensions)
{
    QStringList filters;
    for (const QString &e : extensions)
        filters << QStringLiteral("*.") + e;
    QVariantList out;
    for (const QString &dir : directories) {
        QDirIterator it(dir, filters, QDir::Files | QDir::Readable,
                        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (it.hasNext()) {
            const QFileInfo f(it.next());
            QString art;
            for (const char *ext : {"jpg", "jpeg", "png", "webp"}) {
                const QString p = f.absolutePath() + QLatin1Char('/') + f.completeBaseName() + QLatin1Char('.') + QLatin1String(ext);
                if (QFileInfo::exists(p)) {
                    art = QUrl::fromLocalFile(p).toString(QUrl::FullyEncoded); // "#" etc. stay part of the path
                    break;
                }
            }
            out << QVariantMap{{"name", titleFor(f.fileName())}, {"path", f.absoluteFilePath()}, {"art", art}};
        }
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("name").toString().compare(b.toMap().value("name").toString(), Qt::CaseInsensitive) < 0;
    });
    return out;
}

QVariantList GameLibrary::games(const QString &appId) const
{
    const AppSpec *app = m_config->app(appId);
    if (!app || app->type != QLatin1String("library"))
        return {};
    return scan(app->libraryDirectories, app->libraryExtensions);
}

void GameLibrary::play(const QString &appId, const QString &path)
{
    const AppSpec *app = m_config->app(appId);
    if (!app || app->type != QLatin1String("library") || !QFileInfo::exists(path))
        return;
    m_life->launchSession(appId, argumentsFor(app->libraryArguments, path),
                          QVariantMap{{"appName", titleFor(path)}, {"file", path}});
}
