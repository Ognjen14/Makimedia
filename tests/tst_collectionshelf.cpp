#include <QtTest>

#include "Library/CollectionShelf.h"

namespace {

CollectionPartRecord part(qint64 tmdbId, const QString &released, const QString &poster = QString())
{
    CollectionPartRecord record;
    record.tmdbId = tmdbId;
    record.releaseDate = released;
    record.posterPath = poster.isEmpty() ? QStringLiteral("/p%1.jpg").arg(tmdbId) : poster;
    return record;
}

MediaRecord film(qint64 mediaId, qint64 tmdbId, int year, int watched = 0, double progress = 0.0)
{
    MediaRecord record;
    record.id = mediaId;
    record.tmdbId = tmdbId;
    record.kind = QStringLiteral("movie");
    record.year = year;
    record.fileCount = 1;
    record.watchedCount = watched;
    record.partialProgress = progress;
    record.posterPath = QStringLiteral("/f%1.jpg").arg(tmdbId);
    return record;
}

CollectionRecord backToTheFuture()
{
    CollectionRecord collection;
    collection.tmdbId = 264;
    collection.name = QStringLiteral("Back to the Future Collection");
    collection.posterPath = QStringLiteral("/bttf.jpg");
    collection.parts = {part(105, QStringLiteral("1985-07-03")),
                        part(165, QStringLiteral("1989-11-22")),
                        part(196, QStringLiteral("1990-05-25"))};
    return collection;
}

const QDate kToday(2026, 9, 17);

}

class TestCollectionShelf : public QObject
{
    Q_OBJECT

private slots:
    void aCollectionCountsTheWholeSeriesAndWhatIsMissing();
    void unreleasedFilmsAreNotCountedOrMissing();
    void everyFilmWatchedIsCompleted();
    void aStartedFilmMakesItInProgress();
    void aCollectionWaitsForItsFilmList();
    void aSeriesWithOneReleasedFilmIsNotACollection();
    void aCollectionWithNothingOwnedIsLeftOut();
    void theFilterKeepsOnlyItsState();
    void yearsAreWrittenShort();
    void aCollectionTheUserMadeIsAlwaysComplete();
    void aCollectionTakenOffThePageStaysOff();
};

void TestCollectionShelf::aCollectionCountsTheWholeSeriesAndWhatIsMissing()
{
    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {backToTheFuture()}, {{1, 264}}, {film(1, 105, 1985, 1)}, kToday);

    QCOMPARE(shelf.size(), 1);
    const CollectionShelf::Summary &summary = shelf.first();
    QCOMPARE(summary.total, 3);
    QCOMPARE(summary.owned, 1);
    QCOMPARE(summary.missing, 2);
    QCOMPARE(summary.watched, 1);
    QCOMPARE(summary.firstYear, 1985);
    QCOMPARE(summary.lastYear, 1990);
    QCOMPARE(summary.state, CollectionShelf::State::InProgress);
    QCOMPARE(summary.behindPosters, QStringList({QStringLiteral("/p105.jpg"),
                                                 QStringLiteral("/p165.jpg")}));
    QCOMPARE(summary.progress(), 1.0 / 3.0);
}

void TestCollectionShelf::unreleasedFilmsAreNotCountedOrMissing()
{
    CollectionRecord collection = backToTheFuture();
    collection.parts.append(part(900, QStringLiteral("2027-01-01")));
    collection.parts.append(part(901, QString()));

    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {collection}, {{1, 264}}, {film(1, 105, 1985)}, kToday);

    QCOMPARE(shelf.first().total, 3);
    QCOMPARE(shelf.first().missing, 2);
    QCOMPARE(shelf.first().lastYear, 1990);
}

void TestCollectionShelf::everyFilmWatchedIsCompleted()
{
    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {backToTheFuture()}, {{1, 264}, {2, 264}, {3, 264}},
        {film(1, 105, 1985, 1), film(2, 165, 1989, 1), film(3, 196, 1990, 1)}, kToday);

    QCOMPARE(shelf.first().missing, 0);
    QCOMPARE(shelf.first().watched, 3);
    QCOMPARE(shelf.first().state, CollectionShelf::State::Completed);
}

void TestCollectionShelf::aStartedFilmMakesItInProgress()
{
    const QList<CollectionShelf::Summary> started = CollectionShelf::build(
        {backToTheFuture()}, {{1, 264}}, {film(1, 105, 1985, 0, 0.4)}, kToday);
    QCOMPARE(started.first().state, CollectionShelf::State::InProgress);
    QCOMPARE(started.first().watched, 0);

    const QList<CollectionShelf::Summary> untouched = CollectionShelf::build(
        {backToTheFuture()}, {{1, 264}}, {film(1, 105, 1985)}, kToday);
    QCOMPARE(untouched.first().state, CollectionShelf::State::NotStarted);
}

void TestCollectionShelf::aCollectionWaitsForItsFilmList()
{
    CollectionRecord collection = backToTheFuture();
    collection.parts.clear();

    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {collection}, {{1, 264}, {2, 264}},
        {film(1, 105, 1985), film(2, 165, 1989)}, kToday);

    QVERIFY(shelf.isEmpty());
}

