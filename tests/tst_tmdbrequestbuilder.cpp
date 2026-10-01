#include <QtTest>

#include <QByteArray>
#include <QList>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <chrono>

#include "Metadata/TmdbDtos.h"
#include "Metadata/TmdbRequestBuilder.h"
#include "Metadata/TmdbRuntimeConfig.h"

using namespace Makimedia::Tmdb;

namespace {

const QString kV3Key = QStringLiteral("0123456789abcdef0123456789abcdef");
const QString kV4Token =
    QStringLiteral("eyJhbGciOiJIUzI1NiJ9.notarealtoken.signaturepart");

class StubConfig final : public TmdbRuntimeConfig
{
public:
    StubConfig(const QString &baseUrl, const QString &apiKey)
    {
        m_access.baseUrl = QUrl(baseUrl);
        m_access.apiKey = apiKey;
    }

    TmdbRequestAccess requestAccess() const override { return m_access; }

private:
    TmdbRequestAccess m_access;
};

QString valueOf(const QNetworkRequest &request, const QString &key)
{
    return QUrlQuery(request.url()).queryItemValue(
        key, QUrl::FullyDecoded);
}

bool carries(const QNetworkRequest &request, const QString &key)
{
    return QUrlQuery(request.url()).hasQueryItem(key);
}

QList<QNetworkRequest> everyShape(const TmdbRequestBuilder &builder,
                                  TmdbMediaType mediaType)
{
    TmdbDiscoverRequestDto discover;
    discover.mediaType = mediaType;

    TmdbConfigurationRequestDto configuration;

    TmdbGenreListRequestDto genres;
    genres.mediaType = mediaType;

    TmdbTrendingRequestDto trending;
    trending.mediaType = mediaType;

    TmdbPopularRequestDto popular;
    popular.mediaType = mediaType;

    TmdbSearchRequestDto search;
    search.mediaType = mediaType;
    search.query = "the wire";

    TmdbVideosRequestDto videos;
    videos.mediaType = mediaType;
    videos.id = 42;

    TmdbWatchProvidersRequestDto providers;
    providers.mediaType = mediaType;
    providers.id = 42;

    TmdbTitleDetailsRequestDto details;
    details.mediaType = mediaType;
    details.id = 42;

    TmdbSeasonRequestDto season;
    season.showId = 42;
    season.seasonNumber = 3;

    return {
        builder.build(discover),
        builder.build(configuration),
        builder.build(genres),
        builder.build(trending),
        builder.build(popular),
        builder.build(search),
        builder.build(videos),
        builder.build(providers),
        builder.build(details),
        builder.build(season),
    };
}

}

class TestTmdbRequestBuilder : public QObject
{
    Q_OBJECT

private slots:
    void aV3KeyGoesInTheQuery();
    void aV4TokenGoesInAHeaderAndNeverInTheUrl();
    void everyShapeCarriesTheKeyExactlyOneWay_data();
    void everyShapeCarriesTheKeyExactlyOneWay();

    void everyShapeAsksForJsonAndGivesUpAfterFifteenSeconds();

    void theMoviePathsAreWhereTheyShouldBe();
    void theTvPathsAreWhereTheyShouldBe();

    void aBaseWithoutATrailingSlashStillWorks();
    void whateverTheBaseCarriesIsDropped();

    void searchCarriesTheQueryAndThePage();
    void theYearIsNamedForTheMediaType();
    void theYearIsLeftOutWhenThereIsNone();
    void whatTheUserTypedIsEscaped_data();
    void whatTheUserTypedIsEscaped();

    void titleDetailsAsksForTheAgeRatingInTheSameRequest();
    void aCollectionIsAskedForByItsId();

    void discoverTurnsYearsIntoDates();
    void discoverJoinsGenresWithItsMatchMode();
    void discoverLeavesOutWhatWasNotAsked();
};

void TestTmdbRequestBuilder::aV3KeyGoesInTheQuery()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    const QNetworkRequest request = builder.build(TmdbConfigurationRequestDto{});

    QCOMPARE(valueOf(request, QStringLiteral("api_key")), kV3Key);
    QVERIFY(!request.hasRawHeader(QByteArrayLiteral("Authorization")));
}

