#include <QtTest>

#include "Metadata/MatchDecision.h"

using Makimedia::Tmdb::TmdbMediaType;
using Makimedia::Tmdb::TmdbTitleResultDto;

namespace {

ParsedFileName named(const QString &title, int year = 0,
                     const QString &alternativeTitle = QString())
{
    ParsedFileName parsed;
    parsed.title = title;
    parsed.year = year;
    parsed.alternativeTitle = alternativeTitle;
    return parsed;
}

TmdbTitleResultDto film(const QString &title, const QString &releaseDate)
{
    TmdbTitleResultDto candidate;
    candidate.id = 1;
    candidate.mediaType = TmdbMediaType::Movie;
    candidate.title = title.toStdString();
    candidate.originalTitle = title.toStdString();
    candidate.releaseDate = releaseDate.toStdString();
    return candidate;
}

}

class TestMatchDecision : public QObject
{
    Q_OBJECT

private slots:
    void nothingToChooseFrom();
    void oneCandidate();
    void picksTheHighestScore();
    void firstWinsATie();
    void stillChoosesWhenEverythingIsRejected();

    void asksWithoutTheYearFirst();
    void thenTriesTheOtherName();
    void dropsTheYearWhenTheOtherNameIsTheOneCarryingIt();
    void givesUpWithNothingLeftToTry();
    void doesNotRetryAnAlreadyBareName();
    void theChainTerminates_data();
    void theChainTerminates();
};

void TestMatchDecision::nothingToChooseFrom()
{
    const MatchDecision::Choice choice = MatchDecision::pickBest(
        named(QStringLiteral("Fight Club"), 1999), {});

    QVERIFY(choice.isEmpty());
    QCOMPARE(choice.index, -1);
}

void TestMatchDecision::oneCandidate()
{
    const std::vector<TmdbTitleResultDto> candidates = {
        film(QStringLiteral("Fight Club"), QStringLiteral("1999-10-15"))
    };

    const MatchDecision::Choice choice = MatchDecision::pickBest(
        named(QStringLiteral("Fight Club"), 1999), candidates);

    QVERIFY(!choice.isEmpty());
    QCOMPARE(choice.index, 0);
    QCOMPARE(choice.score.value, 100);
}

void TestMatchDecision::picksTheHighestScore()
{
    const std::vector<TmdbTitleResultDto> candidates = {
        film(QStringLiteral("Amelie"), QStringLiteral("2001-04-25")),
        film(QStringLiteral("Fight Club"), QStringLiteral("2005-03-04")),
        film(QStringLiteral("Fight Club"), QStringLiteral("1999-10-15"))
    };

    const MatchDecision::Choice choice = MatchDecision::pickBest(
        named(QStringLiteral("Fight Club"), 1999), candidates);

    QCOMPARE(choice.index, 2);
    QCOMPARE(choice.score.value, 100);
    QVERIFY(choice.score.isConfident());
}

void TestMatchDecision::firstWinsATie()
{
    const std::vector<TmdbTitleResultDto> candidates = {
        film(QStringLiteral("Fight Club"), QStringLiteral("1999-10-15")),
        film(QStringLiteral("Fight Club"), QStringLiteral("1999-11-11"))
    };

    const MatchDecision::Choice choice = MatchDecision::pickBest(
        named(QStringLiteral("Fight Club"), 1999), candidates);

    QCOMPARE(choice.index, 0);
}

void TestMatchDecision::stillChoosesWhenEverythingIsRejected()
{
    const std::vector<TmdbTitleResultDto> candidates = {
        film(QStringLiteral("Sample"), QStringLiteral("2014-01-01")),
        film(QStringLiteral("Sample"), QStringLiteral("2016-01-01"))
    };

    const MatchDecision::Choice choice =
        MatchDecision::pickBest(named(QStringLiteral("sample")), candidates);

    QVERIFY(!choice.isEmpty());
    QCOMPARE(choice.score.value, 0);
    QVERIFY(choice.score.isRejected());
}

