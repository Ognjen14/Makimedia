#include <QtTest>

#include "Metadata/MatchScorer.h"

using Makimedia::Tmdb::TmdbMediaType;
using Makimedia::Tmdb::TmdbTitleResultDto;

namespace {

ParsedFileName named(const QString &title, int year = 0,
                     int season = 0, int episode = 0)
{
    ParsedFileName parsed;
    parsed.title = title;
    parsed.year = year;
    parsed.season = season;
    parsed.episode = episode;
    return parsed;
}

TmdbTitleResultDto film(const QString &title, const QString &releaseDate = QString())
{
    TmdbTitleResultDto candidate;
    candidate.id = 1;
    candidate.mediaType = TmdbMediaType::Movie;
    candidate.title = title.toStdString();
    candidate.originalTitle = title.toStdString();
    if (!releaseDate.isEmpty()) {
        candidate.releaseDate = releaseDate.toStdString();
    }
    return candidate;
}

TmdbTitleResultDto show(const QString &title, const QString &originalTitle,
                        const QString &releaseDate = QString())
{
    TmdbTitleResultDto candidate;
    candidate.id = 2;
    candidate.mediaType = TmdbMediaType::Tv;
    candidate.title = title.toStdString();
    candidate.originalTitle = originalTitle.toStdString();
    if (!releaseDate.isEmpty()) {
        candidate.releaseDate = releaseDate.toStdString();
    }
    return candidate;
}

QString withAcute(const QString &before, const QString &after)
{
    return before + QChar(0x00E9) + after;
}

}

class TestMatchScorer : public QObject
{
    Q_OBJECT

private slots:
    void normalisesForComparison_data();
    void normalisesForComparison();

    void comparesTitles_data();
    void comparesTitles();

    void rejectsAFilenameThatNamesNothing_data();
    void rejectsAFilenameThatNamesNothing();

    void scoresAgainstWhicheverNameAgrees();
    void scoresANameCarryingBothOfItsNames();

    void scores_data();
    void scores();

    void capsATitleNothingCorroborates();

    void theSuggestionBand_data();
    void theSuggestionBand();

    void thresholds_data();
    void thresholds();
};

void TestMatchScorer::normalisesForComparison_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("normalised");

    QTest::newRow("leading article dropped")
        << QStringLiteral("The Wire") << QStringLiteral("wire");

    QTest::newRow("apostrophe removed")
        << QStringLiteral("Ocean's Eleven") << QStringLiteral("oceans eleven");

    QTest::newRow("roman numeral becomes a digit")
        << QStringLiteral("The Godfather Part II")
        << QStringLiteral("godfather part 2");

    QTest::newRow("punctuation becomes spaces")
        << QStringLiteral("S.W.A.T.") << QStringLiteral("s w a t");

    QTest::newRow("accent dropped")
        << withAcute(QStringLiteral("Am"), QStringLiteral("lie"))
        << QStringLiteral("amelie");

    QTest::newRow("case folded")
        << QStringLiteral("MINDHUNTER") << QStringLiteral("mindhunter");
}

void TestMatchScorer::normalisesForComparison()
{
    QFETCH(QString, title);
    QFETCH(QString, normalised);

    QCOMPARE(MatchScorer::normaliseTitle(title), normalised);
}

void TestMatchScorer::comparesTitles_data()
{
    QTest::addColumn<QString>("left");
    QTest::addColumn<QString>("right");
    QTest::addColumn<int>("similarity");

    QTest::newRow("identical")
        << QStringLiteral("Fight Club") << QStringLiteral("Fight Club") << 100;

    QTest::newRow("same after the article is dropped")
        << QStringLiteral("The Wire") << QStringLiteral("Wire") << 100;

    QTest::newRow("same after the numeral is converted")
        << QStringLiteral("The Godfather Part 2")
        << QStringLiteral("The Godfather Part II") << 100;

    QTest::newRow("an ampersand is the word it stands for")
        << QStringLiteral("Ernest and Celestine")
        << QStringLiteral("Ernest & Celestine") << 100;

    QTest::newRow("and written as an ampersand the other way round")
        << QStringLiteral("Pride & Prejudice")
        << QStringLiteral("Pride and Prejudice") << 100;

    QTest::newRow("a number written as a word")
        << QStringLiteral("The Fantastic Four First Steps")
        << QStringLiteral("The Fantastic 4: First Steps") << 100;

    QTest::newRow("a number written as a word the other way round")
        << QStringLiteral("Oceans 11") << QStringLiteral("Ocean's Eleven") << 100;

    QTest::newRow("one letter identifies nothing")
        << QStringLiteral("A") << QStringLiteral("Anatomy of a Fall") << 0;

    QTest::newRow("one letter that becomes two digits identifies nothing")
        << QStringLiteral("X") << QStringLiteral("X-Men") << 0;

    QTest::newRow("a one letter candidate identifies nothing either")
        << QStringLiteral("X-Men") << QStringLiteral("X") << 0;

    QTest::newRow("one letter that becomes one digit identifies nothing")
        << QStringLiteral("V") << QStringLiteral("V for Vendetta") << 0;

    QTest::newRow("a one letter title still matches itself")
        << QStringLiteral("X") << QStringLiteral("X") << 100;

    QTest::newRow("series name in front of the title")
        << QStringLiteral("Star Wars Return of the Jedi")
        << QStringLiteral("Return of the Jedi") << 78;

    QTest::newRow("a sequel number is the whole difference")
        << QStringLiteral("Rush Hour") << QStringLiteral("Rush Hour 2") << 78;

    QTest::newRow("whole words contained in the middle")
        << QStringLiteral("one two three four")
        << QStringLiteral("two three") << 66;

    QTest::newRow("one shared word out of four")
        << QStringLiteral("The Dark Knight Rises")
        << QStringLiteral("The Dark Tower") << 25;

    QTest::newRow("nothing in common")
        << QStringLiteral("Fight Club") << QStringLiteral("Amelie") << 0;

    QTest::newRow("empty against a title")
        << QString() << QStringLiteral("Fight Club") << 0;
}

