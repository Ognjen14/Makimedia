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

QTEST_GUILESS_MAIN(TestGenreShelf)
#include "tst_genreshelf.moc"
