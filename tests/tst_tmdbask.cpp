#include <QtTest>

#include "Metadata/FileNameParser.h"
#include "Metadata/TmdbAsk.h"

using TmdbAsk::Ledger;
using TmdbAsk::Verdict;

namespace {

const QString kKey = QStringLiteral("tv:fargo|0");
const QString kOtherKey = QStringLiteral("movie:dune|2021");

ParsedFileName movie(const QString &title, int year)
{
    ParsedFileName parsed;
    parsed.title = title;
    parsed.year = year;
    return parsed;
}

ParsedFileName episode(const QString &title, int season, int number)
{
    ParsedFileName parsed;
    parsed.title = title;
    parsed.season = season;
    parsed.episode = number;
    return parsed;
}

}

class TestTmdbAsk : public QObject
{
    Q_OBJECT

private slots:
    void aSecondAskWaitsForTheFirst();
    void theAnswerIsHandedToEveryWaiterOnce();
    void anAnsweredKeyIsNotAskedAgain();
    void aFailureReleasesTheWaitersAndStaysAskable();
    void aNotFoundIsAnAnswer();
    void onlyNotFoundIsDefinitive_data();
    void onlyNotFoundIsDefinitive();
    void forgettingAnswersKeepsWhatIsInFlight();
    void keysDoNotInterfere();
    void aLedgerWithNothingToCarryStillCounts();

    void searchKeyIgnoresCaseAndAnUnusedYear();
    void searchKeySeparatesMoviesFromShows();
    void detailsAndSeasonKeysNameWhatTheyAsk();
};

void TestTmdbAsk::aSecondAskWaitsForTheFirst()
{
    Ledger<QString, int> ledger;

    QVERIFY(ledger.ask(kKey, 1) == Verdict::AskNow);
    QVERIFY(ledger.isAsking(kKey));
    QVERIFY(ledger.ask(kKey, 2) == Verdict::AlreadyAsking);
    QVERIFY(!ledger.isAnswered(kKey));
}

void TestTmdbAsk::theAnswerIsHandedToEveryWaiterOnce()
{
    Ledger<QString, int> ledger;
    ledger.ask(kKey, 1);
    ledger.ask(kKey, 2);

    QCOMPARE(ledger.answer(kKey, QStringLiteral("found")), QList<int>({1, 2}));
    QVERIFY(!ledger.isAsking(kKey));
    QCOMPARE(ledger.answer(kKey, QStringLiteral("again")), QList<int>());
}

void TestTmdbAsk::anAnsweredKeyIsNotAskedAgain()
{
    Ledger<QString, int> ledger;
    ledger.ask(kKey, 1);
    ledger.answer(kKey, QStringLiteral("found"));

    QVERIFY(ledger.ask(kKey, 3) == Verdict::AlreadyAnswered);
    QCOMPARE(ledger.answerFor(kKey), QStringLiteral("found"));
    QVERIFY(!ledger.isAsking(kKey));
}

void TestTmdbAsk::aFailureReleasesTheWaitersAndStaysAskable()
{
    Ledger<QString, int> ledger;
    ledger.ask(kKey, 1);
    ledger.ask(kKey, 2);

    QCOMPARE(ledger.fail(kKey, 503), QList<int>({1, 2}));
    QVERIFY(!ledger.isAnswered(kKey));
    QVERIFY(!ledger.isAsking(kKey));
    QVERIFY(ledger.ask(kKey, 3) == Verdict::AskNow);

    QCOMPARE(ledger.fail(kKey, std::nullopt), QList<int>({3}));
    QVERIFY(ledger.ask(kKey, 4) == Verdict::AskNow);
}

void TestTmdbAsk::aNotFoundIsAnAnswer()
{
    Ledger<QString, int> ledger;
    ledger.ask(kKey, 1);

    QCOMPARE(ledger.fail(kKey, 404), QList<int>({1}));
    QVERIFY(ledger.isAnswered(kKey));
    QVERIFY(ledger.ask(kKey, 2) == Verdict::AlreadyAnswered);
    QVERIFY(ledger.answerFor(kKey).isEmpty());
}

void TestTmdbAsk::onlyNotFoundIsDefinitive_data()
{
    QTest::addColumn<bool>("hasStatus");
    QTest::addColumn<int>("status");
    QTest::addColumn<bool>("definitive");

    QTest::newRow("no reply at all") << false << 0 << false;
    QTest::newRow("not found") << true << 404 << true;
    QTest::newRow("bad request") << true << 400 << false;
    QTest::newRow("unauthorised") << true << 401 << false;
    QTest::newRow("rate limited") << true << 429 << false;
    QTest::newRow("server error") << true << 500 << false;
    QTest::newRow("unavailable") << true << 503 << false;
}

