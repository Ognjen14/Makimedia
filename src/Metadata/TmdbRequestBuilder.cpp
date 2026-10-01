#include "Metadata/TmdbRequestBuilder.h"

#include <QByteArray>
#include <QUrlQuery>

namespace Makimedia::Tmdb
{
namespace
{

constexpr int RequestTransferTimeoutMs = 15000;

QString discoverPath(TmdbMediaType mediaType)
{
    switch (mediaType)
    {
    case TmdbMediaType::Movie:
        return QStringLiteral("discover/movie");
    case TmdbMediaType::Tv:
        return QStringLiteral("discover/tv");
    }

    return QStringLiteral("discover/movie");
}

QUrl endpointUrl(const QUrl &baseUrl, const QString &relativePath)
{
    QUrl url(baseUrl);
    QString path = url.path();

    if (!path.endsWith(QLatin1Char('/')))
    {
        path.append(QLatin1Char('/'));
    }

    path.append(relativePath);
    url.setPath(path);
    url.setQuery(QString{});
    url.setFragment(QString{});
    return url;
}

QString firstDayOfYear(int year)
{
    return QString::number(year) + QStringLiteral("-01-01");
}

QString lastDayOfYear(int year)
{
    return QString::number(year) + QStringLiteral("-12-31");
}

QString genreQueryValue(const TmdbDiscoverRequestDto &request)
{
    const QChar separator = request.genreMatchMode == TmdbGenreMatchMode::And
        ? QLatin1Char(',')
        : QLatin1Char('|');
    QString value;

    for (const TmdbGenreId genreId : request.genreIds)
    {
        if (!value.isEmpty())
        {
            value.append(separator);
        }
        value.append(QString::number(genreId));
    }

    return value;
}

QNetworkRequest jsonGetRequest(QUrl url,
                               const QString &apiKey,
                               QUrlQuery query = {})
{
    const bool usesBearerToken = apiKey.startsWith(QStringLiteral("eyJ"));
    if (!usesBearerToken)
    {
        query.addQueryItem(QStringLiteral("api_key"), apiKey);
    }
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    if (usesBearerToken)
    {
        request.setRawHeader(QByteArrayLiteral("Authorization"),
                             QByteArrayLiteral("Bearer ") + apiKey.toUtf8());
    }
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(RequestTransferTimeoutMs);
    return request;
}

}

TmdbRequestBuilder::TmdbRequestBuilder(const TmdbRuntimeConfig &runtimeConfig) noexcept
    : m_runtimeConfig(runtimeConfig)
{
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbDiscoverRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(request.page));
    query.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));

    if (request.language.has_value() && !request.language->empty())
    {
        query.addQueryItem(QStringLiteral("language"),
                           QString::fromStdString(*request.language));
    }

    if (request.minimumYear.has_value())
    {
        const QString key = request.mediaType == TmdbMediaType::Movie
            ? QStringLiteral("primary_release_date.gte")
            : QStringLiteral("first_air_date.gte");
        query.addQueryItem(key, firstDayOfYear(*request.minimumYear));
    }
    if (request.maximumYear.has_value())
    {
        const QString key = request.mediaType == TmdbMediaType::Movie
            ? QStringLiteral("primary_release_date.lte")
            : QStringLiteral("first_air_date.lte");
        query.addQueryItem(key, lastDayOfYear(*request.maximumYear));
    }
    if (request.minimumRating > 0.0)
    {
        query.addQueryItem(QStringLiteral("vote_average.gte"),
                           QString::number(request.minimumRating));
    }
    if (!request.genreIds.empty())
    {
        query.addQueryItem(QStringLiteral("with_genres"),
                           genreQueryValue(request));
    }
    if (request.originalLanguage.has_value() && !request.originalLanguage->empty())
    {
        query.addQueryItem(QStringLiteral("with_original_language"),
                           QString::fromStdString(*request.originalLanguage));
    }

    return jsonGetRequest(endpointUrl(access.baseUrl, discoverPath(request.mediaType)),
                          access.apiKey,
                          query);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbConfigurationRequestDto &) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    return jsonGetRequest(endpointUrl(access.baseUrl, QStringLiteral("configuration")),
                          access.apiKey);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbGenreListRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const QString path = request.mediaType == TmdbMediaType::Movie
        ? QStringLiteral("genre/movie/list")
        : QStringLiteral("genre/tv/list");
    QUrlQuery query;
    if (request.language.has_value() && !request.language->empty())
    {
        query.addQueryItem(QStringLiteral("language"),
                           QString::fromStdString(*request.language));
    }
    return jsonGetRequest(endpointUrl(access.baseUrl, path),
                          access.apiKey,
                          query);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbTrendingRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const QString path = request.mediaType == TmdbMediaType::Movie
        ? QStringLiteral("trending/movie/week")
        : QStringLiteral("trending/tv/week");
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(request.page));
    query.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));
    return jsonGetRequest(endpointUrl(access.baseUrl, path),
                          access.apiKey,
                          query);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbPopularRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const QString path = request.mediaType == TmdbMediaType::Movie
        ? QStringLiteral("movie/popular")
        : QStringLiteral("tv/popular");
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(request.page));
    query.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));
    return jsonGetRequest(endpointUrl(access.baseUrl, path),
                          access.apiKey,
                          query);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbSearchRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const QString path = request.mediaType == TmdbMediaType::Movie
        ? QStringLiteral("search/movie")
        : QStringLiteral("search/tv");
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("query"),
                       QString::fromStdString(request.query));
    query.addQueryItem(QStringLiteral("page"), QString::number(request.page));
    query.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));

    if (request.year.has_value() && *request.year > 0) {
        query.addQueryItem(request.mediaType == TmdbMediaType::Movie
                               ? QStringLiteral("year")
                               : QStringLiteral("first_air_date_year"),
                           QString::number(*request.year));
    }

    return jsonGetRequest(endpointUrl(access.baseUrl, path),
                          access.apiKey,
                          query);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbVideosRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const QString path = (request.mediaType == TmdbMediaType::Movie
        ? QStringLiteral("movie/%1/videos")
        : QStringLiteral("tv/%1/videos")).arg(request.id);
    QUrlQuery query;
    if (request.language.has_value() && !request.language->empty())
    {
        query.addQueryItem(QStringLiteral("language"),
                           QString::fromStdString(*request.language));
    }
    return jsonGetRequest(endpointUrl(access.baseUrl, path),
                          access.apiKey,
                          query);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbWatchProvidersRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const QString path = (request.mediaType == TmdbMediaType::Movie
        ? QStringLiteral("movie/%1/watch/providers")
        : QStringLiteral("tv/%1/watch/providers")).arg(request.id);
    return jsonGetRequest(endpointUrl(access.baseUrl, path), access.apiKey);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbTitleDetailsRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const bool movie = request.mediaType == TmdbMediaType::Movie;
    const QString path = (movie ? QStringLiteral("movie/%1")
                                : QStringLiteral("tv/%1")).arg(request.id);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("append_to_response"),
                       movie ? QStringLiteral("release_dates,credits")
                             : QStringLiteral("content_ratings,credits"));

    return jsonGetRequest(endpointUrl(access.baseUrl, path),
                          access.apiKey,
                          query);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbSeasonRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const QString path = QStringLiteral("tv/%1/season/%2")
        .arg(request.showId)
        .arg(request.seasonNumber);

    QUrlQuery query;
    if (request.language.has_value() && !request.language->empty())
    {
        query.addQueryItem(QStringLiteral("language"),
                           QString::fromStdString(*request.language));
    }

    return jsonGetRequest(endpointUrl(access.baseUrl, path),
                          access.apiKey,
                          query);
}

QNetworkRequest TmdbRequestBuilder::build(
    const TmdbCollectionRequestDto &request) const
{
    const TmdbRequestAccess access = m_runtimeConfig.requestAccess();
    const QString path = QStringLiteral("collection/%1").arg(request.id);

    return jsonGetRequest(endpointUrl(access.baseUrl, path),
                          access.apiKey,
                          QUrlQuery());
}

}