void TestMatchScorer::comparesTitles()
{
    QFETCH(QString, left);
    QFETCH(QString, right);
    QFETCH(int, similarity);

    QCOMPARE(MatchScorer::titleSimilarity(left, right), similarity);
}

void TestMatchScorer::rejectsAFilenameThatNamesNothing_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<int>("year");
    QTest::addColumn<bool>("rejected");

    QTest::newRow("a sample clip") << QStringLiteral("sample") << 0 << true;
    QTest::newRow("a bare stem") << QStringLiteral("video") << 0 << true;
    QTest::newRow("a numbered dump") << QStringLiteral("video 01") << 0 << true;
    QTest::newRow("a screen recording")
        << QStringLiteral("recording 3") << 0 << true;
    QTest::newRow("a bare number") << QStringLiteral("01") << 0 << true;
    QTest::newRow("a combined marker") << QStringLiteral("301") << 0 << true;
    QTest::newRow("a date stamp") << QStringLiteral("20240817") << 0 << true;

    QTest::newRow("a numeric title with its own year")
        << QStringLiteral("1917") << 2019 << false;
    QTest::newRow("a numeric title that is also a year")
        << QStringLiteral("2012") << 2009 << false;
    QTest::newRow("an ordinary title")
        << QStringLiteral("Fight Club") << 1999 << false;
}

void TestMatchScorer::rejectsAFilenameThatNamesNothing()
{
    QFETCH(QString, title);
    QFETCH(int, year);
    QFETCH(bool, rejected);

    const MatchScore score = MatchScorer::score(
        named(title, year), film(title, QStringLiteral("2019-01-01")));

    if (rejected) {
        QCOMPARE(score.value, 0);
    } else {
        QVERIFY(score.value > 0);
    }
}

void TestMatchScorer::scoresAgainstWhicheverNameAgrees()
{
    const MatchScore heist = MatchScorer::score(
        named(QStringLiteral("La Casa De Papel"), 0, 1, 1),
        show(QStringLiteral("Money Heist"), QStringLiteral("La Casa de Papel"),
             QStringLiteral("2017-05-02")));

    QCOMPARE(heist.value, 87);
    QVERIFY(heist.isConfident());

    const MatchScore balkans = MatchScorer::score(
        named(QStringLiteral("Senke nad Balkanom"), 0, 1, 1),
        show(QStringLiteral("Shadows over the Balkans"),
             QStringLiteral("Senke nad Balkanom"),
             QStringLiteral("2017-11-05")));

    QCOMPARE(balkans.value, 87);
    QVERIFY(balkans.isConfident());
}

void TestMatchScorer::scoresANameCarryingBothOfItsNames()
{
    const auto heist = show(QStringLiteral("Money Heist"),
                            QStringLiteral("La Casa de Papel"),
                            QStringLiteral("2017-05-02"));

    const MatchScore both = MatchScorer::score(
        named(QStringLiteral("Money Heist La Casa de Papel"), 0, 1, 1), heist);
    QCOMPARE(both.value, 87);
    QVERIFY(both.isConfident());

    const MatchScore reversed = MatchScorer::score(
        named(QStringLiteral("La Casa de Papel Money Heist"), 0, 1, 1), heist);
    QCOMPARE(reversed.value, 87);

    const MatchScore remake = MatchScorer::score(
        named(QStringLiteral("Money Heist Korea"), 0, 1, 1), heist);
    QCOMPARE(remake.value, 71);
    QVERIFY(remake.isSuggestion());
}

void TestMatchScorer::scores_data()
{
    QTest::addColumn<int>("parsedYear");
    QTest::addColumn<int>("season");
    QTest::addColumn<int>("episode");
    QTest::addColumn<bool>("candidateIsTv");
    QTest::addColumn<QString>("candidateDate");
    QTest::addColumn<int>("expected");

    QTest::newRow("film, exact title and exact year")
        << 1999 << 0 << 0 << false << QStringLiteral("1999-10-15") << 100;

    QTest::newRow("film, exact title and no year in the name")
        << 0 << 0 << 0 << false << QStringLiteral("1975-11-19") << 87;

    QTest::newRow("film, year off by one")
        << 2000 << 0 << 0 << false << QStringLiteral("2001-03-04") << 93;

    QTest::newRow("film, year off by two")
        << 2000 << 0 << 0 << false << QStringLiteral("2002-03-04") << 75;

    QTest::newRow("film, year off by five")
        << 2000 << 0 << 0 << false << QStringLiteral("2005-03-04") << 62;

    QTest::newRow("episode against a show")
        << 0 << 1 << 1 << true << QStringLiteral("2008-01-20") << 87;

    QTest::newRow("episode against a film of the same name")
        << 0 << 1 << 1 << false << QString() << 45;
}