void TestTmdbAsk::onlyNotFoundIsDefinitive()
{
    QFETCH(bool, hasStatus);
    QFETCH(int, status);
    QFETCH(bool, definitive);

    const std::optional<int> httpStatus =
        hasStatus ? std::optional<int>(status) : std::nullopt;
    QCOMPARE(TmdbAsk::isDefinitive(httpStatus), definitive);
}

void TestTmdbAsk::forgettingAnswersKeepsWhatIsInFlight()
{
    Ledger<QString, int> ledger;
    ledger.ask(kKey, 1);
    ledger.answer(kKey, QStringLiteral("found"));
    ledger.ask(kOtherKey, 2);

    ledger.forgetAnswers();

    QVERIFY(ledger.ask(kKey, 3) == Verdict::AskNow);
    QVERIFY(ledger.ask(kOtherKey, 4) == Verdict::AlreadyAsking);
}

void TestTmdbAsk::keysDoNotInterfere()
{
    Ledger<QString, int> ledger;

    QVERIFY(ledger.ask(kKey, 1) == Verdict::AskNow);
    QVERIFY(ledger.ask(kOtherKey, 2) == Verdict::AskNow);

    QCOMPARE(ledger.answer(kKey, QStringLiteral("found")), QList<int>({1}));
    QVERIFY(ledger.ask(kOtherKey, 3) == Verdict::AlreadyAsking);
    QCOMPARE(ledger.fail(kOtherKey, 500), QList<int>({2, 3}));
    QVERIFY(ledger.ask(kKey, 4) == Verdict::AlreadyAnswered);
}

void TestTmdbAsk::aLedgerWithNothingToCarryStillCounts()
{
    Ledger<> ledger;
    const QString season = TmdbAsk::seasonKey(1399, 2);

    QVERIFY(ledger.ask(season) == Verdict::AskNow);
    QVERIFY(ledger.ask(season) == Verdict::AlreadyAsking);
    QCOMPARE(ledger.answer(season).size(), 2);
    QVERIFY(ledger.ask(season) == Verdict::AlreadyAnswered);
}

void TestTmdbAsk::searchKeyIgnoresCaseAndAnUnusedYear()
{
    QCOMPARE(TmdbAsk::searchKey(movie(QStringLiteral("Dune"), 2021), true),
             TmdbAsk::searchKey(movie(QStringLiteral("dUNE"), 2021), true));

    QCOMPARE(TmdbAsk::searchKey(movie(QStringLiteral("Dune"), 2021), false),
             TmdbAsk::searchKey(movie(QStringLiteral("Dune"), 0), true));

    QVERIFY(TmdbAsk::searchKey(movie(QStringLiteral("Dune"), 2021), true)
            != TmdbAsk::searchKey(movie(QStringLiteral("Dune"), 1984), true));
}

void TestTmdbAsk::searchKeySeparatesMoviesFromShows()
{
    QVERIFY(TmdbAsk::searchKey(movie(QStringLiteral("Fargo"), 0), true)
            != TmdbAsk::searchKey(episode(QStringLiteral("Fargo"), 1, 1), true));

    QCOMPARE(TmdbAsk::searchKey(episode(QStringLiteral("Fargo"), 1, 1), true),
             TmdbAsk::searchKey(episode(QStringLiteral("Fargo"), 2, 5), true));
}

void TestTmdbAsk::detailsAndSeasonKeysNameWhatTheyAsk()
{
    QVERIFY(TmdbAsk::detailsKey(1399, QStringLiteral("tv"))
            != TmdbAsk::detailsKey(1399, QStringLiteral("movie")));
    QVERIFY(TmdbAsk::detailsKey(1399, QStringLiteral("tv"))
            != TmdbAsk::detailsKey(1400, QStringLiteral("tv")));

    QVERIFY(TmdbAsk::seasonKey(1399, 1) != TmdbAsk::seasonKey(1399, 2));
    QVERIFY(TmdbAsk::seasonKey(1399, 1) != TmdbAsk::seasonKey(1400, 1));
}

QTEST_APPLESS_MAIN(TestTmdbAsk)

#include "tst_tmdbask.moc"
