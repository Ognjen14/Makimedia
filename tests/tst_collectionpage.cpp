#include <QtTest>

#include <QSet>

#include "Library/CollectionPage.h"

namespace {

MediaRecord film(qint64 mediaId, qint64 tmdbId, const QString &title, int year,
                 int runtime, double rating, int watched = 0, double progress = 0.0)
{
    MediaRecord record;
    record.id = mediaId;
    record.tmdbId = tmdbId;
    record.kind = QStringLiteral("movie");
    record.title = title;
    record.year = year;
    record.runtimeMinutes = runtime;
    record.rating = rating;
    record.fileCount = 1;
    record.watchedCount = watched;
    record.partialProgress = progress;
    record.firstFileHandle = QStringLiteral("F:/Films/%1.mkv").arg(tmdbId);
    record.posterPath = QStringLiteral("/f%1.jpg").arg(tmdbId);
    record.backdropPath = QStringLiteral("/b%1.jpg").arg(tmdbId);
    return record;
}

CollectionPartRecord part(qint64 tmdbId, const QString &title, const QString &released)
{
    CollectionPartRecord record;
    record.tmdbId = tmdbId;
    record.title = title;
    record.releaseDate = released;
    record.posterPath = QStringLiteral("/p%1.jpg").arg(tmdbId);
    return record;
}

CollectionRecord jumpStreet()
{
    CollectionRecord collection;
    collection.tmdbId = 206;
    collection.name = QStringLiteral("Jump Street Collection");
    collection.overview = QStringLiteral("Two cops go undercover.");
    collection.backdropPath = QStringLiteral("/js.jpg");
    collection.parts = {part(107575, QStringLiteral("22 Jump Street"), QStringLiteral("2014-06-13")),
                        part(64688, QStringLiteral("21 Jump Street"), QStringLiteral("2012-03-16"))};
    return collection;
}

UniverseCatalog::Universe starWars()
{
    UniverseCatalog::Universe universe;
    universe.key = QStringLiteral("star-wars");
    universe.name = QStringLiteral("Star Wars");
    universe.description = QStringLiteral("A galaxy far away.");
    universe.collections = {10};
    universe.storyOrder = {
        {1893, QStringLiteral("The Phantom Menace"), QDate(1999, 5, 19), QStringLiteral("Prequel trilogy")},
        {348350, QStringLiteral("Solo"), QDate(2018, 5, 25), QStringLiteral("Anthology")},
        {11, QStringLiteral("Star Wars"), QDate(1977, 5, 25), QStringLiteral("Original trilogy")},
        {1892, QStringLiteral("Return of the Jedi"), QDate(1983, 5, 25), QStringLiteral("Original trilogy")},
        {999999, QStringLiteral("Next Episode"), QDate(2030, 1, 1), QStringLiteral("Future")}
    };
    return universe;
}

const QDate kToday(2026, 9, 17);

}

class TestCollectionPage : public QObject
{
    Q_OBJECT

private slots:
    void aCollectionListsItsFilmsInReleaseOrder();
    void theHeaderAddsUpTheSeries();
    void theNextFilmIsTheOneInProgress();
    void aUniverseInStoryOrderHasPhaseHeadings();
    void aUniverseInReleaseOrderHasNoHeadings();
    void aPhaseThatComesBackGetsItsOwnHeading();
    void anUnknownCollectionIsInvalid();
    void aFilmNotOnDiskTakesItsFetchedDetails();
    void theFilmsToAskAboutAreReleasedAndNotOwned();
    void aCollectionTheUserMadeIsItsOwnTitlesInOrder();
};

