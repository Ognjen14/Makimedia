#include <QtTest>

#include "Library/TitleArrangement.h"

namespace {

MediaRecord film(qint64 id, const QString &title, int year, double rating, int runtime)
{
    MediaRecord record;
    record.id = id;
    record.tmdbId = id;
    record.kind = QStringLiteral("movie");
    record.title = title;
    record.year = year;
    record.rating = rating;
    record.runtimeMinutes = runtime;
    record.fileCount = 1;
    return record;
}

MediaRecord show(qint64 id, const QString &title, int episodes, int watched, double progress)
{
    MediaRecord record;
    record.id = id;
    record.tmdbId = id;
    record.kind = QStringLiteral("tv");
    record.title = title;
    record.fileCount = episodes;
    record.watchedCount = watched;
    record.partialProgress = progress;
    return record;
}

QStringList titlesOf(const QList<MediaRecord> &records)
{
    QStringList names;
    for (const MediaRecord &record : records) {
        names.append(record.title);
    }
    return names;
}

}

class TestTitleArrangement : public QObject
{
    Q_OBJECT

private slots:
    void eachSortOrdersFilms_data();
    void eachSortOrdersFilms();

    void aShowIsAsLongAsItsEpisodes();
    void theFiltersSplitByWatchState();
    void aGenreNarrowsTheListAndListsOnlyWhatIsThere();
};

void TestTitleArrangement::eachSortOrdersFilms_data()
{
    QTest::addColumn<int>("sort");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("title a to z")
        << int(TitleArrangement::TitleAscending)
        << QStringList({QStringLiteral("Alien"), QStringLiteral("Brazil"),
                        QStringLiteral("Casablanca"), QStringLiteral("Dune")});
    QTest::newRow("recently added")
        << int(TitleArrangement::RecentlyAdded)
        << QStringList({QStringLiteral("Dune"), QStringLiteral("Alien"),
                        QStringLiteral("Brazil"), QStringLiteral("Casablanca")});
    QTest::newRow("year newest, unknown year last")
        << int(TitleArrangement::YearNewest)
        << QStringList({QStringLiteral("Brazil"), QStringLiteral("Alien"),
                        QStringLiteral("Casablanca"), QStringLiteral("Dune")});
    QTest::newRow("year oldest, unknown year last")
        << int(TitleArrangement::YearOldest)
        << QStringList({QStringLiteral("Casablanca"), QStringLiteral("Alien"),
                        QStringLiteral("Brazil"), QStringLiteral("Dune")});
    QTest::newRow("rating")
        << int(TitleArrangement::Rating)
        << QStringList({QStringLiteral("Casablanca"), QStringLiteral("Alien"),
                        QStringLiteral("Brazil"), QStringLiteral("Dune")});
    QTest::newRow("shortest, unknown runtime last")
        << int(TitleArrangement::Shortest)
        << QStringList({QStringLiteral("Casablanca"), QStringLiteral("Alien"),
                        QStringLiteral("Brazil"), QStringLiteral("Dune")});
    QTest::newRow("longest, unknown runtime last")
        << int(TitleArrangement::Longest)
        << QStringList({QStringLiteral("Brazil"), QStringLiteral("Alien"),
                        QStringLiteral("Casablanca"), QStringLiteral("Dune")});
}

void TestTitleArrangement::eachSortOrdersFilms()
{
    QFETCH(int, sort);
    QFETCH(QStringList, expected);

    const QList<MediaRecord> films = {
        film(1, QStringLiteral("Dune"), 0, 7.0, 0),
        film(2, QStringLiteral("Alien"), 1979, 8.2, 117),
        film(3, QStringLiteral("Casablanca"), 1942, 8.5, 102),
        film(4, QStringLiteral("Brazil"), 1985, 7.8, 142),
    };
    const QHash<qint64, qint64> added = {{1, 400}, {2, 300}, {3, 100}, {4, 200}};

    QCOMPARE(titlesOf(TitleArrangement::arrange(films, sort,
                                                TitleArrangement::Everything, QString(),
                                                added)),
             expected);
}

