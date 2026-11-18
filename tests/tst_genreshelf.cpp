#include <QtTest>

#include "Library/GenreShelf.h"

namespace {

MediaRecord titled(qint64 id, const QString &kind, const QString &title,
                   const QString &genres)
{
    MediaRecord record;
    record.id = id;
    record.tmdbId = id;
    record.kind = kind;
    record.title = title;
    record.genres = genres;
    return record;
}

QStringList genreNames(const QList<GenreShelf::Row> &rows)
{
    QStringList names;
    for (const GenreShelf::Row &row : rows) {
        names.append(row.genre);
    }
    return names;
}

QStringList titleNames(const GenreShelf::Row &row)
{
    QStringList names;
    for (const MediaRecord &title : row.titles) {
        names.append(title.title);
    }
    return names;
}

QList<MediaRecord> manyGenres()
{
    const QStringList genres = {
        QStringLiteral("Action"), QStringLiteral("Comedy"), QStringLiteral("Drama"),
        QStringLiteral("Horror"), QStringLiteral("Thriller"), QStringLiteral("Romance"),
        QStringLiteral("Crime"), QStringLiteral("Animation"), QStringLiteral("Mystery"),
        QStringLiteral("Family"),
    };
    QList<MediaRecord> titles;
    qint64 id = 1;
    for (const QString &genre : genres) {
        for (int i = 0; i < 8; ++i, ++id) {
            MediaRecord title = titled(id, QStringLiteral("movie"),
                                       QStringLiteral("%1 %2").arg(genre).arg(i), genre);
            title.fileCount = 1;
            title.rating = double(id % 10);
            titles.append(title);
        }
    }
    return titles;
}

}

class TestGenreShelf : public QObject
{
    Q_OBJECT

private slots:
    void filmAndShowGenresShareOneName_data();
    void filmAndShowGenresShareOneName();

    void aRowMixesFilmsAndShows();
    void theBiggestGenreComesFirstAndSmallOnesHaveNoRow();
    void theNewestAdditionComesFirstInARow();

    void aDayKeepsTheSameRowsAndOrder();
    void aDayShowsOnlySomeGenres();
    void anotherDayChangesTheRows();
    void finishedTitlesGoToTheBackOfADailyRow();
    void shortOnTimeKeepsShortUnwatchedFilms();
};

void TestGenreShelf::filmAndShowGenresShareOneName_data()
{
    QTest::addColumn<QString>("stored");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("film genres pass through")
        << QStringLiteral("Action, Science Fiction")
        << QStringList({QStringLiteral("Action"), QStringLiteral("Science Fiction")});
    QTest::newRow("a show's action and adventure is both")
        << QStringLiteral("Action & Adventure, Drama")
        << QStringList({QStringLiteral("Action"), QStringLiteral("Adventure"),
                        QStringLiteral("Drama")});
    QTest::newRow("a show's sci-fi and fantasy is both")
        << QStringLiteral("Sci-Fi & Fantasy")
        << QStringList({QStringLiteral("Science Fiction"), QStringLiteral("Fantasy")});
    QTest::newRow("war and politics is war")
        << QStringLiteral("War & Politics")
        << QStringList({QStringLiteral("War")});
    QTest::newRow("kids is family")
        << QStringLiteral("Kids, Family")
        << QStringList({QStringLiteral("Family")});
    QTest::newRow("tv movie is no genre")
        << QStringLiteral("TV Movie, Comedy")
        << QStringList({QStringLiteral("Comedy")});
    QTest::newRow("no genres") << QString() << QStringList();
}

void TestGenreShelf::filmAndShowGenresShareOneName()
{
    QFETCH(QString, stored);
    QFETCH(QStringList, expected);

    QCOMPARE(GenreShelf::genresOf(stored), expected);
}

void TestGenreShelf::aRowMixesFilmsAndShows()
{
    const QList<MediaRecord> titles = {
        titled(1, QStringLiteral("movie"), QStringLiteral("Heat"), QStringLiteral("Action, Crime")),
        titled(2, QStringLiteral("tv"), QStringLiteral("Vikings"),
               QStringLiteral("Action & Adventure, Drama")),
    };

    const QList<GenreShelf::Row> rows = GenreShelf::build(titles, {}, 1);
    const qsizetype action = genreNames(rows).indexOf(QStringLiteral("Action"));

    QVERIFY(action >= 0);
    QCOMPARE(rows.at(action).titles.size(), 2);
}

void TestGenreShelf::theBiggestGenreComesFirstAndSmallOnesHaveNoRow()
{
    const QList<MediaRecord> titles = {
        titled(1, QStringLiteral("movie"), QStringLiteral("A"), QStringLiteral("Drama")),
        titled(2, QStringLiteral("movie"), QStringLiteral("B"), QStringLiteral("Drama, Comedy")),
        titled(3, QStringLiteral("movie"), QStringLiteral("C"), QStringLiteral("Drama, Comedy")),
        titled(4, QStringLiteral("movie"), QStringLiteral("D"), QStringLiteral("Horror")),
    };

    QCOMPARE(genreNames(GenreShelf::build(titles, {}, 2)),
             QStringList({QStringLiteral("Drama"), QStringLiteral("Comedy")}));
}