void TestCollectionPage::aCollectionTheUserMadeIsItsOwnTitlesInOrder()
{
    CustomCollectionRecord made;
    made.id = 7;
    made.name = QStringLiteral("Friday night");
    made.description = QStringLiteral("Loud ones.");
    made.mediaIds = {2, 1, 9, 404};

    MediaRecord show = film(9, 1438, QStringLiteral("The Wire"), 2002, 59, 8.6);
    show.kind = QStringLiteral("tv");

    const QList<MediaRecord> films = {
        film(1, 64688, QStringLiteral("21 Jump Street"), 2012, 109, 7.0, 1),
        film(2, 107575, QStringLiteral("22 Jump Street"), 2014, 112, 6.8),
        show
    };

    const qint64 id = CollectionShelf::customId(made.id);
    QVERIFY(CollectionShelf::isCustom(id));
    QCOMPARE(CollectionShelf::customCollectionIdOf(id), Q_INT64_C(7));

    const CollectionPage::Page page = CollectionPage::build(
        id, CollectionPage::Release, {}, {}, films, {}, kToday, {}, {made});

    QVERIFY(page.header.valid);
    QVERIFY(page.header.custom);
    QVERIFY(!page.header.hasStoryOrder);
    QCOMPARE(page.header.tag, QStringLiteral("custom"));
    QCOMPARE(page.header.description, QStringLiteral("Loud ones."));
    QCOMPARE(page.header.total, 3);
    QCOMPARE(page.header.owned, 3);
    QCOMPARE(page.header.watched, 1);
    QCOMPARE(page.header.totalMinutes, 221);
    QCOMPARE(page.rows.size(), 3);
    QCOMPARE(page.rows.at(0).title, QStringLiteral("22 Jump Street"));
    QCOMPARE(page.rows.at(1).title, QStringLiteral("21 Jump Street"));
    QCOMPARE(page.rows.at(2).title, QStringLiteral("The Wire"));
    QVERIFY(page.rows.at(2).show);
    QVERIFY(!page.rows.at(0).show);
    QCOMPARE(page.header.nextTitle, QStringLiteral("22 Jump Street"));

    QVERIFY(!CollectionPage::build(CollectionShelf::customId(99), 0, {}, {}, films, {},
                                   kToday, {}, {made}).header.valid);
}

void TestCollectionPage::aFilmNotOnDiskTakesItsFetchedDetails()
{
    FilmDetailsRecord details;
    details.tmdbId = 64688;
    details.title = QStringLiteral("21 Jump Street");
    details.posterPath = QStringLiteral("/d64688.jpg");
    details.runtimeMinutes = 109;
    details.rating = 7.0;

    const CollectionPage::Page page = CollectionPage::build(
        206, CollectionPage::Release, {jumpStreet()}, {{1, 206}},
        {film(1, 107575, QStringLiteral("22 Jump Street"), 2014, 112, 6.8)}, {}, kToday,
        {{64688, details}});

    const CollectionPage::Row &missing = page.rows.at(0);
    QVERIFY(!missing.owned);
    QCOMPARE(missing.posterPath, QStringLiteral("/d64688.jpg"));
    QCOMPARE(missing.runtimeMinutes, 109);
    QCOMPARE(page.header.totalMinutes, 221);
    QCOMPARE(page.header.leftMinutes, 221);
    QCOMPARE(page.header.averageRating, 6.9);
}

void TestCollectionPage::theFilmsToAskAboutAreReleasedAndNotOwned()
{
    CollectionRecord collection = jumpStreet();
    collection.parts.append(part(500, QStringLiteral("23 Jump Street"), QStringLiteral("2031-01-01")));

    const QList<qint64> ids = CollectionPage::filmsNotInLibrary(
        {collection}, {{107575, 206}, {1892, 10}}, {starWars()}, kToday);

    QCOMPARE(ids, QList<qint64>({64688, 1893, 348350, 11}));
}