void TestTmdbRequestBuilder::aV4TokenGoesInAHeaderAndNeverInTheUrl()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV4Token);
    const TmdbRequestBuilder builder(config);

    const QNetworkRequest request = builder.build(TmdbConfigurationRequestDto{});

    QCOMPARE(request.rawHeader(QByteArrayLiteral("Authorization")),
             QByteArrayLiteral("Bearer ") + kV4Token.toUtf8());

    QVERIFY(!carries(request, QStringLiteral("api_key")));
    QVERIFY(!request.url().toString().contains(kV4Token));
    QVERIFY(!request.url().toString().contains(QStringLiteral("eyJ")));
}

void TestTmdbRequestBuilder::everyShapeCarriesTheKeyExactlyOneWay_data()
{
    QTest::addColumn<QString>("apiKey");

    QTest::newRow("a v3 key") << kV3Key;
    QTest::newRow("a v4 token") << kV4Token;
}

void TestTmdbRequestBuilder::everyShapeCarriesTheKeyExactlyOneWay()
{
    QFETCH(QString, apiKey);

    const bool bearer = apiKey.startsWith(QStringLiteral("eyJ"));
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            apiKey);
    const TmdbRequestBuilder builder(config);

    for (const TmdbMediaType mediaType : {TmdbMediaType::Movie,
                                          TmdbMediaType::Tv}) {
        const QList<QNetworkRequest> requests = everyShape(builder, mediaType);
        QCOMPARE(requests.size(), 10);

        for (const QNetworkRequest &request : requests) {
            const QString url = request.url().toString();

            if (bearer) {
                QVERIFY2(!url.contains(apiKey), qPrintable(url));
                QVERIFY2(!carries(request, QStringLiteral("api_key")),
                         qPrintable(url));
                QCOMPARE(request.rawHeader(QByteArrayLiteral("Authorization")),
                         QByteArrayLiteral("Bearer ") + apiKey.toUtf8());
            } else {
                QVERIFY2(carries(request, QStringLiteral("api_key")),
                         qPrintable(url));
                QCOMPARE(valueOf(request, QStringLiteral("api_key")), apiKey);
                QVERIFY2(!request.hasRawHeader(
                             QByteArrayLiteral("Authorization")),
                         qPrintable(url));
            }
        }
    }
}

void TestTmdbRequestBuilder::everyShapeAsksForJsonAndGivesUpAfterFifteenSeconds()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    for (const QNetworkRequest &request : everyShape(builder,
                                                     TmdbMediaType::Movie)) {
        QCOMPARE(request.rawHeader(QByteArrayLiteral("Accept")),
                 QByteArrayLiteral("application/json"));
        QCOMPARE(request.transferTimeoutAsDuration(),
                 std::chrono::milliseconds(15000));
        QCOMPARE(request.attribute(
                     QNetworkRequest::RedirectPolicyAttribute).toInt(),
                 int(QNetworkRequest::NoLessSafeRedirectPolicy));
        QCOMPARE(request.url().scheme(), QStringLiteral("https"));
    }
}

void TestTmdbRequestBuilder::theMoviePathsAreWhereTheyShouldBe()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    const QList<QNetworkRequest> requests =
        everyShape(builder, TmdbMediaType::Movie);

    const QStringList expected = {
        QStringLiteral("/3/discover/movie"),
        QStringLiteral("/3/configuration"),
        QStringLiteral("/3/genre/movie/list"),
        QStringLiteral("/3/trending/movie/week"),
        QStringLiteral("/3/movie/popular"),
        QStringLiteral("/3/search/movie"),
        QStringLiteral("/3/movie/42/videos"),
        QStringLiteral("/3/movie/42/watch/providers"),
        QStringLiteral("/3/movie/42"),
        QStringLiteral("/3/tv/42/season/3"),
    };

    QCOMPARE(requests.size(), expected.size());
    for (qsizetype i = 0; i < requests.size(); ++i) {
        QCOMPARE(requests.at(i).url().path(), expected.at(i));
    }
}

void TestTmdbRequestBuilder::aCollectionIsAskedForByItsId()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbCollectionRequestDto collection;
    collection.id = 264;
    const QNetworkRequest request = builder.build(collection);

    QCOMPARE(request.url().path(), QStringLiteral("/3/collection/264"));
    QCOMPARE(valueOf(request, QStringLiteral("api_key")), kV3Key);
    QCOMPARE(QUrlQuery(request.url()).queryItems().size(), 1);
}