void TestGenreShelf::theNewestAdditionComesFirstInARow()
{
    const QList<MediaRecord> titles = {
        titled(1, QStringLiteral("movie"), QStringLiteral("Old"), QStringLiteral("Drama")),
        titled(2, QStringLiteral("tv"), QStringLiteral("Newest"), QStringLiteral("Drama")),
        titled(3, QStringLiteral("movie"), QStringLiteral("Middle"), QStringLiteral("Drama")),
        titled(4, QStringLiteral("movie"), QStringLiteral("Never added"), QStringLiteral("Drama")),
    };
    const QHash<qint64, qint64> added = {{1, 100}, {2, 300}, {3, 200}};

    const QList<GenreShelf::Row> rows = GenreShelf::build(titles, added, 1);

    QCOMPARE(rows.size(), 1);
    QCOMPARE(titleNames(rows.first()),
             QStringList({QStringLiteral("Newest"), QStringLiteral("Middle"),
                          QStringLiteral("Old"), QStringLiteral("Never added")}));
}

void TestGenreShelf::aDayKeepsTheSameRowsAndOrder()
{
    const QList<MediaRecord> titles = manyGenres();
    const QDate day(2026, 11, 18);

    const QList<GenreShelf::Row> first = GenreShelf::daily(titles, {}, 6, 6, day);
    const QList<GenreShelf::Row> again = GenreShelf::daily(titles, {}, 6, 6, day);

    QCOMPARE(genreNames(again), genreNames(first));
    for (int i = 0; i < first.size(); ++i) {
        QCOMPARE(titleNames(again.at(i)), titleNames(first.at(i)));
    }
}

void TestGenreShelf::aDayShowsOnlySomeGenres()
{
    const QList<GenreShelf::Row> rows =
        GenreShelf::daily(manyGenres(), {}, 6, 6, QDate(2026, 11, 18));
    QCOMPARE(rows.size(), 6);
}

void TestGenreShelf::anotherDayChangesTheRows()
{
    const QList<MediaRecord> titles = manyGenres();
    const QList<GenreShelf::Row> today = GenreShelf::daily(titles, {}, 6, 6, QDate(2026, 11, 18));

    bool changed = false;
    for (int offset = 1; offset <= 7 && !changed; ++offset) {
        const QList<GenreShelf::Row> other =
            GenreShelf::daily(titles, {}, 6, 6, QDate(2026, 11, 18).addDays(offset));
        changed = genreNames(other) != genreNames(today)
                  || titleNames(other.first()) != titleNames(today.first());
    }
    QVERIFY(changed);
}

void TestGenreShelf::finishedTitlesGoToTheBackOfADailyRow()
{
    QList<MediaRecord> titles;
    for (int i = 1; i <= 8; ++i) {
        MediaRecord title = titled(i, QStringLiteral("movie"), QStringLiteral("Film %1").arg(i),
                                   QStringLiteral("Drama"));
        title.fileCount = 1;
        title.rating = 9.0;
        if (i <= 3) {
            title.watchedCount = 1;
        }
        titles.append(title);
    }

    const QList<GenreShelf::Row> rows = GenreShelf::daily(titles, {}, 6, 6, QDate(2026, 11, 18));
    QCOMPARE(rows.size(), 1);
    const QList<MediaRecord> &row = rows.first().titles;
    for (int i = 0; i < 5; ++i) {
        QVERIFY(!GenreShelf::finished(row.at(i)));
    }
    for (int i = 5; i < 8; ++i) {
        QVERIFY(GenreShelf::finished(row.at(i)));
    }
}

void TestGenreShelf::shortOnTimeKeepsShortUnwatchedFilms()
{
    auto film = [](qint64 id, const QString &name, int minutes) {
        MediaRecord title = titled(id, QStringLiteral("movie"), name, QStringLiteral("Drama"));
        title.fileCount = 1;
        title.runtimeMinutes = minutes;
        return title;
    };

    MediaRecord watched = film(4, QStringLiteral("Watched"), 90);
    watched.watchedCount = 1;
    MediaRecord started = film(5, QStringLiteral("Started"), 90);
    started.partialProgress = 0.3;
    MediaRecord show = titled(6, QStringLiteral("tv"), QStringLiteral("Show"),
                              QStringLiteral("Drama"));
    show.runtimeMinutes = 30;
    show.fileCount = 1;

    const QList<MediaRecord> titles = {
        film(1, QStringLiteral("Short"), 85),
        film(2, QStringLiteral("Exactly"), 100),
        film(3, QStringLiteral("Long"), 101),
        watched, started, show,
        film(7, QStringLiteral("Unknown"), 0),
    };

    QStringList names = titleNames(
        {QString(), GenreShelf::shortOnTime(titles, 100, 30, QDate(2026, 11, 18)), QString()});
    names.sort();
    QCOMPARE(names, QStringList({QStringLiteral("Exactly"), QStringLiteral("Short")}));
}

QTEST_GUILESS_MAIN(TestGenreShelf)
#include "tst_genreshelf.moc"