void TestCollectionPage::aCollectionListsItsFilmsInReleaseOrder()
{
    const CollectionPage::Page page = CollectionPage::build(
        206, CollectionPage::Story, {jumpStreet()}, {{1, 206}},
        {film(1, 107575, QStringLiteral("22 Jump Street"), 2014, 112, 6.8, 0, 0.4)}, {}, kToday);

    QVERIFY(page.header.valid);
    QVERIFY(!page.header.hasStoryOrder);
    QCOMPARE(page.header.tag, QStringLiteral("duology"));
    QCOMPARE(page.header.description, QStringLiteral("Two cops go undercover."));
    QCOMPARE(page.header.backdropPath, QStringLiteral("/js.jpg"));
    QCOMPARE(page.rows.size(), 2);

    const CollectionPage::Row &first = page.rows.at(0);
    QCOMPARE(first.number, 1);
    QCOMPARE(first.title, QStringLiteral("21 Jump Street"));
    QVERIFY(!first.owned);
    QCOMPARE(first.year, 2012);
    QCOMPARE(first.posterPath, QStringLiteral("/p64688.jpg"));

    const CollectionPage::Row &second = page.rows.at(1);
    QCOMPARE(second.number, 2);
    QVERIFY(second.owned);
    QCOMPARE(second.handle, QStringLiteral("F:/Films/107575.mkv"));
    QCOMPARE(second.posterPath, QStringLiteral("/f107575.jpg"));
    QCOMPARE(second.runtimeMinutes, 112);
}

void TestCollectionPage::theHeaderAddsUpTheSeries()
{
    CollectionRecord collection = jumpStreet();
    collection.parts.append(part(500, QStringLiteral("23 Jump Street"), QStringLiteral("2031-01-01")));

    const CollectionPage::Page page = CollectionPage::build(
        206, CollectionPage::Release, {collection}, {{1, 206}, {2, 206}},
        {film(1, 64688, QStringLiteral("21 Jump Street"), 2012, 109, 7.0, 1),
         film(2, 107575, QStringLiteral("22 Jump Street"), 2014, 112, 6.8, 0, 0.5)},
        {}, kToday);

    const CollectionPage::Header &header = page.header;
    QCOMPARE(header.total, 2);
    QCOMPARE(header.owned, 2);
    QCOMPARE(header.watched, 1);
    QCOMPARE(header.totalMinutes, 221);
    QCOMPARE(header.leftMinutes, 56);
    QCOMPARE(header.averageRating, 6.9);
    QCOMPARE(header.firstYear, 2012);
    QCOMPARE(header.lastYear, 2014);
    QCOMPARE(header.ownedHandles.size(), 2);
    QVERIFY(!header.allOwnedWatched);
    QCOMPARE(header.posters, QStringList({QStringLiteral("/f64688.jpg"),
                                          QStringLiteral("/f107575.jpg")}));
}

void TestCollectionPage::theNextFilmIsTheOneInProgress()
{
    const CollectionPage::Page started = CollectionPage::build(
        206, CollectionPage::Release, {jumpStreet()}, {{1, 206}, {2, 206}},
        {film(1, 64688, QStringLiteral("21 Jump Street"), 2012, 109, 7.0),
         film(2, 107575, QStringLiteral("22 Jump Street"), 2014, 100, 6.8, 0, 0.25)},
        {}, kToday);
    QCOMPARE(started.header.nextTitle, QStringLiteral("22 Jump Street"));
    QVERIFY(started.header.nextResumes);
    QCOMPARE(started.header.nextLeftMinutes, 75);

    const CollectionPage::Page fresh = CollectionPage::build(
        206, CollectionPage::Release, {jumpStreet()}, {{1, 206}, {2, 206}},
        {film(1, 64688, QStringLiteral("21 Jump Street"), 2012, 109, 7.0),
         film(2, 107575, QStringLiteral("22 Jump Street"), 2014, 100, 6.8)},
        {}, kToday);
    QCOMPARE(fresh.header.nextTitle, QStringLiteral("21 Jump Street"));
    QVERIFY(!fresh.header.nextResumes);

    const CollectionPage::Page done = CollectionPage::build(
        206, CollectionPage::Release, {jumpStreet()}, {{1, 206}},
        {film(1, 64688, QStringLiteral("21 Jump Street"), 2012, 109, 7.0, 1)}, {}, kToday);
    QVERIFY(done.header.nextHandle.isEmpty());
    QVERIFY(done.header.allOwnedWatched);
}

