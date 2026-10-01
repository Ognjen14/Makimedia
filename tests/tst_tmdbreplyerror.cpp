#include <QtTest>

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QNetworkReply>
#include <QString>
#include <QTimeZone>

#include <chrono>
#include <optional>

#include "Metadata/TmdbError.h"
#include "Metadata/TmdbReplyError.h"

using Makimedia::Tmdb::TmdbError;
namespace ReplyError = Makimedia::Tmdb::TmdbReplyError;

namespace {

QDateTime aFixedNow()
{
    return QDateTime(QDate(2026, 9, 9), QTime(12, 0, 0), QTimeZone::UTC);
}

std::optional<TmdbError::HttpStatus> http(int status)
{
    return status < 0 ? std::optional<TmdbError::HttpStatus>()
                      : std::optional<TmdbError::HttpStatus>(status);
}

std::optional<TmdbError::TmdbStatusCode> tmdb(int status)
{
    return status < 0 ? std::optional<TmdbError::TmdbStatusCode>()
                      : std::optional<TmdbError::TmdbStatusCode>(status);
}

TmdbError mapped(QNetworkReply::NetworkError networkError,
                 int httpStatus,
                 int tmdbStatus)
{
    return ReplyError::from(networkError, http(httpStatus), tmdb(tmdbStatus),
                            std::nullopt);
}

}

class TestTmdbReplyError : public QObject
{
    Q_OBJECT

private slots:
    void statusCodeIsReadFromTheBody_data();
    void statusCodeIsReadFromTheBody();

    void retryAfterCountsSeconds_data();
    void retryAfterCountsSeconds();
    void retryAfterAcceptsADate();
    void retryAfterNeverGoesBackwards();

    void eachFailureGetsItsCategory_data();
    void eachFailureGetsItsCategory();

    void rateLimitingWinsOverEverythingElse();
    void onlyRateLimitingCarriesTheRetryAfter();
    void theStatusesAreCarriedThrough();
    void everyCategoryExplainsItselfWithoutTheDetails();
};

void TestTmdbReplyError::statusCodeIsReadFromTheBody_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<int>("expected");

    QTest::newRow("a rate limit code")
        << QByteArrayLiteral("{\"status_code\": 25}") << 25;
    QTest::newRow("the success code")
        << QByteArrayLiteral("{\"status_code\": 1}") << 1;
    QTest::newRow("alongside other fields")
        << QByteArrayLiteral("{\"status_message\": \"no\", \"status_code\": 34}")
        << 34;

    QTest::newRow("no body") << QByteArray() << -1;
    QTest::newRow("not json") << QByteArrayLiteral("<html>nope</html>") << -1;
    QTest::newRow("an array, not an object")
        << QByteArrayLiteral("[{\"status_code\": 25}]") << -1;
    QTest::newRow("no such field")
        << QByteArrayLiteral("{\"results\": []}") << -1;
    QTest::newRow("a code that is text")
        << QByteArrayLiteral("{\"status_code\": \"25\"}") << -1;
    QTest::newRow("a code that is null")
        << QByteArrayLiteral("{\"status_code\": null}") << -1;
    QTest::newRow("a code too large to be one")
        << QByteArrayLiteral("{\"status_code\": 99999999999}") << -1;
}

void TestTmdbReplyError::statusCodeIsReadFromTheBody()
{
    QFETCH(QByteArray, body);
    QFETCH(int, expected);

    const auto code = ReplyError::statusCodeIn(body);

    if (expected < 0) {
        QVERIFY(!code.has_value());
    } else {
        QVERIFY(code.has_value());
        QCOMPARE(int(*code), expected);
    }
}

void TestTmdbReplyError::retryAfterCountsSeconds_data()
{
    QTest::addColumn<QByteArray>("header");
    QTest::addColumn<int>("expected");

    QTest::newRow("a plain count") << QByteArrayLiteral("120") << 120;
    QTest::newRow("zero") << QByteArrayLiteral("0") << 0;
    QTest::newRow("padded with spaces") << QByteArrayLiteral("  120  ") << 120;

    QTest::newRow("nothing sent") << QByteArray() << -1;
    QTest::newRow("not a number and not a date")
        << QByteArrayLiteral("soon") << -1;
    QTest::newRow("a negative count")
        << QByteArrayLiteral("-5") << -1;
}