void TestCollectionShelf::aSeriesWithOneReleasedFilmIsNotACollection()
{
    CollectionRecord collection = backToTheFuture();
    collection.parts = {part(105, QStringLiteral("1985-07-03")),
                        part(900, QStringLiteral("2030-01-01"))};

    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {collection}, {{1, 264}}, {film(1, 105, 1985)}, kToday);

    QVERIFY(shelf.isEmpty());
}

void TestCollectionShelf::aCollectionWithNothingOwnedIsLeftOut()
{
    CollectionRecord alien;
    alien.tmdbId = 8091;
    alien.name = QStringLiteral("Alien Collection");

    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {backToTheFuture(), alien}, {{1, 264}}, {film(1, 105, 1985)}, kToday);

    QCOMPARE(shelf.size(), 1);
    QCOMPARE(shelf.first().id, Q_INT64_C(264));
}

void TestCollectionShelf::theFilterKeepsOnlyItsState()
{
    CollectionRecord alien;
    alien.tmdbId = 8091;
    alien.name = QStringLiteral("Alien Collection");
    alien.parts = {part(348, QStringLiteral("1979-05-25")),
                   part(679, QStringLiteral("1986-07-18"))};

    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {backToTheFuture(), alien}, {{1, 264}, {2, 8091}, {3, 8091}},
        {film(1, 105, 1985), film(2, 348, 1979, 1), film(3, 679, 1986, 1)}, kToday);

    QCOMPARE(shelf.size(), 2);
    QCOMPARE(shelf.at(0).name, QStringLiteral("Alien Collection"));
    QCOMPARE(CollectionShelf::filtered(shelf, CollectionShelf::All).size(), 2);
    QCOMPARE(CollectionShelf::filtered(shelf, CollectionShelf::Completed).first().id,
             Q_INT64_C(8091));
    QCOMPARE(CollectionShelf::filtered(shelf, CollectionShelf::NotStarted).first().id,
             Q_INT64_C(264));
    QVERIFY(CollectionShelf::filtered(shelf, CollectionShelf::InProgress).isEmpty());
}

void TestCollectionShelf::aCollectionTheUserMadeIsAlwaysComplete()
{
    CustomCollectionRecord made;
    made.id = 3;
    made.name = QStringLiteral("Friday night");
    made.mediaIds = {1, 2, 77};

    const QList<CollectionShelf::Summary> shelf = CollectionShelf::build(
        {}, {}, {film(1, 105, 1985, 1), film(2, 165, 1989)}, kToday, {}, {made});

    QCOMPARE(shelf.size(), 1);
    const CollectionShelf::Summary &summary = shelf.first();
    QVERIFY(summary.custom);
    QCOMPARE(summary.id, CollectionShelf::customId(3));
    QCOMPARE(summary.name, QStringLiteral("Friday night"));
    QCOMPARE(summary.total, 2);
    QCOMPARE(summary.owned, 2);
    QCOMPARE(summary.missing, 0);
    QCOMPARE(summary.watched, 1);
    QCOMPARE(summary.state, CollectionShelf::State::InProgress);
    QCOMPARE(summary.firstYear, 1985);
    QCOMPARE(summary.lastYear, 1989);

    CustomCollectionRecord gone;
    gone.id = 4;
    gone.name = QStringLiteral("Nothing left");
    gone.mediaIds = {77};
    QVERIFY(CollectionShelf::build({}, {}, {film(1, 105, 1985)}, kToday, {}, {gone}).isEmpty());
}

void TestCollectionShelf::aCollectionTakenOffThePageStaysOff()
{
    CustomCollectionRecord made;
    made.id = 3;
    made.name = QStringLiteral("Friday night");
    made.mediaIds = {1};

    const QList<MediaRecord> films = {film(1, 105, 1985), film(2, 165, 1989)};
    const QHash<qint64, qint64> byMedia = {{1, 264}, {2, 264}};

    QCOMPARE(CollectionShelf::build({backToTheFuture()}, byMedia, films, kToday, {}, {made}).size(),
             2);

    const QList<CollectionShelf::Summary> hiddenOne = CollectionShelf::build(
        {backToTheFuture()}, byMedia, films, kToday, {}, {made}, {264});
    QCOMPARE(hiddenOne.size(), 1);
    QVERIFY(hiddenOne.first().custom);

    const QList<CollectionShelf::Summary> hiddenBoth = CollectionShelf::build(
        {backToTheFuture()}, byMedia, films, kToday, {}, {made},
        {264, CollectionShelf::customId(3)});
    QVERIFY(hiddenBoth.isEmpty());
}

void TestCollectionShelf::yearsAreWrittenShort()
{
    QCOMPARE(CollectionShelf::yearsText(1985, 1990), QStringLiteral("1985") + QChar(0x2013) + QStringLiteral("90"));
    QCOMPARE(CollectionShelf::yearsText(1999, 2003), QStringLiteral("1999") + QChar(0x2013) + QStringLiteral("2003"));
    QCOMPARE(CollectionShelf::yearsText(2001, 2001), QStringLiteral("2001"));
    QCOMPARE(CollectionShelf::yearsText(0, 0), QString());
}

QTEST_GUILESS_MAIN(TestCollectionShelf)

#include "tst_collectionshelf.moc"