void TestMatchScorer::scores()
{
    QFETCH(int, parsedYear);
    QFETCH(int, season);
    QFETCH(int, episode);
    QFETCH(bool, candidateIsTv);
    QFETCH(QString, candidateDate);
    QFETCH(int, expected);

    const QString title = QStringLiteral("Fight Club");
    const TmdbTitleResultDto candidate = candidateIsTv
        ? show(title, title, candidateDate)
        : film(title, candidateDate);

    const MatchScore score =
        MatchScorer::score(named(title, parsedYear, season, episode), candidate);

    QCOMPARE(score.value, expected);
}

void TestMatchScorer::capsATitleNothingCorroborates()
{
    const MatchScore score = MatchScorer::score(
        named(QStringLiteral("Fight Club"), 1999),
        film(QStringLiteral("Fight Club")));

    QCOMPARE(score.value, 79);
    QVERIFY(!score.isConfident());
    QVERIFY(score.isSuggestion());
}

void TestMatchScorer::theSuggestionBand_data()
{
    QTest::addColumn<QString>("parsedTitle");
    QTest::addColumn<int>("parsedYear");
    QTest::addColumn<QString>("candidateTitle");
    QTest::addColumn<QString>("candidateDate");
    QTest::addColumn<int>("expected");

    QTest::newRow("sequel number, no year to separate them")
        << QStringLiteral("Rush Hour") << 0
        << QStringLiteral("Rush Hour 2") << QStringLiteral("2001-08-03") << 71;

    QTest::newRow("series prefix, no year to separate them")
        << QStringLiteral("Star Trek") << 0
        << QStringLiteral("Star Trek Voyager") << QStringLiteral("1995-01-16")
        << 71;

    QTest::newRow("an episode number kept in a film title, no year")
        << QStringLiteral("Star Wars Episode 5") << 0
        << QStringLiteral("Star Wars") << QStringLiteral("1977-05-25") << 71;

    QTest::newRow("exact title, year off by two")
        << QStringLiteral("Fight Club") << 2000
        << QStringLiteral("Fight Club") << QStringLiteral("2002-03-04") << 75;

    QTest::newRow("series prefix, year off by one")
        << QStringLiteral("Star Wars Return of the Jedi") << 1982
        << QStringLiteral("Return of the Jedi") << QStringLiteral("1983-05-25")
        << 77;

    QTest::newRow("partial containment, exact year")
        << QStringLiteral("one two three four") << 2000
        << QStringLiteral("two three") << QStringLiteral("2000-01-01") << 77;

    QTest::newRow("exact title, the name claims a year and TMDB has none")
        << QStringLiteral("Fight Club") << 1999
        << QStringLiteral("Fight Club") << QString() << 79;

    QTest::newRow("series prefix with an agreeing year is already confident")
        << QStringLiteral("Star Wars Return of the Jedi") << 1983
        << QStringLiteral("Return of the Jedi") << QStringLiteral("1983-05-25")
        << 86;
}

void TestMatchScorer::theSuggestionBand()
{
    QFETCH(QString, parsedTitle);
    QFETCH(int, parsedYear);
    QFETCH(QString, candidateTitle);
    QFETCH(QString, candidateDate);
    QFETCH(int, expected);

    const MatchScore score = MatchScorer::score(
        named(parsedTitle, parsedYear), film(candidateTitle, candidateDate));

    QCOMPARE(score.value, expected);
}

void TestMatchScorer::thresholds_data()
{
    QTest::addColumn<int>("value");
    QTest::addColumn<bool>("confident");
    QTest::addColumn<bool>("suggestion");
    QTest::addColumn<bool>("rejected");

    QTest::newRow("at the confident line") << 80 << true << false << false;
    QTest::newRow("one below the confident line") << 79 << false << true << false;
    QTest::newRow("at the suggestion line") << 45 << false << true << false;
    QTest::newRow("one below the suggestion line") << 44 << false << false << true;
    QTest::newRow("nothing at all") << 0 << false << false << true;
    QTest::newRow("perfect") << 100 << true << false << false;
}

void TestMatchScorer::thresholds()
{
    QFETCH(int, value);
    QFETCH(bool, confident);
    QFETCH(bool, suggestion);
    QFETCH(bool, rejected);

    MatchScore score;
    score.value = value;

    QCOMPARE(score.isConfident(), confident);
    QCOMPARE(score.isSuggestion(), suggestion);
    QCOMPARE(score.isRejected(), rejected);
}

QTEST_APPLESS_MAIN(TestMatchScorer)

#include "tst_matchscorer.moc"