void TestTmdbRequestBuilder::theTvPathsAreWhereTheyShouldBe()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    const QList<QNetworkRequest> requests =
        everyShape(builder, TmdbMediaType::Tv);

    const QStringList expected = {
        QStringLiteral("/3/discover/tv"),
        QStringLiteral("/3/configuration"),
        QStringLiteral("/3/genre/tv/list"),
        QStringLiteral("/3/trending/tv/week"),
        QStringLiteral("/3/tv/popular"),
        QStringLiteral("/3/search/tv"),
        QStringLiteral("/3/tv/42/videos"),
        QStringLiteral("/3/tv/42/watch/providers"),
        QStringLiteral("/3/tv/42"),
        QStringLiteral("/3/tv/42/season/3"),
    };

    QCOMPARE(requests.size(), expected.size());
    for (qsizetype i = 0; i < requests.size(); ++i) {
        QCOMPARE(requests.at(i).url().path(), expected.at(i));
    }
}

void TestTmdbRequestBuilder::aBaseWithoutATrailingSlashStillWorks()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    QCOMPARE(builder.build(TmdbConfigurationRequestDto{}).url().path(),
             QStringLiteral("/3/configuration"));
}

void TestTmdbRequestBuilder::whateverTheBaseCarriesIsDropped()
{
    const StubConfig config(
        QStringLiteral("https://api.themoviedb.org/3/?leftover=1#anchor"),
        kV3Key);
    const TmdbRequestBuilder builder(config);

    const QNetworkRequest request = builder.build(TmdbConfigurationRequestDto{});

    QCOMPARE(request.url().path(), QStringLiteral("/3/configuration"));
    QVERIFY(!carries(request, QStringLiteral("leftover")));
    QVERIFY(!request.url().hasFragment());
}

void TestTmdbRequestBuilder::searchCarriesTheQueryAndThePage()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbSearchRequestDto search;
    search.query = "the wire";
    search.page = 3;

    const QNetworkRequest request = builder.build(search);

    QCOMPARE(valueOf(request, QStringLiteral("query")),
             QStringLiteral("the wire"));
    QCOMPARE(valueOf(request, QStringLiteral("page")), QStringLiteral("3"));
    QCOMPARE(valueOf(request, QStringLiteral("include_adult")),
             QStringLiteral("false"));
}

void TestTmdbRequestBuilder::theYearIsNamedForTheMediaType()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbSearchRequestDto movie;
    movie.mediaType = TmdbMediaType::Movie;
    movie.query = "heat";
    movie.year = 1995;

    const QNetworkRequest movieRequest = builder.build(movie);
    QCOMPARE(valueOf(movieRequest, QStringLiteral("year")),
             QStringLiteral("1995"));
    QVERIFY(!carries(movieRequest, QStringLiteral("first_air_date_year")));

    TmdbSearchRequestDto show;
    show.mediaType = TmdbMediaType::Tv;
    show.query = "the wire";
    show.year = 2002;

    const QNetworkRequest showRequest = builder.build(show);
    QCOMPARE(valueOf(showRequest, QStringLiteral("first_air_date_year")),
             QStringLiteral("2002"));
    QVERIFY(!carries(showRequest, QStringLiteral("year")));
}

void TestTmdbRequestBuilder::theYearIsLeftOutWhenThereIsNone()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbSearchRequestDto unknown;
    unknown.query = "heat";

    QVERIFY(!carries(builder.build(unknown), QStringLiteral("year")));

    TmdbSearchRequestDto zero;
    zero.query = "heat";
    zero.year = 0;
    QVERIFY(!carries(builder.build(zero), QStringLiteral("year")));

    TmdbSearchRequestDto negative;
    negative.query = "heat";
    negative.year = -1;
    QVERIFY(!carries(builder.build(negative), QStringLiteral("year")));
}

void TestTmdbRequestBuilder::whatTheUserTypedIsEscaped_data()
{
    QTest::addColumn<QString>("query");

    QTest::newRow("an ampersand") << QStringLiteral("this & that");
    QTest::newRow("an equals sign") << QStringLiteral("api_key=stolen");
    QTest::newRow("a question mark") << QStringLiteral("who? me");
    QTest::newRow("a hash") << QStringLiteral("track #2");
    QTest::newRow("a plus") << QStringLiteral("rock + roll");
    QTest::newRow("a slash") << QStringLiteral("either/or");
    QTest::newRow("an accent")
        << QStringLiteral("Am") + QChar(0x00E9) + QStringLiteral("lie");
}

