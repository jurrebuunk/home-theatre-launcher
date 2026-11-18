#include "library/GameLibrary.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

class TestLibrary : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void titles()
    {
        QCOMPARE(GameLibrary::titleFor("Mario Kart Wii (Europe, Australia) (En,Fr,De,Es,It).rvz"),
                 QStringLiteral("Mario Kart Wii"));
        QCOMPARE(GameLibrary::titleFor("/g/Super Mario Galaxy [RMGP01].nkit.iso"), QStringLiteral("Super Mario Galaxy"));
        QCOMPARE(GameLibrary::titleFor("Zelda.wbfs"), QStringLiteral("Zelda"));
        QCOMPARE(GameLibrary::titleFor("(Demo).iso"), QStringLiteral("(Demo)"));
    }
    void arguments()
    {
        QCOMPARE(GameLibrary::argumentsFor({"--batch", "--exec={file}", "{file}"}, "/g/a b.iso"),
                 (QStringList{"--batch", "--exec=/g/a b.iso", "/g/a b.iso"}));
    }
    void scan()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDir(dir.path()).mkpath("Wii");
        for (const char *f : {"Wii/b game (Europe).rvz", "Wii/b game (Europe).jpg", "A game.iso", "notes.txt"}) {
            QFile file(dir.filePath(f));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const QVariantList games = GameLibrary::scan({dir.path()}, {"iso", "rvz"});
        QCOMPARE(games.size(), 2);
        QCOMPARE(games.at(0).toMap().value("name").toString(), QStringLiteral("A game"));
        QVERIFY(games.at(0).toMap().value("art").toString().isEmpty());
        QCOMPARE(games.at(1).toMap().value("name").toString(), QStringLiteral("b game"));
        QVERIFY(games.at(1).toMap().value("art").toString().endsWith("/Wii/b%20game%20(Europe).jpg"));
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "tst_library.moc"