void TestTmdbReplyError::retryAfterCountsSeconds()
{
    QFETCH(QByteArray, header);
    QFETCH(int, expected);

    const auto after = ReplyError::retryAfterFrom(header, aFixedNow());

    if (expected < 0) {
        QVERIFY(!after.has_value());
    } else {
        QVERIFY(after.has_value());
        QCOMPARE(int(after->count()), expected);
    }
}

void TestTmdbReplyError::retryAfterAcceptsADate()
{
    const QDateTime later = aFixedNow().addSecs(90);
    const QByteArray header = later.toString(Qt::RFC2822Date).toLatin1();

    const auto after = ReplyError::retryAfterFrom(header, aFixedNow());

    QVERIFY2(after.has_value(), header.constData());
    QCOMPARE(int(after->count()), 90);
}

void TestTmdbReplyError::retryAfterNeverGoesBackwards()
{
    const QDateTime past = aFixedNow().addSecs(-3600);
    const QByteArray header = past.toString(Qt::RFC2822Date).toLatin1();

    const auto after = ReplyError::retryAfterFrom(header, aFixedNow());

    QVERIFY2(after.has_value(), header.constData());
    QCOMPARE(int(after->count()), 0);
}

void TestTmdbReplyError::eachFailureGetsItsCategory_data()
{
    QTest::addColumn<int>("networkError");
    QTest::addColumn<int>("httpStatus");
    QTest::addColumn<int>("tmdbStatus");
    QTest::addColumn<int>("expected");

    QTest::newRow("too many requests")
        << int(QNetworkReply::NoError) << 429 << -1
        << int(TmdbError::Category::RateLimited);
    QTest::newRow("tmdb says it is limiting")
        << int(QNetworkReply::NoError) << 200 << 25
        << int(TmdbError::Category::RateLimited);

    QTest::newRow("the host is unknown")
        << int(QNetworkReply::HostNotFoundError) << -1 << -1
        << int(TmdbError::Category::Dns);
    QTest::newRow("the proxy is unknown")
        << int(QNetworkReply::ProxyNotFoundError) << -1 << -1
        << int(TmdbError::Category::Dns);

    QTest::newRow("it timed out")
        << int(QNetworkReply::TimeoutError) << -1 << -1
        << int(TmdbError::Category::Timeout);
    QTest::newRow("the proxy timed out")
        << int(QNetworkReply::ProxyTimeoutError) << -1 << -1
        << int(TmdbError::Category::Timeout);

    QTest::newRow("the network went away")
        << int(QNetworkReply::TemporaryNetworkFailureError) << -1 << -1
        << int(TmdbError::Category::Offline);
    QTest::newRow("the session failed")
        << int(QNetworkReply::NetworkSessionFailedError) << -1 << -1
        << int(TmdbError::Category::Offline);

    QTest::newRow("we cancelled it")
        << int(QNetworkReply::OperationCanceledError) << -1 << -1
        << int(TmdbError::Category::Cancelled);

    QTest::newRow("tmdb refused")
        << int(QNetworkReply::NoError) << 404 << 34
        << int(TmdbError::Category::Tmdb);
    QTest::newRow("tmdb refused with a fine status")
        << int(QNetworkReply::NoError) << 200 << 7
        << int(TmdbError::Category::Tmdb);

    QTest::newRow("a plain http failure")
        << int(QNetworkReply::NoError) << 500 << -1
        << int(TmdbError::Category::Http);
    QTest::newRow("a success code alongside an http failure")
        << int(QNetworkReply::NoError) << 404 << 1
        << int(TmdbError::Category::Http);

    QTest::newRow("nothing to go on")
        << int(QNetworkReply::UnknownNetworkError) << -1 << -1
        << int(TmdbError::Category::Network);
    QTest::newRow("connection refused")
        << int(QNetworkReply::ConnectionRefusedError) << -1 << -1
        << int(TmdbError::Category::Network);
    QTest::newRow("an ssl failure")
        << int(QNetworkReply::SslHandshakeFailedError) << -1 << -1
        << int(TmdbError::Category::Network);
}

