#include "Metadata/TmdbClient.h"

#include "Metadata/TmdbJsonReader.h"
#include "Metadata/TmdbReplyError.h"

#include "MmLog.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonValue>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace Makimedia::Tmdb
{
namespace
{

TmdbError invalidFieldError(const char *fieldName)
{
    return TmdbError(
        TmdbError::Category::Parse,
        std::string("TMDB returned invalid data for the '") + fieldName + "' field.");
}

using GenreParseResult = std::variant<std::vector<TmdbGenreId>, TmdbError>;

GenreParseResult parseGenreIds(const std::optional<QJsonArray> &array)
{
    std::vector<TmdbGenreId> genreIds;
    if (!array.has_value())
    {
        return genreIds;
    }

    genreIds.reserve(static_cast<std::size_t>(array->size()));
    for (const QJsonValue &value : *array)
    {
        if (!value.isDouble())
        {
            return invalidFieldError("genre_ids");
        }

        const double rawId = value.toDouble();
        const long double wideId = static_cast<long double>(rawId);
        const long double minimum = static_cast<long double>(
            std::numeric_limits<TmdbGenreId>::lowest());
        const long double maximum = static_cast<long double>(
            std::numeric_limits<TmdbGenreId>::max());

        if (!std::isfinite(rawId)
            || std::trunc(wideId) != wideId
            || wideId < minimum
            || wideId > maximum)
        {
            return invalidFieldError("genre_ids");
        }

        genreIds.push_back(static_cast<TmdbGenreId>(rawId));
    }

    return genreIds;
}

std::optional<TmdbPageNumber> pageNumber(std::int64_t value, bool allowZero)
{
    const std::int64_t minimum = allowZero ? 0 : 1;
    if (value < minimum
        || static_cast<std::uint64_t>(value)
               > std::numeric_limits<TmdbPageNumber>::max())
    {
        return std::nullopt;
    }

    return static_cast<TmdbPageNumber>(value);
}

std::optional<TmdbResultCount> resultCount(std::int64_t value)
{
    if (value < 0)
    {
        return std::nullopt;
    }

    return static_cast<TmdbResultCount>(value);
}

std::optional<TmdbError::HttpStatus> httpStatus(const QNetworkReply &reply)
{
    const QVariant statusAttribute = reply.attribute(
        QNetworkRequest::HttpStatusCodeAttribute);
    if (!statusAttribute.isValid())
    {
        return std::nullopt;
    }

    return statusAttribute.toInt();
}

std::optional<TmdbError::RetryAfter> retryAfter(const QNetworkReply &reply)
{
    return TmdbReplyError::retryAfterFrom(
        reply.rawHeader(QLatin1StringView("Retry-After")),
        QDateTime::currentDateTimeUtc());
}

TmdbError mappedReplyError(QNetworkReply &reply, const QByteArray &body)
{
    return TmdbReplyError::from(reply.error(),
                                httpStatus(reply),
                                TmdbReplyError::statusCodeIn(body),
                                retryAfter(reply));
}

TmdbClient::TitleParseResult parseTitleResult(const QJsonObject &object,
                                              TmdbMediaType mediaType,
                                              const QString &titleField,
                                              const QString &dateField)
{
    TmdbJsonReader reader(object);

    const auto id = reader.requiredInteger(QStringLiteral("id"));
    const auto title = reader.requiredString(titleField);
    const auto originalTitle =
        reader.optionalString(QStringLiteral("original_") + titleField);
    const auto releaseDate = reader.optionalString(dateField);
    const auto rawGenreIds = reader.optionalArray(QStringLiteral("genre_ids"));
    const auto rating = reader.optionalNumber(QStringLiteral("vote_average"));
    const auto voteCount = reader.optionalInteger(QStringLiteral("vote_count"));
    const auto popularity = reader.optionalNumber(QStringLiteral("popularity"));
    const auto overview = reader.optionalString(QStringLiteral("overview"));
    const auto posterPath = reader.optionalString(QStringLiteral("poster_path"));

    if (reader.hasError())
    {
        return *reader.error();
    }

    GenreParseResult genreResult = parseGenreIds(rawGenreIds);
    if (auto *error = std::get_if<TmdbError>(&genreResult))
    {
        return std::move(*error);
    }

    TmdbTitleResultDto dto;
    dto.id = *id;
    dto.mediaType = mediaType;
    dto.title = *title;
    dto.originalTitle = originalTitle.value_or(std::string{});
    dto.releaseDate = releaseDate;
    dto.genreIds = std::move(std::get<std::vector<TmdbGenreId>>(genreResult));
    dto.rating = rating.value_or(0.0);
    dto.voteCount = voteCount.value_or(0);
    dto.popularity = popularity.value_or(0.0);
    dto.overview = overview.value_or(std::string{});
    dto.posterPath = posterPath;
    return dto;
}

}

TmdbClient::TmdbClient(QNetworkAccessManager &networkAccessManager,
                       const TmdbRequestBuilder &requestBuilder,
                       QObject *parent)
    : QObject(parent)
    , m_networkAccessManager(networkAccessManager)
    , m_requestBuilder(requestBuilder)
{
    m_rateLimitTimer.setSingleShot(true);
    connect(&m_rateLimitTimer, &QTimer::timeout, this, [this]() {
        m_rateLimited = false;
        MM_LOG_I() << "TMDB rate limit window over, resuming"
                   << m_pendingRequests.size() << "queued requests";
        startQueuedRequests();
    });
}

void TmdbClient::holdFor(int seconds)
{
    const int waitMs = qBound(1000, seconds * 1000, 120000);
    m_rateLimited = true;

    if (m_rateLimitTimer.isActive() && m_rateLimitTimer.remainingTime() > waitMs) {
        return;
    }

    MM_LOG_W() << "TMDB is rate limiting, holding the queue for"
               << waitMs / 1000 << "seconds," << m_pendingRequests.size()
               << "requests waiting";
    m_rateLimitTimer.start(waitMs);
}

TmdbClient::~TmdbClient()
{
    discardAllRequests();
}

TmdbClient::RequestId TmdbClient::discoverMovie(
    TmdbDiscoverRequestDto request,
    DiscoverCompletionHandler completionHandler)
{
    request.mediaType = TmdbMediaType::Movie;
    return discover(std::move(request), std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::discoverTv(
    TmdbDiscoverRequestDto request,
    DiscoverCompletionHandler completionHandler)
{
    request.mediaType = TmdbMediaType::Tv;
    return discover(std::move(request), std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::configuration(
    ConfigurationCompletionHandler completionHandler)
{
    MM_LOG_D() << "TMDB configuration requested";

    InternalCompletionHandler internalHandler =
        [completionHandler = std::move(completionHandler)](
            InternalResult result) mutable {
        if (!completionHandler)
        {
            return;
        }

        if (auto *configuration = std::get_if<TmdbConfigurationResponseDto>(&result))
        {
            completionHandler(std::move(*configuration));
            return;
        }

        if (auto *error = std::get_if<TmdbError>(&result))
        {
            completionHandler(std::move(*error));
            return;
        }

        completionHandler(TmdbError(
            TmdbError::Category::Parse,
            "TMDB returned an unexpected configuration response."));
    };

    return enqueue(m_requestBuilder.build(TmdbConfigurationRequestDto{}),
                   RequestKind::Configuration,
                   std::move(internalHandler));
}

TmdbClient::RequestId TmdbClient::movieGenres(
    std::optional<std::string> language,
    GenreListCompletionHandler completionHandler)
{
    TmdbGenreListRequestDto request;
    request.mediaType = TmdbMediaType::Movie;
    request.language = std::move(language);
    return genreList(std::move(request), std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::tvGenres(
    std::optional<std::string> language,
    GenreListCompletionHandler completionHandler)
{
    TmdbGenreListRequestDto request;
    request.mediaType = TmdbMediaType::Tv;
    request.language = std::move(language);
    return genreList(std::move(request), std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::trendingMovie(
    TmdbPageNumber page,
    DiscoverCompletionHandler completionHandler)
{
    TmdbTrendingRequestDto request;
    request.mediaType = TmdbMediaType::Movie;
    request.page = page;
    MM_LOG_D() << "TMDB trending requested movie page" << page;
    return enqueueDiscoverShapedRequest(m_requestBuilder.build(request),
                                        RequestKind::TrendingMovie,
                                        std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::trendingTv(
    TmdbPageNumber page,
    DiscoverCompletionHandler completionHandler)
{
    TmdbTrendingRequestDto request;
    request.mediaType = TmdbMediaType::Tv;
    request.page = page;
    MM_LOG_D() << "TMDB trending requested tv page" << page;
    return enqueueDiscoverShapedRequest(m_requestBuilder.build(request),
                                        RequestKind::TrendingTv,
                                        std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::popularMovie(
    TmdbPageNumber page,
    DiscoverCompletionHandler completionHandler)
{
    TmdbPopularRequestDto request;
    request.mediaType = TmdbMediaType::Movie;
    request.page = page;
    MM_LOG_D() << "TMDB popular requested movie page" << page;
    return enqueueDiscoverShapedRequest(m_requestBuilder.build(request),
                                        RequestKind::PopularMovie,
                                        std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::popularTv(
    TmdbPageNumber page,
    DiscoverCompletionHandler completionHandler)
{
    TmdbPopularRequestDto request;
    request.mediaType = TmdbMediaType::Tv;
    request.page = page;
    MM_LOG_D() << "TMDB popular requested tv page" << page;
    return enqueueDiscoverShapedRequest(m_requestBuilder.build(request),
                                        RequestKind::PopularTv,
                                        std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::searchMovie(
    TmdbSearchRequestDto request,
    DiscoverCompletionHandler completionHandler)
{
    request.mediaType = TmdbMediaType::Movie;
    MM_LOG_D() << "TMDB search requested movie" << QString::fromStdString(request.query)
               << (request.year.has_value() ? *request.year : 0);
    return enqueueDiscoverShapedRequest(m_requestBuilder.build(request),
                                        RequestKind::SearchMovie,
                                        std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::searchTv(
    TmdbSearchRequestDto request,
    DiscoverCompletionHandler completionHandler)
{
    request.mediaType = TmdbMediaType::Tv;
    MM_LOG_D() << "TMDB search requested tv" << QString::fromStdString(request.query)
               << (request.year.has_value() ? *request.year : 0);
    return enqueueDiscoverShapedRequest(m_requestBuilder.build(request),
                                        RequestKind::SearchTv,
                                        std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::videos(
    TmdbMediaType mediaType,
    TmdbId id,
    VideosCompletionHandler completionHandler)
{
    TmdbVideosRequestDto request;
    request.mediaType = mediaType;
    request.id = id;

    MM_LOG_D() << "TMDB videos requested"
               << (mediaType == TmdbMediaType::Movie ? "movie" : "tv") << id;

    InternalCompletionHandler internalHandler =
        [completionHandler = std::move(completionHandler)](
            InternalResult result) mutable {
        if (!completionHandler)
        {
            return;
        }

        if (auto *videos = std::get_if<TmdbVideosResponseDto>(&result))
        {
            completionHandler(std::move(*videos));
            return;
        }

        if (auto *error = std::get_if<TmdbError>(&result))
        {
            completionHandler(std::move(*error));
            return;
        }

        completionHandler(TmdbError(
            TmdbError::Category::Parse,
            "TMDB returned an unexpected videos response."));
    };

    return enqueue(m_requestBuilder.build(request),
                   RequestKind::Videos,
                   std::move(internalHandler));
}

TmdbClient::RequestId TmdbClient::watchProviders(
    TmdbMediaType mediaType,
    TmdbId id,
    WatchProvidersCompletionHandler completionHandler)
{
    TmdbWatchProvidersRequestDto request;
    request.mediaType = mediaType;
    request.id = id;

    MM_LOG_D() << "TMDB watch providers requested"
               << (mediaType == TmdbMediaType::Movie ? "movie" : "tv") << id;

    InternalCompletionHandler internalHandler =
        [completionHandler = std::move(completionHandler)](
            InternalResult result) mutable {
        if (!completionHandler)
        {
            return;
        }

        if (auto *providers = std::get_if<TmdbWatchProvidersResponseDto>(&result))
        {
            completionHandler(std::move(*providers));
            return;
        }

        if (auto *error = std::get_if<TmdbError>(&result))
        {
            completionHandler(std::move(*error));
            return;
        }

        completionHandler(TmdbError(
            TmdbError::Category::Parse,
            "TMDB returned an unexpected watch providers response."));
    };

    return enqueue(m_requestBuilder.build(request),
                   RequestKind::WatchProviders,
                   std::move(internalHandler));
}

TmdbClient::RequestId TmdbClient::titleDetails(
    TmdbMediaType mediaType,
    TmdbId id,
    TitleDetailsCompletionHandler completionHandler)
{
    TmdbTitleDetailsRequestDto request;
    request.mediaType = mediaType;
    request.id = id;

    const RequestKind kind = mediaType == TmdbMediaType::Movie
        ? RequestKind::TitleDetailsMovie
        : RequestKind::TitleDetailsTv;

    MM_LOG_D() << "TMDB title details requested"
               << (mediaType == TmdbMediaType::Movie ? "movie" : "tv") << id;

    InternalCompletionHandler internalHandler =
        [completionHandler = std::move(completionHandler)](
            InternalResult result) mutable {
        if (!completionHandler)
        {
            return;
        }

        if (auto *details = std::get_if<TmdbTitleDetailsDto>(&result))
        {
            completionHandler(std::move(*details));
            return;
        }

        if (auto *error = std::get_if<TmdbError>(&result))
        {
            completionHandler(std::move(*error));
            return;
        }

        completionHandler(TmdbError(
            TmdbError::Category::Parse,
            "TMDB returned an unexpected title details response."));
    };

    return enqueue(m_requestBuilder.build(request),
                   kind,
                   std::move(internalHandler));
}

TmdbClient::RequestId TmdbClient::season(
    TmdbId showId,
    int seasonNumber,
    std::optional<std::string> language,
    SeasonCompletionHandler completionHandler)
{
    TmdbSeasonRequestDto request;
    request.showId = showId;
    request.seasonNumber = seasonNumber;
    request.language = std::move(language);

    MM_LOG_D() << "TMDB season requested, show" << showId
               << "season" << seasonNumber;

    InternalCompletionHandler internalHandler =
        [completionHandler = std::move(completionHandler)](
            InternalResult result) mutable {
        if (!completionHandler)
        {
            return;
        }

        if (auto *season = std::get_if<TmdbSeasonDto>(&result))
        {
            completionHandler(std::move(*season));
            return;
        }

        if (auto *error = std::get_if<TmdbError>(&result))
        {
            completionHandler(std::move(*error));
            return;
        }

        completionHandler(TmdbError(
            TmdbError::Category::Parse,
            "TMDB returned an unexpected season response."));
    };

    return enqueue(m_requestBuilder.build(request),
                   RequestKind::Season,
                   std::move(internalHandler));
}

TmdbClient::RequestId TmdbClient::collection(
    TmdbId collectionId,
    CollectionCompletionHandler completionHandler)
{
    TmdbCollectionRequestDto request;
    request.id = collectionId;

    MM_LOG_D() << "TMDB collection requested" << collectionId;

    InternalCompletionHandler internalHandler =
        [completionHandler = std::move(completionHandler)](
            InternalResult result) mutable {
        if (!completionHandler)
        {
            return;
        }

        if (auto *collection = std::get_if<TmdbCollectionDto>(&result))
        {
            completionHandler(std::move(*collection));
            return;
        }

        if (auto *error = std::get_if<TmdbError>(&result))
        {
            completionHandler(std::move(*error));
            return;
        }

        completionHandler(TmdbError(
            TmdbError::Category::Parse,
            "TMDB returned an unexpected collection response."));
    };

    return enqueue(m_requestBuilder.build(request),
                   RequestKind::Collection,
                   std::move(internalHandler));
}

bool TmdbClient::cancelRequest(RequestId requestId)
{
    const auto pending = std::find_if(
        m_pendingRequests.begin(),
        m_pendingRequests.end(),
        [requestId](const PendingRequest &request) {
            return request.id == requestId;
        });

    if (pending != m_pendingRequests.end())
    {
        MM_LOG_W() << "TMDB request cancelled while pending" << requestId
                   << requestKindLabel(pending->kind);
        InternalCompletionHandler completionHandler =
            std::move(pending->completionHandler);
        m_pendingRequests.erase(pending);
        if (completionHandler)
        {
            completionHandler(cancelledError());
        }
        return true;
    }

    const auto active = m_activeRequests.find(requestId);
    if (active == m_activeRequests.end())
    {
        return false;
    }

    MM_LOG_W() << "TMDB request cancelled while active" << requestId
               << requestKindLabel(active->kind);
    ActiveRequest activeRequest = std::move(active.value());
    m_activeRequests.erase(active);
    if (activeRequest.reply)
    {
        disconnect(activeRequest.reply, nullptr, this, nullptr);
        activeRequest.reply->abort();
        activeRequest.reply->deleteLater();
    }

    startQueuedRequests();
    if (activeRequest.completionHandler)
    {
        activeRequest.completionHandler(cancelledError());
    }
    return true;
}

void TmdbClient::cancelAllRequests()
{
    MM_LOG_W() << "Cancelling all TMDB requests" << m_pendingRequests.size()
               << "pending" << m_activeRequests.size() << "active";

    std::vector<InternalCompletionHandler> completionHandlers;
    completionHandlers.reserve(m_pendingRequests.size()
                               + static_cast<std::size_t>(m_activeRequests.size()));

    for (PendingRequest &pendingRequest : m_pendingRequests)
    {
        completionHandlers.push_back(std::move(pendingRequest.completionHandler));
    }
    m_pendingRequests.clear();

    for (auto active = m_activeRequests.begin();
         active != m_activeRequests.end();
         ++active)
    {
        ActiveRequest &activeRequest = active.value();
        completionHandlers.push_back(std::move(activeRequest.completionHandler));
        if (activeRequest.reply)
        {
            disconnect(activeRequest.reply, nullptr, this, nullptr);
            activeRequest.reply->abort();
            activeRequest.reply->deleteLater();
        }
    }
    m_activeRequests.clear();

    for (InternalCompletionHandler &completionHandler : completionHandlers)
    {
        if (completionHandler)
        {
            completionHandler(cancelledError());
        }
    }
}

std::size_t TmdbClient::activeRequestCount() const noexcept
{
    return static_cast<std::size_t>(m_activeRequests.size());
}

std::size_t TmdbClient::pendingRequestCount() const noexcept
{
    return m_pendingRequests.size();
}

void TmdbClient::setConcurrentRequestLimit(std::size_t limit)
{
    const std::size_t next = std::clamp<std::size_t>(limit, 1, MaximumConcurrentRequests);
    if (next == m_concurrentLimit) {
        return;
    }

    MM_LOG_I() << "TMDB requests now run" << next << "at a time";
    m_concurrentLimit = next;
    startQueuedRequests();
}

TmdbClient::TitleParseResult TmdbClient::parseMovieResult(const QJsonObject &object)
{
    return parseTitleResult(object,
                            TmdbMediaType::Movie,
                            QStringLiteral("title"),
                            QStringLiteral("release_date"));
}

TmdbClient::TitleParseResult TmdbClient::parseTvResult(const QJsonObject &object)
{
    return parseTitleResult(object,
                            TmdbMediaType::Tv,
                            QStringLiteral("name"),
                            QStringLiteral("first_air_date"));
}

TmdbClient::DiscoverResult TmdbClient::parseDiscoverPage(
    const QJsonObject &object,
    TmdbMediaType mediaType)
{
    TmdbJsonReader reader(object);
    const auto rawPage = reader.requiredInteger(QStringLiteral("page"));
    const auto rawTotalPages = reader.requiredInteger(QStringLiteral("total_pages"));
    const auto rawTotalResults = reader.requiredInteger(QStringLiteral("total_results"));
    const auto rawResults = reader.requiredArray(QStringLiteral("results"));

    if (reader.hasError())
    {
        return *reader.error();
    }

    const auto parsedPage = pageNumber(*rawPage, false);
    const auto parsedTotalPages = pageNumber(*rawTotalPages, true);
    const auto parsedTotalResults = resultCount(*rawTotalResults);
    if (!parsedPage.has_value()
        || !parsedTotalPages.has_value()
        || !parsedTotalResults.has_value())
    {
        return TmdbError(TmdbError::Category::Parse,
                         "TMDB returned invalid page information.");
    }

    TmdbDiscoverPageDto page;
    page.page = *parsedPage;
    page.totalPages = *parsedTotalPages;
    page.totalResults = *parsedTotalResults;
    page.results.reserve(static_cast<std::size_t>(rawResults->size()));

    for (const QJsonValue &value : *rawResults)
    {
        if (!value.isObject())
        {
            return invalidFieldError("results");
        }

        TitleParseResult titleResult = mediaType == TmdbMediaType::Movie
            ? parseMovieResult(value.toObject())
            : parseTvResult(value.toObject());

        if (auto *error = std::get_if<TmdbError>(&titleResult))
        {
            return std::move(*error);
        }

        page.results.push_back(
            std::move(std::get<TmdbTitleResultDto>(titleResult)));
    }

    return page;
}

TmdbClient::ConfigurationResult TmdbClient::parseConfigurationResponse(
    const QJsonObject &object)
{
    TmdbJsonReader rootReader(object);
    const auto imagesObject = rootReader.requiredObject(QStringLiteral("images"));
    if (rootReader.hasError())
    {
        return *rootReader.error();
    }

    TmdbJsonReader imagesReader(*imagesObject);
    const auto baseUrl = imagesReader.requiredString(QStringLiteral("base_url"));
    const auto secureBaseUrl = imagesReader.requiredString(
        QStringLiteral("secure_base_url"));
    const auto rawPosterSizes = imagesReader.requiredArray(
        QStringLiteral("poster_sizes"));
    if (imagesReader.hasError())
    {
        return *imagesReader.error();
    }

    std::vector<std::string> posterSizes;
    posterSizes.reserve(static_cast<std::size_t>(rawPosterSizes->size()));
    for (const QJsonValue &value : *rawPosterSizes)
    {
        if (!value.isString())
        {
            return invalidFieldError("poster_sizes");
        }
        posterSizes.push_back(value.toString().toUtf8().toStdString());
    }

    std::vector<std::string> stillSizes;
    const auto rawStillSizes = imagesReader.optionalArray(
        QStringLiteral("still_sizes"));
    if (rawStillSizes.has_value())
    {
        for (const QJsonValue &value : *rawStillSizes)
        {
            if (value.isString())
            {
                stillSizes.push_back(value.toString().toUtf8().toStdString());
            }
        }
    }

    std::vector<std::string> backdropSizes;
    const auto rawBackdropSizes = imagesReader.optionalArray(
        QStringLiteral("backdrop_sizes"));
    if (rawBackdropSizes.has_value())
    {
        for (const QJsonValue &value : *rawBackdropSizes)
        {
            if (value.isString())
            {
                backdropSizes.push_back(value.toString().toUtf8().toStdString());
            }
        }
    }

    std::vector<std::string> profileSizes;
    const auto rawProfileSizes = imagesReader.optionalArray(
        QStringLiteral("profile_sizes"));
    if (rawProfileSizes.has_value())
    {
        for (const QJsonValue &value : *rawProfileSizes)
        {
            if (value.isString())
            {
                profileSizes.push_back(value.toString().toUtf8().toStdString());
            }
        }
    }

    TmdbConfigurationResponseDto response;
    response.images.baseUrl = *baseUrl;
    response.images.secureBaseUrl = *secureBaseUrl;
    response.images.posterSizes = std::move(posterSizes);
    response.images.stillSizes = std::move(stillSizes);
    response.images.backdropSizes = std::move(backdropSizes);
    response.images.profileSizes = std::move(profileSizes);
    return response;
}

TmdbClient::GenreListResult TmdbClient::parseGenreListResponse(
    const QJsonObject &object)
{
    TmdbJsonReader rootReader(object);
    const auto rawGenres = rootReader.requiredArray(QStringLiteral("genres"));
    if (rootReader.hasError())
    {
        return *rootReader.error();
    }

    TmdbGenreListResponseDto response;
    response.genres.reserve(static_cast<std::size_t>(rawGenres->size()));
    for (const QJsonValue &value : *rawGenres)
    {
        if (!value.isObject())
        {
            return invalidFieldError("genres");
        }

        TmdbJsonReader genreReader(value.toObject());
        const auto rawId = genreReader.requiredInteger(QStringLiteral("id"));
        const auto name = genreReader.requiredString(QStringLiteral("name"));
        if (genreReader.hasError())
        {
            return *genreReader.error();
        }
        if (*rawId <= 0
            || static_cast<std::uint64_t>(*rawId)
                > static_cast<std::uint64_t>(
                    std::numeric_limits<TmdbGenreId>::max())
            || name->empty())
        {
            return invalidFieldError("genres");
        }

        response.genres.push_back(
            TmdbGenreDto{static_cast<TmdbGenreId>(*rawId), *name});
    }

    return response;
}

TmdbClient::VideosResult TmdbClient::parseVideosResponse(const QJsonObject &object)
{
    TmdbJsonReader rootReader(object);
    const auto rawResults = rootReader.requiredArray(QStringLiteral("results"));
    if (rootReader.hasError())
    {
        return *rootReader.error();
    }

    TmdbVideosResponseDto response;
    response.results.reserve(static_cast<std::size_t>(rawResults->size()));
    for (const QJsonValue &value : *rawResults)
    {
        if (!value.isObject())
        {
            continue;
        }

        TmdbJsonReader videoReader(value.toObject());
        const auto key = videoReader.optionalString(QStringLiteral("key"));
        const auto site = videoReader.optionalString(QStringLiteral("site"));
        const auto type = videoReader.optionalString(QStringLiteral("type"));
        const auto official = videoReader.optionalBoolean(QStringLiteral("official"));
        if (videoReader.hasError() || !key.has_value() || key->empty())
        {
            continue;
        }

        response.results.push_back(TmdbVideoDto{
            *key,
            site.value_or(std::string{}),
            type.value_or(std::string{}),
            official.value_or(false)});
    }

    return response;
}

TmdbClient::WatchProvidersResult TmdbClient::parseWatchProvidersResponse(
    const QJsonObject &object)
{
    static const QString region = QStringLiteral("US");

    TmdbJsonReader rootReader(object);
    const auto resultsObject = rootReader.optionalObject(QStringLiteral("results"));
    if (rootReader.hasError())
    {
        return *rootReader.error();
    }

    TmdbWatchProvidersResponseDto response;
    if (!resultsObject.has_value())
    {
        return response;
    }

    TmdbJsonReader resultsReader(*resultsObject);
    const auto regionObject = resultsReader.optionalObject(region);
    if (resultsReader.hasError())
    {
        return *resultsReader.error();
    }
    if (!regionObject.has_value())
    {
        return response;
    }

    TmdbJsonReader regionReader(*regionObject);
    const auto rawFlatrate = regionReader.optionalArray(QStringLiteral("flatrate"));
    if (regionReader.hasError())
    {
        return *regionReader.error();
    }
    if (!rawFlatrate.has_value())
    {
        return response;
    }

    response.providers.reserve(static_cast<std::size_t>(rawFlatrate->size()));
    for (const QJsonValue &value : *rawFlatrate)
    {
        if (!value.isObject())
        {
            continue;
        }

        TmdbJsonReader providerReader(value.toObject());
        const auto providerId = providerReader.optionalInteger(
            QStringLiteral("provider_id"));
        const auto providerName = providerReader.optionalString(
            QStringLiteral("provider_name"));
        const auto logoPath = providerReader.optionalString(
            QStringLiteral("logo_path"));
        if (providerReader.hasError()
            || !providerName.has_value()
            || providerName->empty())
        {
            continue;
        }

        response.providers.push_back(TmdbWatchProviderDto{
            providerId.value_or(0),
            *providerName,
            logoPath.value_or(std::string{})});
    }

    return response;
}

namespace
{

QString preferredCertificationRegion()
{
    const QString code = QLocale::territoryToCode(QLocale::system().territory());
    return code.isEmpty() ? QStringLiteral("US") : code;
}

std::string readCertification(const QJsonObject &object, TmdbMediaType mediaType)
{
    const bool movie = mediaType == TmdbMediaType::Movie;
    const QString container = movie ? QStringLiteral("release_dates")
                                    : QStringLiteral("content_ratings");

    const QJsonArray results =
        object.value(container).toObject().value(QStringLiteral("results")).toArray();
    if (results.isEmpty())
    {
        return {};
    }

    const QString preferred = preferredCertificationRegion();
    std::string mine;
    std::string fallbackUs;
    std::string anything;

    for (const QJsonValue &value : results)
    {
        const QJsonObject entry = value.toObject();
        const QString region = entry.value(QStringLiteral("iso_3166_1")).toString();

        QString rating;
        if (movie)
        {
            const QJsonArray releases =
                entry.value(QStringLiteral("release_dates")).toArray();
            for (const QJsonValue &release : releases)
            {
                const QString candidate = release.toObject()
                    .value(QStringLiteral("certification")).toString().trimmed();
                if (!candidate.isEmpty())
                {
                    rating = candidate;
                    break;
                }
            }
        }
        else
        {
            rating = entry.value(QStringLiteral("rating")).toString().trimmed();
        }

        if (rating.isEmpty())
        {
            continue;
        }

        const std::string value8 = rating.toUtf8().toStdString();
        if (region.compare(preferred, Qt::CaseInsensitive) == 0)
        {
            mine = value8;
            break;
        }
        if (region == QLatin1String("US"))
        {
            fallbackUs = value8;
        }
        if (anything.empty())
        {
            anything = value8;
        }
    }

    if (!mine.empty())
    {
        return mine;
    }
    if (!fallbackUs.empty())
    {
        return fallbackUs;
    }
    return anything;
}

constexpr int kCastKept = 10;

std::optional<std::string> optionalPath(const QJsonValue &value)
{
    const QString text = value.toString();
    if (text.isEmpty())
    {
        return std::nullopt;
    }
    return text.toUtf8().toStdString();
}

std::vector<TmdbCreditDto> readCast(const QJsonArray &array)
{
    std::vector<TmdbCreditDto> cast;
    for (const QJsonValue &value : array)
    {
        if (!value.isObject() || int(cast.size()) >= kCastKept)
        {
            continue;
        }

        const QJsonObject person = value.toObject();
        const QString name = person.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
        {
            continue;
        }

        TmdbCreditDto credit;
        credit.name = name.toUtf8().toStdString();
        credit.role = person.value(QStringLiteral("character"))
                          .toString().toUtf8().toStdString();
        credit.profilePath = optionalPath(person.value(QStringLiteral("profile_path")));
        cast.push_back(std::move(credit));
    }
    return cast;
}

bool isWritingJob(const QString &job, const QString &department)
{
    return department.compare(QLatin1String("Writing"), Qt::CaseInsensitive) == 0
        || job.compare(QLatin1String("Writer"), Qt::CaseInsensitive) == 0
        || job.compare(QLatin1String("Screenplay"), Qt::CaseInsensitive) == 0;
}

void readCrew(const QJsonArray &array,
              std::vector<TmdbCreditDto> &directors,
              std::vector<TmdbCreditDto> &writers)
{
    for (const QJsonValue &value : array)
    {
        if (!value.isObject())
        {
            continue;
        }

        const QJsonObject person = value.toObject();
        const QString name = person.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
        {
            continue;
        }

        const QString job = person.value(QStringLiteral("job")).toString();
        const QString department = person.value(QStringLiteral("department")).toString();

        TmdbCreditDto credit;
        credit.name = name.toUtf8().toStdString();
        credit.role = job.toUtf8().toStdString();
        credit.profilePath = optionalPath(person.value(QStringLiteral("profile_path")));

        if (job.compare(QLatin1String("Director"), Qt::CaseInsensitive) == 0)
        {
            directors.push_back(std::move(credit));
        }
        else if (isWritingJob(job, department))
        {
            writers.push_back(std::move(credit));
        }
    }
}

std::vector<TmdbCreditDto> readCreators(const QJsonArray &array)
{
    std::vector<TmdbCreditDto> creators;
    for (const QJsonValue &value : array)
    {
        if (!value.isObject())
        {
            continue;
        }

        const QJsonObject person = value.toObject();
        const QString name = person.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
        {
            continue;
        }

        TmdbCreditDto credit;
        credit.name = name.toUtf8().toStdString();
        credit.profilePath = optionalPath(person.value(QStringLiteral("profile_path")));
        creators.push_back(std::move(credit));
    }
    return creators;
}

TmdbClient::TitleDetailsResult parseTitleDetails(const QJsonObject &object,
                                                 TmdbMediaType mediaType,
                                                 const QString &titleField,
                                                 const QString &dateField)
{
    TmdbJsonReader reader(object);

    const auto id = reader.requiredInteger(QStringLiteral("id"));
    const auto title = reader.requiredString(titleField);
    const auto releaseDate = reader.optionalString(dateField);
    const auto rawGenres = reader.optionalArray(QStringLiteral("genres"));
    const auto rating = reader.optionalNumber(QStringLiteral("vote_average"));
    const auto overview = reader.optionalString(QStringLiteral("overview"));
    const auto posterPath = reader.optionalString(QStringLiteral("poster_path"));
    const auto backdropPath = reader.optionalString(QStringLiteral("backdrop_path"));

    const auto runtime = reader.optionalInteger(QStringLiteral("runtime"));
    const auto episodeRuntimes =
        reader.optionalArray(QStringLiteral("episode_run_time"));
    const auto rawCollection =
        reader.optionalObject(QStringLiteral("belongs_to_collection"));

    if (reader.hasError())
    {
        return *reader.error();
    }

    std::optional<TmdbCollectionRefDto> collection;
    if (rawCollection.has_value())
    {
        TmdbJsonReader collectionReader(*rawCollection);
        const auto collectionId = collectionReader.requiredInteger(QStringLiteral("id"));
        const auto collectionName = collectionReader.optionalString(QStringLiteral("name"));
        const auto collectionPoster =
            collectionReader.optionalString(QStringLiteral("poster_path"));
        const auto collectionBackdrop =
            collectionReader.optionalString(QStringLiteral("backdrop_path"));
        if (!collectionReader.hasError() && collectionId.has_value() && *collectionId > 0)
        {
            TmdbCollectionRefDto ref;
            ref.id = *collectionId;
            ref.name = collectionName.value_or(std::string{});
            ref.posterPath = collectionPoster;
            ref.backdropPath = collectionBackdrop;
            collection = std::move(ref);
        }
    }

    int runtimeMinutes = static_cast<int>(runtime.value_or(0));
    if (runtimeMinutes <= 0 && episodeRuntimes.has_value()
        && !episodeRuntimes->isEmpty())
    {
        runtimeMinutes = episodeRuntimes->first().toInt(0);
    }

    std::vector<std::string> genreNames;
    if (rawGenres.has_value())
    {
        genreNames.reserve(static_cast<std::size_t>(rawGenres->size()));
        for (const QJsonValue &value : *rawGenres)
        {
            if (!value.isObject())
            {
                continue;
            }

            TmdbJsonReader genreReader(value.toObject());
            const auto name = genreReader.optionalString(QStringLiteral("name"));
            if (genreReader.hasError() || !name.has_value() || name->empty())
            {
                continue;
            }

            genreNames.push_back(*name);
        }
    }

    TmdbTitleDetailsDto dto;
    dto.id = *id;
    dto.mediaType = mediaType;
    dto.title = *title;
    dto.releaseDate = releaseDate;
    dto.genreNames = std::move(genreNames);
    dto.rating = rating.value_or(0.0);
    dto.overview = overview.value_or(std::string{});
    dto.posterPath = posterPath;
    dto.backdropPath = backdropPath;
    dto.runtimeMinutes = runtimeMinutes;
    dto.collection = std::move(collection);
    dto.certification = readCertification(object, mediaType);

    const QJsonObject credits = object.value(QStringLiteral("credits")).toObject();
    dto.cast = readCast(credits.value(QStringLiteral("cast")).toArray());
    readCrew(credits.value(QStringLiteral("crew")).toArray(),
             dto.directors,
             dto.writers);
    dto.creators = readCreators(object.value(QStringLiteral("created_by")).toArray());
    return dto;
}

}

TmdbClient::SeasonResult TmdbClient::parseSeasonResponse(const QJsonObject &object)
{
    TmdbJsonReader reader(object);

    const auto id = reader.requiredInteger(QStringLiteral("id"));
    const auto seasonNumber = reader.requiredInteger(QStringLiteral("season_number"));
    const auto name = reader.optionalString(QStringLiteral("name"));
    const auto overview = reader.optionalString(QStringLiteral("overview"));
    const auto airDate = reader.optionalString(QStringLiteral("air_date"));
    const auto posterPath = reader.optionalString(QStringLiteral("poster_path"));
    const auto rawEpisodes = reader.optionalArray(QStringLiteral("episodes"));

    if (reader.hasError())
    {
        return *reader.error();
    }

    std::vector<TmdbEpisodeDto> episodes;
    if (rawEpisodes.has_value())
    {
        episodes.reserve(static_cast<std::size_t>(rawEpisodes->size()));
        for (const QJsonValue &value : *rawEpisodes)
        {
            if (!value.isObject())
            {
                continue;
            }

            TmdbJsonReader episodeReader(value.toObject());
            const auto episodeId = episodeReader.requiredInteger(QStringLiteral("id"));
            const auto episodeNumber =
                episodeReader.requiredInteger(QStringLiteral("episode_number"));
            const auto episodeName = episodeReader.optionalString(QStringLiteral("name"));
            const auto episodeOverview =
                episodeReader.optionalString(QStringLiteral("overview"));
            const auto episodeAirDate =
                episodeReader.optionalString(QStringLiteral("air_date"));
            const auto stillPath =
                episodeReader.optionalString(QStringLiteral("still_path"));
            const auto runtime = episodeReader.optionalInteger(QStringLiteral("runtime"));
            const auto episodeRating =
                episodeReader.optionalNumber(QStringLiteral("vote_average"));
            const auto episodeSeasonNumber =
                episodeReader.optionalInteger(QStringLiteral("season_number"));

            if (episodeReader.hasError() || !episodeId.has_value()
                || !episodeNumber.has_value())
            {
                continue;
            }

            TmdbEpisodeDto episode;
            episode.id = *episodeId;
            episode.seasonNumber = static_cast<int>(
                episodeSeasonNumber.value_or(seasonNumber.value_or(0)));
            episode.episodeNumber = static_cast<int>(*episodeNumber);
            episode.name = episodeName.value_or(std::string{});
            episode.overview = episodeOverview.value_or(std::string{});
            episode.airDate = episodeAirDate;
            episode.stillPath = stillPath;
            episode.runtimeMinutes = static_cast<int>(runtime.value_or(0));
            episode.rating = episodeRating.value_or(0.0);

            readCrew(value.toObject().value(QStringLiteral("crew")).toArray(),
                     episode.directors,
                     episode.writers);
            episodes.push_back(std::move(episode));
        }
    }

    TmdbSeasonDto dto;
    dto.id = *id;
    dto.seasonNumber = static_cast<int>(*seasonNumber);
    dto.name = name.value_or(std::string{});
    dto.overview = overview.value_or(std::string{});
    dto.airDate = airDate;
    dto.posterPath = posterPath;
    dto.episodes = std::move(episodes);
    return dto;
}

TmdbClient::CollectionResult TmdbClient::parseCollectionResponse(const QJsonObject &object)
{
    TmdbJsonReader reader(object);

    const auto id = reader.requiredInteger(QStringLiteral("id"));
    const auto name = reader.optionalString(QStringLiteral("name"));
    const auto overview = reader.optionalString(QStringLiteral("overview"));
    const auto posterPath = reader.optionalString(QStringLiteral("poster_path"));
    const auto backdropPath = reader.optionalString(QStringLiteral("backdrop_path"));
    const auto rawParts = reader.optionalArray(QStringLiteral("parts"));

    if (reader.hasError())
    {
        return *reader.error();
    }

    std::vector<TmdbCollectionPartDto> parts;
    if (rawParts.has_value())
    {
        parts.reserve(static_cast<std::size_t>(rawParts->size()));
        for (const QJsonValue &value : *rawParts)
        {
            if (!value.isObject())
            {
                continue;
            }

            TmdbJsonReader partReader(value.toObject());
            const auto partId = partReader.requiredInteger(QStringLiteral("id"));
            const auto title = partReader.optionalString(QStringLiteral("title"));
            const auto releaseDate = partReader.optionalString(QStringLiteral("release_date"));
            const auto partPoster = partReader.optionalString(QStringLiteral("poster_path"));
            const auto partBackdrop = partReader.optionalString(QStringLiteral("backdrop_path"));

            if (partReader.hasError() || !partId.has_value() || *partId <= 0)
            {
                continue;
            }

            TmdbCollectionPartDto part;
            part.id = *partId;
            part.title = title.value_or(std::string{});
            if (releaseDate.has_value() && !releaseDate->empty())
            {
                part.releaseDate = releaseDate;
            }
            part.posterPath = partPoster;
            part.backdropPath = partBackdrop;
            parts.push_back(std::move(part));
        }
    }

    TmdbCollectionDto dto;
    dto.id = *id;
    dto.name = name.value_or(std::string{});
    dto.overview = overview.value_or(std::string{});
    dto.posterPath = posterPath;
    dto.backdropPath = backdropPath;
    dto.parts = std::move(parts);
    return dto;
}

TmdbClient::TitleDetailsResult TmdbClient::parseMovieDetailsResponse(
    const QJsonObject &object)
{
    return parseTitleDetails(object,
                             TmdbMediaType::Movie,
                             QStringLiteral("title"),
                             QStringLiteral("release_date"));
}

TmdbClient::TitleDetailsResult TmdbClient::parseTvDetailsResponse(
    const QJsonObject &object)
{
    return parseTitleDetails(object,
                             TmdbMediaType::Tv,
                             QStringLiteral("name"),
                             QStringLiteral("first_air_date"));
}

TmdbClient::RequestId TmdbClient::discover(
    TmdbDiscoverRequestDto request,
    DiscoverCompletionHandler completionHandler)
{
    const RequestKind kind = request.mediaType == TmdbMediaType::Movie
        ? RequestKind::DiscoverMovie
        : RequestKind::DiscoverTv;

    MM_LOG_D() << "TMDB discover requested" << requestKindLabel(kind)
               << "page" << request.page
               << "genres" << request.genreIds.size()
               << "minRating" << request.minimumRating;

    return enqueueDiscoverShapedRequest(m_requestBuilder.build(request),
                                        kind,
                                        std::move(completionHandler));
}

TmdbClient::RequestId TmdbClient::enqueueDiscoverShapedRequest(
    QNetworkRequest networkRequest,
    RequestKind kind,
    DiscoverCompletionHandler completionHandler)
{
    InternalCompletionHandler internalHandler =
        [completionHandler = std::move(completionHandler)](
            InternalResult result) mutable {
        if (!completionHandler)
        {
            return;
        }

        if (auto *page = std::get_if<TmdbDiscoverPageDto>(&result))
        {
            completionHandler(std::move(*page));
            return;
        }

        if (auto *error = std::get_if<TmdbError>(&result))
        {
            completionHandler(std::move(*error));
            return;
        }

        completionHandler(TmdbError(
            TmdbError::Category::Parse,
            "TMDB returned an unexpected discover-shaped response."));
    };

    return enqueue(std::move(networkRequest),
                   kind,
                   std::move(internalHandler));
}

TmdbClient::RequestId TmdbClient::genreList(
    TmdbGenreListRequestDto request,
    GenreListCompletionHandler completionHandler)
{
    const RequestKind kind = request.mediaType == TmdbMediaType::Movie
        ? RequestKind::GenreListMovie
        : RequestKind::GenreListTv;

    MM_LOG_D() << "TMDB genre list requested" << requestKindLabel(kind);

    InternalCompletionHandler internalHandler =
        [completionHandler = std::move(completionHandler)](
            InternalResult result) mutable {
        if (!completionHandler)
        {
            return;
        }

        if (auto *genres = std::get_if<TmdbGenreListResponseDto>(&result))
        {
            completionHandler(std::move(*genres));
            return;
        }

        if (auto *error = std::get_if<TmdbError>(&result))
        {
            completionHandler(std::move(*error));
            return;
        }

        completionHandler(TmdbError(
            TmdbError::Category::Parse,
            "TMDB returned an unexpected genre-list response."));
    };

    return enqueue(m_requestBuilder.build(request),
                   kind,
                   std::move(internalHandler));
}

TmdbClient::RequestId TmdbClient::enqueue(
    QNetworkRequest networkRequest,
    RequestKind kind,
    InternalCompletionHandler completionHandler)
{
    ++m_nextRequestId;
    if (m_nextRequestId == 0)
    {
        ++m_nextRequestId;
    }

    const RequestId requestId = m_nextRequestId;
    MM_LOG_D() << "TMDB request enqueued" << requestId << requestKindLabel(kind)
               << "active" << activeRequestCount()
               << "pending" << m_pendingRequests.size();
    m_pendingRequests.push_back(
        PendingRequest{requestId,
                       std::move(networkRequest),
                       kind,
                       std::move(completionHandler)});
    startQueuedRequests();
    return requestId;
}

void TmdbClient::startQueuedRequests()
{
    if (m_rateLimited) {
        return;
    }

    while (!m_pendingRequests.empty()
           && activeRequestCount() < m_concurrentLimit)
    {
        PendingRequest pendingRequest = std::move(m_pendingRequests.front());
        m_pendingRequests.pop_front();
        startRequest(std::move(pendingRequest));
    }
}

void TmdbClient::startRequest(PendingRequest pendingRequest)
{
    QNetworkReply *reply = m_networkAccessManager.get(
        pendingRequest.networkRequest);
    const RequestId requestId = pendingRequest.id;

    MM_LOG_D() << "TMDB request started" << requestId
               << requestKindLabel(pendingRequest.kind);

    m_activeRequests.insert(
        requestId,
        ActiveRequest{reply,
                      pendingRequest.kind,
                      std::move(pendingRequest.completionHandler)});

    connect(reply, &QObject::destroyed, this, [this, requestId]() {
        handleReplyDestroyed(requestId);
    });

    connect(reply, &QNetworkReply::finished, this, [this, requestId]() {
        finishRequest(requestId);
    });
}

void TmdbClient::finishRequest(RequestId requestId)
{
    const auto active = m_activeRequests.find(requestId);
    if (active == m_activeRequests.end())
    {
        return;
    }

    ActiveRequest activeRequest = std::move(active.value());
    m_activeRequests.erase(active);

    InternalResult result = parseReply(*activeRequest.reply, activeRequest.kind);

    if (auto *error = std::get_if<TmdbError>(&result))
    {
        MM_LOG_W() << "TMDB request failed" << requestId
                   << requestKindLabel(activeRequest.kind)
                   << "category" << static_cast<int>(error->category())
                   << "httpStatus" << error->httpStatus().value_or(0);

        if (error->category() == TmdbError::Category::RateLimited)
        {
            const auto after = error->retryAfter();
            holdFor(after.has_value() ? int(after->count()) : 10);
        }
    }
    else
    {
        MM_LOG_D() << "TMDB request finished" << requestId
                   << requestKindLabel(activeRequest.kind);
    }

    activeRequest.reply->deleteLater();
    startQueuedRequests();
    if (activeRequest.completionHandler)
    {
        activeRequest.completionHandler(std::move(result));
    }
}

void TmdbClient::handleReplyDestroyed(RequestId requestId)
{
    const auto active = m_activeRequests.find(requestId);
    if (active == m_activeRequests.end())
    {
        return;
    }

    MM_LOG_W() << "TMDB reply destroyed before finishing" << requestId
               << requestKindLabel(active->kind);

    InternalCompletionHandler completionHandler =
        std::move(active->completionHandler);
    m_activeRequests.erase(active);
    startQueuedRequests();

    if (completionHandler)
    {
        completionHandler(TmdbError(
            TmdbError::Category::Network,
            "The TMDB request ended before a response was received."));
    }
}

TmdbClient::InternalResult TmdbClient::parseReply(QNetworkReply &reply,
                                                  RequestKind kind)
{
    const QByteArray body = reply.readAll();
    const auto responseStatus = httpStatus(reply);
    const auto tmdbStatus = TmdbReplyError::statusCodeIn(body);
    const bool unsuccessfulHttpStatus = responseStatus.has_value()
        && (*responseStatus < 200 || *responseStatus >= 300);
    const bool tmdbFailure = tmdbStatus.has_value() && *tmdbStatus != 1;

    if (reply.error() != QNetworkReply::NoError
        || unsuccessfulHttpStatus
        || tmdbFailure)
    {
        return mappedReplyError(reply, body);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return TmdbError(TmdbError::Category::Parse,
                         "TMDB returned a response that the app could not read.");
    }

    if (kind == RequestKind::Configuration)
    {
        ConfigurationResult result = parseConfigurationResponse(document.object());
        if (auto *configuration = std::get_if<TmdbConfigurationResponseDto>(&result))
        {
            return std::move(*configuration);
        }
        return std::move(std::get<TmdbError>(result));
    }

    if (kind == RequestKind::GenreListMovie
        || kind == RequestKind::GenreListTv)
    {
        GenreListResult result = parseGenreListResponse(document.object());
        if (auto *genres = std::get_if<TmdbGenreListResponseDto>(&result))
        {
            return std::move(*genres);
        }
        return std::move(std::get<TmdbError>(result));
    }

    if (kind == RequestKind::Videos)
    {
        VideosResult result = parseVideosResponse(document.object());
        if (auto *videos = std::get_if<TmdbVideosResponseDto>(&result))
        {
            return std::move(*videos);
        }
        return std::move(std::get<TmdbError>(result));
    }

    if (kind == RequestKind::WatchProviders)
    {
        WatchProvidersResult result = parseWatchProvidersResponse(document.object());
        if (auto *providers = std::get_if<TmdbWatchProvidersResponseDto>(&result))
        {
            return std::move(*providers);
        }
        return std::move(std::get<TmdbError>(result));
    }

    if (kind == RequestKind::TitleDetailsMovie || kind == RequestKind::TitleDetailsTv)
    {
        TitleDetailsResult result = kind == RequestKind::TitleDetailsMovie
            ? parseMovieDetailsResponse(document.object())
            : parseTvDetailsResponse(document.object());
        if (auto *details = std::get_if<TmdbTitleDetailsDto>(&result))
        {
            return std::move(*details);
        }
        return std::move(std::get<TmdbError>(result));
    }

    if (kind == RequestKind::Season)
    {
        SeasonResult result = parseSeasonResponse(document.object());
        if (auto *season = std::get_if<TmdbSeasonDto>(&result))
        {
            return std::move(*season);
        }
        return std::move(std::get<TmdbError>(result));
    }

    if (kind == RequestKind::Collection)
    {
        CollectionResult result = parseCollectionResponse(document.object());
        if (auto *collection = std::get_if<TmdbCollectionDto>(&result))
        {
            return std::move(*collection);
        }
        return std::move(std::get<TmdbError>(result));
    }

    const bool isMovieKind = kind == RequestKind::DiscoverMovie
        || kind == RequestKind::TrendingMovie
        || kind == RequestKind::PopularMovie
        || kind == RequestKind::SearchMovie;
    const TmdbMediaType mediaType = isMovieKind
        ? TmdbMediaType::Movie
        : TmdbMediaType::Tv;
    DiscoverResult result = parseDiscoverPage(document.object(), mediaType);
    if (auto *page = std::get_if<TmdbDiscoverPageDto>(&result))
    {
        return std::move(*page);
    }
    return std::move(std::get<TmdbError>(result));
}

void TmdbClient::discardAllRequests() noexcept
{
    m_pendingRequests.clear();

    for (auto active = m_activeRequests.begin();
         active != m_activeRequests.end();
         ++active)
    {
        QNetworkReply *reply = active->reply;
        if (reply)
        {
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            reply->deleteLater();
        }
    }
    m_activeRequests.clear();
}

TmdbError TmdbClient::cancelledError()
{
    return TmdbError(TmdbError::Category::Cancelled,
                     "The TMDB request was cancelled.");
}

const char *TmdbClient::requestKindLabel(RequestKind kind) noexcept
{
    switch (kind)
    {
    case RequestKind::DiscoverMovie:
        return "discoverMovie";
    case RequestKind::DiscoverTv:
        return "discoverTv";
    case RequestKind::GenreListMovie:
        return "genreListMovie";
    case RequestKind::GenreListTv:
        return "genreListTv";
    case RequestKind::Configuration:
        return "configuration";
    case RequestKind::TrendingMovie:
        return "trendingMovie";
    case RequestKind::TrendingTv:
        return "trendingTv";
    case RequestKind::PopularMovie:
        return "popularMovie";
    case RequestKind::PopularTv:
        return "popularTv";
    case RequestKind::Videos:
        return "videos";
    case RequestKind::SearchMovie:
        return "searchMovie";
    case RequestKind::SearchTv:
        return "searchTv";
    case RequestKind::WatchProviders:
        return "watchProviders";
    case RequestKind::TitleDetailsMovie:
        return "titleDetailsMovie";
    case RequestKind::TitleDetailsTv:
        return "titleDetailsTv";
    case RequestKind::Season:
        return "season";
    case RequestKind::Collection:
        return "collection";
    }

    return "unknown";
}

}
