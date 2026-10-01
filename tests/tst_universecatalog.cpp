#include <QtTest>

#include <QFile>
#include <QSet>

#include "Library/CollectionShelf.h"
#include "Library/UniverseCatalog.h"

namespace {

MediaRecord film(qint64 mediaId, qint64 tmdbId, int year, int watched = 0)
{
    MediaRecord record;
    record.id = mediaId;
    record.tmdbId = tmdbId;
    record.kind = QStringLiteral("movie");
    record.year = year;
    record.fileCount = 1;
    record.watchedCount = watched;
    record.posterPath = QStringLiteral("/f%1.jpg").arg(tmdbId);
    return record;
}

const QByteArray kSmall = R"({
  "version": 1,
  "universes": [
    {
      "key": "mcu",
      "name": "Marvel Cinematic Universe",
      "poster": "/mcu.jpg",
      "backdrop": "/mcu-backdrop.jpg",
      "description": "Superheroes.",
      "collections": [131292, 131295],
      "storyOrder": [
        { "film": 1771, "title": "Captain America: The First Avenger", "released": "2011-07-22", "phase": "Phase One" },
        { "film": 1726, "title": "Iron Man", "released": "2008-05-02", "phase": "Phase One" },
        { "film": 1726, "title": "Iron Man again", "released": "2008-05-02", "phase": "Phase One" },
        { "film": 497698, "title": "Black Widow", "released": "2021-07-09", "phase": "Phase Four" },
        { "film": 1003596, "title": "Avengers: Doomsday", "released": "2026-12-18", "phase": "Phase Six" }
      ]
    },
    { "key": "mcu", "name": "Duplicate key", "storyOrder": [] },
    { "key": "", "name": "No key", "storyOrder": [] }
  ]
})";

const QDate kToday(2026, 9, 17);

}

class TestUniverseCatalog : public QObject
{
    Q_OBJECT

private slots:
    void aUniverseReadsItsCollectionsAndStoryOrder();
    void brokenJsonGivesNothingAndSaysWhy();
    void theBundledFileIsValid();
    void aUniverseReplacesItsCollections();
    void aFilmOutsideAnyCollectionJoinsItsUniverse();
    void aUniverseWithNothingOwnedIsLeftOut();
    void onlyOwnedUniversesHaveTheirPosterDownloaded();
};

void TestUniverseCatalog::onlyOwnedUniversesHaveTheirPosterDownloaded()
{
    const QList<UniverseCatalog::Universe> universes = UniverseCatalog::parse(kSmall);

    QVERIFY(UniverseCatalog::ownedPosters(universes, {{550, 0}}).isEmpty());
    QCOMPARE(UniverseCatalog::ownedPosters(universes, {{497698, 0}}),
             QStringList({QStringLiteral("/mcu.jpg")}));
    QCOMPARE(UniverseCatalog::ownedPosters(universes, {{999, 131292}, {1726, 131292}}),
             QStringList({QStringLiteral("/mcu.jpg")}));
}

void TestUniverseCatalog::aUniverseReadsItsCollectionsAndStoryOrder()
{
    QString error;
    const QList<UniverseCatalog::Universe> universes = UniverseCatalog::parse(kSmall, &error);

    QVERIFY(error.isEmpty());
    QCOMPARE(universes.size(), 1);
    const UniverseCatalog::Universe &mcu = universes.first();
    QCOMPARE(mcu.key, QStringLiteral("mcu"));
    QCOMPARE(mcu.posterPath, QStringLiteral("/mcu.jpg"));
    QCOMPARE(mcu.backdropPath, QStringLiteral("/mcu-backdrop.jpg"));
    QCOMPARE(mcu.description, QStringLiteral("Superheroes."));
    QCOMPARE(UniverseCatalog::ownedBackdrops(universes, {{1726, 0}}),
             QStringList({QStringLiteral("/mcu-backdrop.jpg")}));
    QVERIFY(UniverseCatalog::ownedBackdrops(universes, {{550, 0}}).isEmpty());
    QCOMPARE(mcu.collections, QSet<qint64>({131292, 131295}));
    QCOMPARE(mcu.storyOrder.size(), 4);
    QCOMPARE(mcu.storyOrder.at(0).tmdbId, Q_INT64_C(1771));
    QCOMPARE(mcu.storyOrder.at(1).released, QDate(2008, 5, 2));
    QCOMPARE(mcu.storyOrder.at(2).phase, QStringLiteral("Phase Four"));

    QVERIFY(mcu.contains(1726, 0));
    QVERIFY(mcu.contains(999, 131295));
    QVERIFY(!mcu.contains(999, 448150));
}