void TestTitleArrangement::aShowIsAsLongAsItsEpisodes()
{
    const QList<MediaRecord> shows = {
        show(1, QStringLiteral("Dark"), 26, 0, 0.0),
        show(2, QStringLiteral("Chernobyl"), 5, 0, 0.0),
        show(3, QStringLiteral("Vikings"), 89, 0, 0.0),
    };

    QCOMPARE(titlesOf(TitleArrangement::arrange(shows, TitleArrangement::Shortest,
                                                TitleArrangement::Everything, QString(), {})),
             QStringList({QStringLiteral("Chernobyl"), QStringLiteral("Dark"),
                          QStringLiteral("Vikings")}));
}

void TestTitleArrangement::theFiltersSplitByWatchState()
{
    MediaRecord halfFilm = film(4, QStringLiteral("Half film"), 2000, 0.0, 90);
    halfFilm.partialProgress = 0.4;
    MediaRecord seenFilm = film(5, QStringLiteral("Seen film"), 2000, 0.0, 90);
    seenFilm.watchedCount = 1;

    const QList<MediaRecord> titles = {
        show(1, QStringLiteral("Fresh show"), 10, 0, 0.0),
        show(2, QStringLiteral("Half show"), 10, 3, 0.0),
        show(3, QStringLiteral("Seen show"), 10, 10, 0.0),
        halfFilm,
        seenFilm,
    };

    QCOMPARE(titlesOf(TitleArrangement::arrange(titles, TitleArrangement::TitleAscending,
                                                TitleArrangement::Unwatched, QString(), {})),
             QStringList({QStringLiteral("Fresh show")}));
    QCOMPARE(titlesOf(TitleArrangement::arrange(titles, TitleArrangement::TitleAscending,
                                                TitleArrangement::InProgress, QString(), {})),
             QStringList({QStringLiteral("Half film"), QStringLiteral("Half show")}));
    QCOMPARE(titlesOf(TitleArrangement::arrange(titles, TitleArrangement::TitleAscending,
                                                TitleArrangement::Watched, QString(), {})),
             QStringList({QStringLiteral("Seen film"), QStringLiteral("Seen show")}));
    QCOMPARE(TitleArrangement::arrange(titles, TitleArrangement::TitleAscending,
                                       TitleArrangement::Everything, QString(), {}).size(),
             5);
}

void TestTitleArrangement::aGenreNarrowsTheListAndListsOnlyWhatIsThere()
{
    MediaRecord heat = film(1, QStringLiteral("Heat"), 1995, 7.9, 170);
    heat.genres = QStringLiteral("Action, Crime");
    MediaRecord amelie = film(2, QStringLiteral("Amelie"), 2001, 7.9, 122);
    amelie.genres = QStringLiteral("Comedy, Romance");
    MediaRecord vikings = show(3, QStringLiteral("Vikings"), 89, 0, 0.0);
    vikings.genres = QStringLiteral("Action & Adventure, Drama");

    const QList<MediaRecord> titles = {heat, amelie, vikings};

    QCOMPARE(TitleArrangement::genresIn(titles),
             QStringList({QStringLiteral("Action"), QStringLiteral("Adventure"),
                          QStringLiteral("Comedy"), QStringLiteral("Crime"),
                          QStringLiteral("Drama"), QStringLiteral("Romance")}));
    QCOMPARE(titlesOf(TitleArrangement::arrange(titles, TitleArrangement::TitleAscending,
                                                TitleArrangement::Everything,
                                                QStringLiteral("Action"), {})),
             QStringList({QStringLiteral("Heat"), QStringLiteral("Vikings")}));
    QCOMPARE(titlesOf(TitleArrangement::arrange(titles, TitleArrangement::TitleAscending,
                                                TitleArrangement::Everything,
                                                QStringLiteral("Western"), {})),
             QStringList());
}

QTEST_GUILESS_MAIN(TestTitleArrangement)
#include "tst_titlearrangement.moc"
