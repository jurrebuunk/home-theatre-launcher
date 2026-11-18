#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class Config;
class LifecycleController;

// Game libraries: apps of type "library" (e.g. an emulator) list the game
// files in their folders as cards, and a card starts the app with that game
// (the app's library.arguments, "{file}" replaced by the game's path).
// A picture next to a game with the same name (Game.iso + Game.jpg) is its box art.
class GameLibrary : public QObject
{
    Q_OBJECT

public:
    GameLibrary(const Config *config, LifecycleController *life, QObject *parent = nullptr);

    // [{name, path, art}] sorted by name; rescanned on every call.
    Q_INVOKABLE QVariantList games(const QString &appId) const;
    Q_INVOKABLE void play(const QString &appId, const QString &path);

    // Pure helpers (unit tested)
    // "Mario Kart Wii (Europe) (En,Fr).rvz" → "Mario Kart Wii"
    static QString titleFor(const QString &fileName);
    static QStringList argumentsFor(const QStringList &templ, const QString &file);
    static QVariantList scan(const QStringList &directories, const QStringList &extensions);

private:
    const Config *m_config;
    LifecycleController *m_life;
};