void TestMatchDecision::asksWithoutTheYearFirst()
{
    const ParsedFileName parsed =
        named(QStringLiteral("Tvrdjava"), 2025, QStringLiteral("The Fortress"));

    const MatchDecision::Retry retry =
        MatchDecision::askAgainAnotherWay(parsed, true);

    QVERIFY(retry.isWorthTrying());
    QCOMPARE(retry.kind, MatchDecision::Retry::WithoutTheYear);
    QCOMPARE(retry.useYear, false);
    QCOMPARE(retry.parsed.title, QStringLiteral("Tvrdjava"));
    QCOMPARE(retry.parsed.year, 2025);
    QCOMPARE(retry.parsed.alternativeTitle, QStringLiteral("The Fortress"));
}

void TestMatchDecision::thenTriesTheOtherName()
{
    const ParsedFileName parsed = named(QStringLiteral("La Casa De Papel"), 0,
                                        QStringLiteral("Money Heist"));

    const MatchDecision::Retry retry =
        MatchDecision::askAgainAnotherWay(parsed, false);

    QVERIFY(retry.isWorthTrying());
    QCOMPARE(retry.kind, MatchDecision::Retry::TheOtherName);
    QCOMPARE(retry.useYear, true);
    QCOMPARE(retry.parsed.title, QStringLiteral("Money Heist"));
    QVERIFY(retry.parsed.alternativeTitle.isEmpty());
}

void TestMatchDecision::dropsTheYearWhenTheOtherNameIsTheOneCarryingIt()
{
    const MatchDecision::Retry retry = MatchDecision::askAgainAnotherWay(
        named(QStringLiteral("Reply"), 1988, QStringLiteral("Reply 1988")), false);

    QCOMPARE(retry.kind, MatchDecision::Retry::TheOtherName);
    QCOMPARE(retry.parsed.title, QStringLiteral("Reply 1988"));
    QCOMPARE(retry.parsed.year, 0);
    QCOMPARE(retry.useYear, false);

    const MatchDecision::Retry other = MatchDecision::askAgainAnotherWay(
        named(QStringLiteral("La Casa De Papel"), 2017,
              QStringLiteral("Money Heist")), false);

    QCOMPARE(other.parsed.year, 2017);
    QCOMPARE(other.useYear, true);
}

void TestMatchDecision::givesUpWithNothingLeftToTry()
{
    const MatchDecision::Retry retry = MatchDecision::askAgainAnotherWay(
        named(QStringLiteral("Fight Club"), 1999), false);

    QVERIFY(!retry.isWorthTrying());
    QCOMPARE(retry.kind, MatchDecision::Retry::Nothing);
}

void TestMatchDecision::doesNotRetryAnAlreadyBareName()
{
    const MatchDecision::Retry retry =
        MatchDecision::askAgainAnotherWay(named(QStringLiteral("sample")), false);

    QVERIFY(!retry.isWorthTrying());
}

void TestMatchDecision::theChainTerminates_data()
{
    QTest::addColumn<int>("year");
    QTest::addColumn<QString>("alternativeTitle");
    QTest::addColumn<int>("searches");

    QTest::newRow("a year and another name")
        << 2017 << QStringLiteral("Money Heist") << 4;

    QTest::newRow("a year and nothing else")
        << 1999 << QString() << 2;

    QTest::newRow("another name and no year")
        << 0 << QStringLiteral("Money Heist") << 2;

    QTest::newRow("neither")
        << 0 << QString() << 1;
}

void TestMatchDecision::theChainTerminates()
{
    QFETCH(int, year);
    QFETCH(QString, alternativeTitle);
    QFETCH(int, searches);

    ParsedFileName parsed =
        named(QStringLiteral("La Casa De Papel"), year, alternativeTitle);
    bool useYear = true;
    int attempts = 1;

    forever {
        const bool usedYear = useYear && parsed.year > 0;
        const MatchDecision::Retry retry =
            MatchDecision::askAgainAnotherWay(parsed, usedYear);

        if (!retry.isWorthTrying()) {
            break;
        }

        parsed = retry.parsed;
        useYear = retry.useYear;
        ++attempts;

        QVERIFY2(attempts <= 8, "the retry chain did not terminate");
    }

    QCOMPARE(attempts, searches);
}

QTEST_APPLESS_MAIN(TestMatchDecision)

#include "tst_matchdecision.moc"