void TestCollectionPage::aUniverseInStoryOrderHasPhaseHeadings()
{
    const CollectionPage::Page page = CollectionPage::build(
        -1, CollectionPage::Story, {}, {{1, 10}},
        {film(1, 1892, QStringLiteral("Return of the Jedi"), 1983, 131, 7.9)},
        {starWars()}, kToday);

    QVERIFY(page.header.universe);
    QVERIFY(page.header.hasStoryOrder);
    QCOMPARE(page.header.tag, QStringLiteral("universe"));
    QCOMPARE(page.header.total, 4);
    QCOMPARE(page.header.backdropPath, QStringLiteral("/b1892.jpg"));

    QStringList shape;
    for (const CollectionPage::Row &row : page.rows) {
        shape.append(row.heading ? QStringLiteral("# ") + row.phase
                                 : QString::number(row.number) + QLatin1Char(' ') + row.title);
    }
    QCOMPARE(shape, QStringList({QStringLiteral("# Prequel trilogy"),
                                 QStringLiteral("1 The Phantom Menace"),
                                 QStringLiteral("# Anthology"),
                                 QStringLiteral("2 Solo"),
                                 QStringLiteral("# Original trilogy"),
                                 QStringLiteral("3 Star Wars"),
                                 QStringLiteral("4 Return of the Jedi")}));
}

void TestCollectionPage::aUniverseInReleaseOrderHasNoHeadings()
{
    const CollectionPage::Page page = CollectionPage::build(
        -1, CollectionPage::Release, {}, {},
        {film(1, 1892, QStringLiteral("Return of the Jedi"), 1983, 131, 7.9)},
        {starWars()}, kToday);

    QStringList titles;
    for (const CollectionPage::Row &row : page.rows) {
        QVERIFY(!row.heading);
        titles.append(row.title);
    }
    QCOMPARE(titles, QStringList({QStringLiteral("Star Wars"),
                                  QStringLiteral("Return of the Jedi"),
                                  QStringLiteral("The Phantom Menace"),
                                  QStringLiteral("Solo")}));
}

void TestCollectionPage::aPhaseThatComesBackGetsItsOwnHeading()
{
    UniverseCatalog::Universe mcu;
    mcu.key = QStringLiteral("mcu");
    mcu.name = QStringLiteral("Marvel Cinematic Universe");
    mcu.storyOrder = {
        {1771, QStringLiteral("Captain America: The First Avenger"), QDate(2011, 7, 22), QStringLiteral("Phase One")},
        {299537, QStringLiteral("Captain Marvel"), QDate(2019, 3, 8), QStringLiteral("Phase Three")},
        {1726, QStringLiteral("Iron Man"), QDate(2008, 5, 2), QStringLiteral("Phase One")}
    };

    const CollectionPage::Page page = CollectionPage::build(
        -1, CollectionPage::Story, {}, {},
        {film(1, 1726, QStringLiteral("Iron Man"), 2008, 126, 7.6)}, {mcu}, kToday);

    QCOMPARE(page.rows.size(), 6);
    QSet<QString> keys;
    for (const CollectionPage::Row &row : page.rows) {
        QVERIFY2(!keys.contains(row.key), qPrintable(row.key));
        keys.insert(row.key);
    }
    QVERIFY(page.rows.at(0).heading);
    QVERIFY(page.rows.at(4).heading);
    QCOMPARE(page.rows.at(4).phase, QStringLiteral("Phase One"));
}

void TestCollectionPage::anUnknownCollectionIsInvalid()
{
    QVERIFY(!CollectionPage::build(0, 0, {}, {}, {}, {}, kToday).header.valid);
    QVERIFY(!CollectionPage::build(42, 0, {jumpStreet()}, {}, {}, {}, kToday).header.valid);
    QVERIFY(!CollectionPage::build(-7, 0, {}, {}, {}, {starWars()}, kToday).header.valid);
}

QTEST_GUILESS_MAIN(TestCollectionPage)

#include "tst_collectionpage.moc"