void TestTmdbRequestBuilder::whatTheUserTypedIsEscaped()
{
    QFETCH(QString, query);

    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbSearchRequestDto search;
    search.query = query.toStdString();

    const QNetworkRequest request = builder.build(search);

    QCOMPARE(valueOf(request, QStringLiteral("query")), query);
    QCOMPARE(valueOf(request, QStringLiteral("api_key")), kV3Key);
    QCOMPARE(valueOf(request, QStringLiteral("page")), QStringLiteral("1"));
    QCOMPARE(request.url().path(), QStringLiteral("/3/search/movie"));
}

void TestTmdbRequestBuilder::titleDetailsAsksForTheAgeRatingInTheSameRequest()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbTitleDetailsRequestDto movie;
    movie.mediaType = TmdbMediaType::Movie;
    movie.id = 42;
    QCOMPARE(valueOf(builder.build(movie),
                     QStringLiteral("append_to_response")),
             QStringLiteral("release_dates,credits"));

    TmdbTitleDetailsRequestDto show;
    show.mediaType = TmdbMediaType::Tv;
    show.id = 42;
    QCOMPARE(valueOf(builder.build(show),
                     QStringLiteral("append_to_response")),
             QStringLiteral("content_ratings,credits"));
}

void TestTmdbRequestBuilder::discoverTurnsYearsIntoDates()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbDiscoverRequestDto movie;
    movie.mediaType = TmdbMediaType::Movie;
    movie.minimumYear = 1990;
    movie.maximumYear = 1999;

    const QNetworkRequest movieRequest = builder.build(movie);
    QCOMPARE(valueOf(movieRequest,
                     QStringLiteral("primary_release_date.gte")),
             QStringLiteral("1990-01-01"));
    QCOMPARE(valueOf(movieRequest,
                     QStringLiteral("primary_release_date.lte")),
             QStringLiteral("1999-12-31"));

    TmdbDiscoverRequestDto show;
    show.mediaType = TmdbMediaType::Tv;
    show.minimumYear = 1990;
    show.maximumYear = 1999;

    const QNetworkRequest showRequest = builder.build(show);
    QCOMPARE(valueOf(showRequest, QStringLiteral("first_air_date.gte")),
             QStringLiteral("1990-01-01"));
    QCOMPARE(valueOf(showRequest, QStringLiteral("first_air_date.lte")),
             QStringLiteral("1999-12-31"));
}

void TestTmdbRequestBuilder::discoverJoinsGenresWithItsMatchMode()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbDiscoverRequestDto either;
    either.genreIds = {18, 80};
    either.genreMatchMode = TmdbGenreMatchMode::Or;
    QCOMPARE(valueOf(builder.build(either), QStringLiteral("with_genres")),
             QStringLiteral("18|80"));

    TmdbDiscoverRequestDto both;
    both.genreIds = {18, 80};
    both.genreMatchMode = TmdbGenreMatchMode::And;
    QCOMPARE(valueOf(builder.build(both), QStringLiteral("with_genres")),
             QStringLiteral("18,80"));

    TmdbDiscoverRequestDto one;
    one.genreIds = {18};
    QCOMPARE(valueOf(builder.build(one), QStringLiteral("with_genres")),
             QStringLiteral("18"));
}

void TestTmdbRequestBuilder::discoverLeavesOutWhatWasNotAsked()
{
    const StubConfig config(QStringLiteral("https://api.themoviedb.org/3/"),
                            kV3Key);
    const TmdbRequestBuilder builder(config);

    TmdbDiscoverRequestDto bare;
    bare.language.reset();
    bare.originalLanguage.reset();

    const QNetworkRequest request = builder.build(bare);

    QCOMPARE(valueOf(request, QStringLiteral("page")), QStringLiteral("1"));
    QCOMPARE(valueOf(request, QStringLiteral("include_adult")),
             QStringLiteral("false"));

    QVERIFY(!carries(request, QStringLiteral("language")));
    QVERIFY(!carries(request, QStringLiteral("with_original_language")));
    QVERIFY(!carries(request, QStringLiteral("with_genres")));
    QVERIFY(!carries(request, QStringLiteral("vote_average.gte")));
    QVERIFY(!carries(request, QStringLiteral("primary_release_date.gte")));
    QVERIFY(!carries(request, QStringLiteral("primary_release_date.lte")));

    TmdbDiscoverRequestDto empty;
    empty.language = std::string();
    empty.originalLanguage = std::string();
    QVERIFY(!carries(builder.build(empty), QStringLiteral("language")));
    QVERIFY(!carries(builder.build(empty),
                     QStringLiteral("with_original_language")));
}

QTEST_APPLESS_MAIN(TestTmdbRequestBuilder)

#include "tst_tmdbrequestbuilder.moc"