void TestUniverseCatalog::brokenJsonGivesNothingAndSaysWhy()
{
    QString error;
    QVERIFY(UniverseCatalog::parse("{ \"universes\": [", &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void TestUniverseCatalog::theBundledFileIsValid()
{
    QFile file(QStringLiteral(MM_SOURCE_DIR "/assets/universes.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));

    QString error;
    const QList<UniverseCatalog::Universe> universes = UniverseCatalog::parse(file.readAll(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(universes.size(), 11);

    QSet<QString> keysSeen;
    for (const UniverseCatalog::Universe &universe : universes) {
        QVERIFY2(!universe.key.isEmpty(), qPrintable(universe.name));
        QVERIFY2(!universe.name.isEmpty(), qPrintable(universe.key));
        QVERIFY2(!universe.description.isEmpty(), qPrintable(universe.key));

        QVERIFY2(!keysSeen.contains(universe.key), qPrintable(universe.key));
        keysSeen.insert(universe.key);

        QVERIFY2(!universe.collections.isEmpty() || !universe.storyOrder.isEmpty()
                     || !universe.titleKeywords.isEmpty(),
                 qPrintable(universe.key));

        for (const UniverseCatalog::Film &film : universe.storyOrder) {
            QVERIFY2(film.released.isValid(), qPrintable(film.title));
            QVERIFY2(!film.title.isEmpty(), qPrintable(universe.key));
        }
    }
}

void TestUniverseCatalog::aUniverseReplacesItsCollections()
{
    const QList<UniverseCatalog::Universe> universes = UniverseCatalog::parse(kSmall);

    CollectionRecord captainAmerica;
    captainAmerica.tmdbId = 131295;
    captainAmerica.name = QStringLiteral("Captain America Collection");
    CollectionPartRecord first;
    first.tmdbId = 1771;
    first.releaseDate = QStringLiteral("2011-07-22");
    first.posterPath = QStringLiteral("/p1771.jpg");
    captainAmerica.parts = {first};

    CollectionRecord deadpool;
    deadpool.tmdbId = 448150;
    deadpool.name = QStringLiteral("Deadpool Collection");
    CollectionPartRecord deadpoolOne;
    deadpoolOne.tmdbId = 293660;
    deadpoolOne.releaseDate = QStringLiteral("2016-02-12");
    CollectionPartRecord deadpoolTwo;
    deadpoolTwo.tmdbId = 383498;
    deadpoolTwo.releaseDate = QStringLiteral("2018-05-18");
    deadpool.parts = {deadpoolOne, deadpoolTwo};

    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {captainAmerica, deadpool}, {{1, 131295}, {2, 448150}},
        {film(1, 1771, 2011, 1), film(2, 293660, 2016)}, kToday, universes);

    QCOMPARE(shelf.size(), 2);
    QCOMPARE(shelf.at(0).name, QStringLiteral("Deadpool Collection"));
    const CollectionShelf::Summary &mcu = shelf.at(1);
    QVERIFY(mcu.universe);
    QCOMPARE(mcu.id, Q_INT64_C(-1));
    QCOMPARE(mcu.posterPath, QStringLiteral("/mcu.jpg"));
    QCOMPARE(mcu.total, 3);
    QCOMPARE(mcu.owned, 1);
    QCOMPARE(mcu.missing, 2);
    QCOMPARE(mcu.watched, 1);
    QCOMPARE(mcu.firstYear, 2008);
    QCOMPARE(mcu.lastYear, 2021);
    QCOMPARE(mcu.behindPosters.first(), QStringLiteral("/f1771.jpg"));
    QCOMPARE(mcu.state, CollectionShelf::State::InProgress);
}

void TestUniverseCatalog::aFilmOutsideAnyCollectionJoinsItsUniverse()
{
    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {}, {}, {film(1, 497698, 2021)}, kToday, UniverseCatalog::parse(kSmall));

    QCOMPARE(shelf.size(), 1);
    QVERIFY(shelf.first().universe);
    QCOMPARE(shelf.first().owned, 1);
    QCOMPARE(shelf.first().state, CollectionShelf::State::NotStarted);
}

void TestUniverseCatalog::aUniverseWithNothingOwnedIsLeftOut()
{
    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {}, {}, {film(1, 550, 1999)}, kToday, UniverseCatalog::parse(kSmall));

    QVERIFY(shelf.isEmpty());
}

QTEST_GUILESS_MAIN(TestUniverseCatalog)

#include "tst_universecatalog.moc"
