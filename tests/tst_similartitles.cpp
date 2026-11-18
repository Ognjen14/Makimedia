#include <QtTest>

#include "Library/SimilarTitles.h"

namespace {

MediaRecord titled(qint64 id, const QString &title, const QString &genres, int year = 2010,
                   const QString &kind = QStringLiteral("movie"))
{
    MediaRecord record;
    record.id = id;
    record.tmdbId = id;
    record.kind = kind;
    record.title = title;
    record.genres = genres;
    record.year = year;
    record.fileCount = 1;
    return record;
}

QStringList titleNames(const QList<MediaRecord> &titles)
{
    QStringList names;
    for (const MediaRecord &title : titles) {
        names.append(title.title);
    }
    return names;
}

}

class TestSimilarTitles : public QObject
{
    Q_OBJECT

private slots:
    void sharedGenresMakeATitleSimilar();
    void titlesWithNothingInCommonAreLeftOut();
    void theNextFilmInTheCollectionComesFirst();
    void theSameDirectorCountsWithoutSharedGenres();
    void watchedAndStartedTitlesAreLeftOut();
    void theRowStopsAtTheLimit();
};

void TestSimilarTitles::sharedGenresMakeATitleSimilar()
{
    const MediaRecord seed = titled(1, QStringLiteral("Seed"), QStringLiteral("Action, Thriller"));
    const QList<MediaRecord> titles = {
        seed,
        titled(2, QStringLiteral("Half"), QStringLiteral("Action, Comedy")),
        titled(3, QStringLiteral("Same"), QStringLiteral("Action, Thriller")),
    };

    QCOMPARE(titleNames(SimilarTitles::similarTo(seed, titles, {}, 10)),
             QStringList({QStringLiteral("Same"), QStringLiteral("Half")}));
}

void TestSimilarTitles::titlesWithNothingInCommonAreLeftOut()
{
    const MediaRecord seed = titled(1, QStringLiteral("Seed"), QStringLiteral("Horror"));
    const QList<MediaRecord> titles = {
        seed,
        titled(2, QStringLiteral("Comedy"), QStringLiteral("Comedy"), 2010),
    };

    QVERIFY(SimilarTitles::similarTo(seed, titles, {}, 10).isEmpty());
}

void TestSimilarTitles::theNextFilmInTheCollectionComesFirst()
{
    const MediaRecord seed = titled(1, QStringLiteral("Part one"), QStringLiteral("Action"));
    const QList<MediaRecord> titles = {
        seed,
        titled(2, QStringLiteral("Other action"), QStringLiteral("Action")),
        titled(3, QStringLiteral("Part two"), QStringLiteral("Action, Adventure")),
    };

    SimilarTitles::Known known;
    known.collections = {{1, 77}, {3, 77}};

    QCOMPARE(SimilarTitles::similarTo(seed, titles, known, 10).first().title,
             QStringLiteral("Part two"));
}

void TestSimilarTitles::theSameDirectorCountsWithoutSharedGenres()
{
    const MediaRecord seed = titled(1, QStringLiteral("Seed"), QStringLiteral("Drama"));
    const QList<MediaRecord> titles = {
        seed,
        titled(2, QStringLiteral("Same director"), QStringLiteral("Comedy")),
    };

    SimilarTitles::Known known;
    known.people.insert(1, {{QStringLiteral("A Director")}, {}});
    known.people.insert(2, {{QStringLiteral("A Director")}, {}});

    QCOMPARE(titleNames(SimilarTitles::similarTo(seed, titles, known, 10)),
             QStringList({QStringLiteral("Same director")}));
}

void TestSimilarTitles::watchedAndStartedTitlesAreLeftOut()
{
    const MediaRecord seed = titled(1, QStringLiteral("Seed"), QStringLiteral("Drama"));
    MediaRecord watched = titled(2, QStringLiteral("Watched"), QStringLiteral("Drama"));
    watched.watchedCount = 1;
    MediaRecord started = titled(3, QStringLiteral("Started"), QStringLiteral("Drama"));
    started.partialProgress = 0.4;
    const QList<MediaRecord> titles = {
        seed, watched, started, titled(4, QStringLiteral("Fresh"), QStringLiteral("Drama")),
    };

    QCOMPARE(titleNames(SimilarTitles::similarTo(seed, titles, {}, 10)),
             QStringList({QStringLiteral("Fresh")}));
}

void TestSimilarTitles::theRowStopsAtTheLimit()
{
    const MediaRecord seed = titled(1, QStringLiteral("Seed"), QStringLiteral("Drama"));
    QList<MediaRecord> titles = {seed};
    for (int i = 2; i < 30; ++i) {
        titles.append(titled(i, QStringLiteral("Drama %1").arg(i), QStringLiteral("Drama")));
    }

    QCOMPARE(SimilarTitles::similarTo(seed, titles, {}, 20).size(), 20);
}

QTEST_GUILESS_MAIN(TestSimilarTitles)
#include "tst_similartitles.moc"