void TestTmdbReplyError::eachFailureGetsItsCategory()
{
    QFETCH(int, networkError);
    QFETCH(int, httpStatus);
    QFETCH(int, tmdbStatus);
    QFETCH(int, expected);

    const TmdbError error = mapped(
        static_cast<QNetworkReply::NetworkError>(networkError),
        httpStatus, tmdbStatus);

    QCOMPARE(int(error.category()), expected);
}

void TestTmdbReplyError::rateLimitingWinsOverEverythingElse()
{
    QCOMPARE(mapped(QNetworkReply::TimeoutError, 429, -1).category(),
             TmdbError::Category::RateLimited);
    QCOMPARE(mapped(QNetworkReply::HostNotFoundError, -1, 25).category(),
             TmdbError::Category::RateLimited);
    QCOMPARE(mapped(QNetworkReply::OperationCanceledError, 429, -1).category(),
             TmdbError::Category::RateLimited);
}

void TestTmdbReplyError::onlyRateLimitingCarriesTheRetryAfter()
{
    const auto after = std::optional<TmdbError::RetryAfter>(
        std::chrono::seconds(30));

    const TmdbError limited = ReplyError::from(
        QNetworkReply::NoError, http(429), tmdb(-1), after);
    QVERIFY(limited.retryAfter().has_value());
    QCOMPARE(int(limited.retryAfter()->count()), 30);

    const TmdbError timedOut = ReplyError::from(
        QNetworkReply::TimeoutError, http(-1), tmdb(-1), after);
    QVERIFY(!timedOut.retryAfter().has_value());

    const TmdbError refused = ReplyError::from(
        QNetworkReply::NoError, http(404), tmdb(34), after);
    QVERIFY(!refused.retryAfter().has_value());
}

void TestTmdbReplyError::theStatusesAreCarriedThrough()
{
    const TmdbError refused = mapped(QNetworkReply::NoError, 404, 34);
    QVERIFY(refused.httpStatus().has_value());
    QCOMPARE(*refused.httpStatus(), 404);
    QVERIFY(refused.tmdbStatusCode().has_value());
    QCOMPARE(*refused.tmdbStatusCode(), 34);

    const TmdbError plain = mapped(QNetworkReply::NoError, 500, -1);
    QVERIFY(plain.httpStatus().has_value());
    QCOMPARE(*plain.httpStatus(), 500);
    QVERIFY(!plain.tmdbStatusCode().has_value());

    const TmdbError nothing = mapped(QNetworkReply::UnknownNetworkError, -1, -1);
    QVERIFY(!nothing.httpStatus().has_value());
    QVERIFY(!nothing.tmdbStatusCode().has_value());
}

void TestTmdbReplyError::everyCategoryExplainsItselfWithoutTheDetails()
{
    const QList<TmdbError> errors = {
        mapped(QNetworkReply::NoError, 429, -1),
        mapped(QNetworkReply::HostNotFoundError, -1, -1),
        mapped(QNetworkReply::TimeoutError, -1, -1),
        mapped(QNetworkReply::TemporaryNetworkFailureError, -1, -1),
        mapped(QNetworkReply::OperationCanceledError, -1, -1),
        mapped(QNetworkReply::NoError, 404, 34),
        mapped(QNetworkReply::NoError, 500, -1),
        mapped(QNetworkReply::UnknownNetworkError, -1, -1),
    };

    for (const TmdbError &error : errors) {
        const QString context = QString::fromStdString(error.userSafeContext());

        QVERIFY(!context.isEmpty());
        QVERIFY2(context.endsWith(QLatin1Char('.')), qPrintable(context));
        QVERIFY2(!context.contains(QStringLiteral("api_key")),
                 qPrintable(context));
        QVERIFY2(!context.contains(QStringLiteral("http"), Qt::CaseInsensitive),
                 qPrintable(context));
        QVERIFY2(!context.contains(QStringLiteral("QNetworkReply")),
                 qPrintable(context));
    }
}

QTEST_APPLESS_MAIN(TestTmdbReplyError)

#include "tst_tmdbreplyerror.moc"
