#pragma once

#include "TmdbDtos.h"
#include "TmdbError.h"
#include "TmdbRequestBuilder.h"

#include <QJsonObject>
#include <QHash>
#include <QNetworkRequest>
#include <QObject>
#include <QTimer>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <variant>

class QNetworkAccessManager;
class QNetworkReply;

namespace Makimedia::Tmdb
{

class TmdbClient final : public QObject
{
public:
    using TitleParseResult = std::variant<TmdbTitleResultDto, TmdbError>;
    using DiscoverResult = std::variant<TmdbDiscoverPageDto, TmdbError>;
    using DiscoverCompletionHandler = std::function<void(DiscoverResult)>;
    using ConfigurationResult = std::variant<TmdbConfigurationResponseDto, TmdbError>;
    using ConfigurationCompletionHandler = std::function<void(ConfigurationResult)>;
    using GenreListResult = std::variant<TmdbGenreListResponseDto, TmdbError>;
    using GenreListCompletionHandler = std::function<void(GenreListResult)>;
    using VideosResult = std::variant<TmdbVideosResponseDto, TmdbError>;
    using VideosCompletionHandler = std::function<void(VideosResult)>;
    using WatchProvidersResult = std::variant<TmdbWatchProvidersResponseDto, TmdbError>;
    using WatchProvidersCompletionHandler = std::function<void(WatchProvidersResult)>;
    using TitleDetailsResult = std::variant<TmdbTitleDetailsDto, TmdbError>;
    using TitleDetailsCompletionHandler = std::function<void(TitleDetailsResult)>;
    using SeasonResult = std::variant<TmdbSeasonDto, TmdbError>;
    using SeasonCompletionHandler = std::function<void(SeasonResult)>;
    using CollectionResult = std::variant<TmdbCollectionDto, TmdbError>;
    using CollectionCompletionHandler = std::function<void(CollectionResult)>;
    using RequestId = std::uint64_t;

    static constexpr std::size_t MaximumConcurrentRequests = 8;

    TmdbClient(QNetworkAccessManager &networkAccessManager,
               const TmdbRequestBuilder &requestBuilder,
               QObject *parent = nullptr);
    ~TmdbClient() override;

    TmdbClient(const TmdbClient &) = delete;
    TmdbClient &operator=(const TmdbClient &) = delete;

    [[nodiscard]] RequestId discoverMovie(
        TmdbDiscoverRequestDto request,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId discoverTv(
        TmdbDiscoverRequestDto request,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId configuration(
        ConfigurationCompletionHandler completionHandler);
    [[nodiscard]] RequestId movieGenres(
        std::optional<std::string> language,
        GenreListCompletionHandler completionHandler);
    [[nodiscard]] RequestId tvGenres(
        std::optional<std::string> language,
        GenreListCompletionHandler completionHandler);
    [[nodiscard]] RequestId trendingMovie(
        TmdbPageNumber page,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId trendingTv(
        TmdbPageNumber page,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId popularMovie(
        TmdbPageNumber page,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId popularTv(
        TmdbPageNumber page,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId videos(
        TmdbMediaType mediaType,
        TmdbId id,
        VideosCompletionHandler completionHandler);
    [[nodiscard]] RequestId watchProviders(
        TmdbMediaType mediaType,
        TmdbId id,
        WatchProvidersCompletionHandler completionHandler);
    [[nodiscard]] RequestId titleDetails(
        TmdbMediaType mediaType,
        TmdbId id,
        TitleDetailsCompletionHandler completionHandler);
    [[nodiscard]] RequestId season(
        TmdbId showId,
        int seasonNumber,
        std::optional<std::string> language,
        SeasonCompletionHandler completionHandler);
    [[nodiscard]] RequestId collection(
        TmdbId collectionId,
        CollectionCompletionHandler completionHandler);
    [[nodiscard]] RequestId searchMovie(
        TmdbSearchRequestDto request,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId searchTv(
        TmdbSearchRequestDto request,
        DiscoverCompletionHandler completionHandler);

    [[nodiscard]] bool cancelRequest(RequestId requestId);
    void cancelAllRequests();

    [[nodiscard]] std::size_t activeRequestCount() const noexcept;
    [[nodiscard]] std::size_t pendingRequestCount() const noexcept;

    void setConcurrentRequestLimit(std::size_t limit);

    [[nodiscard]] static TitleParseResult parseMovieResult(
        const QJsonObject &object);
    [[nodiscard]] static TitleParseResult parseTvResult(
        const QJsonObject &object);
    [[nodiscard]] static DiscoverResult parseDiscoverPage(
        const QJsonObject &object,
        TmdbMediaType mediaType);
    [[nodiscard]] static ConfigurationResult parseConfigurationResponse(
        const QJsonObject &object);
    [[nodiscard]] static GenreListResult parseGenreListResponse(
        const QJsonObject &object);
    [[nodiscard]] static VideosResult parseVideosResponse(
        const QJsonObject &object);
    [[nodiscard]] static WatchProvidersResult parseWatchProvidersResponse(
        const QJsonObject &object);
    [[nodiscard]] static TitleDetailsResult parseMovieDetailsResponse(
        const QJsonObject &object);
    [[nodiscard]] static TitleDetailsResult parseTvDetailsResponse(
        const QJsonObject &object);
    [[nodiscard]] static SeasonResult parseSeasonResponse(
        const QJsonObject &object);
    [[nodiscard]] static CollectionResult parseCollectionResponse(
        const QJsonObject &object);

private:
    enum class RequestKind
    {
        DiscoverMovie,
        DiscoverTv,
        GenreListMovie,
        GenreListTv,
        Configuration,
        TrendingMovie,
        TrendingTv,
        PopularMovie,
        PopularTv,
        Videos,
        SearchMovie,
        SearchTv,
        WatchProviders,
        TitleDetailsMovie,
        TitleDetailsTv,
        Season,
        Collection
    };

    using InternalResult = std::variant<TmdbDiscoverPageDto,
                                        TmdbGenreListResponseDto,
                                        TmdbConfigurationResponseDto,
                                        TmdbVideosResponseDto,
                                        TmdbWatchProvidersResponseDto,
                                        TmdbTitleDetailsDto,
                                        TmdbSeasonDto,
                                        TmdbCollectionDto,
                                        TmdbError>;
    using InternalCompletionHandler = std::function<void(InternalResult)>;

    struct PendingRequest final
    {
        RequestId id{0};
        QNetworkRequest networkRequest;
        RequestKind kind{RequestKind::DiscoverMovie};
        InternalCompletionHandler completionHandler;
    };

    struct ActiveRequest final
    {
        QNetworkReply *reply{nullptr};
        RequestKind kind{RequestKind::DiscoverMovie};
        InternalCompletionHandler completionHandler;
    };

    [[nodiscard]] RequestId discover(
        TmdbDiscoverRequestDto request,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId genreList(
        TmdbGenreListRequestDto request,
        GenreListCompletionHandler completionHandler);
    [[nodiscard]] RequestId enqueueDiscoverShapedRequest(
        QNetworkRequest networkRequest,
        RequestKind kind,
        DiscoverCompletionHandler completionHandler);
    [[nodiscard]] RequestId enqueue(
        QNetworkRequest networkRequest,
        RequestKind kind,
        InternalCompletionHandler completionHandler);
    void startQueuedRequests();

    void holdFor(int seconds);
    void startRequest(PendingRequest pendingRequest);
    void finishRequest(RequestId requestId);
    void handleReplyDestroyed(RequestId requestId);
    void discardAllRequests() noexcept;

    [[nodiscard]] static InternalResult parseReply(
        QNetworkReply &reply,
        RequestKind kind);
    [[nodiscard]] static TmdbError cancelledError();
    [[nodiscard]] static const char *requestKindLabel(RequestKind kind) noexcept;

    QNetworkAccessManager &m_networkAccessManager;
    const TmdbRequestBuilder &m_requestBuilder;
    std::deque<PendingRequest> m_pendingRequests;
    QTimer m_rateLimitTimer;
    bool m_rateLimited = false;
    std::size_t m_concurrentLimit = MaximumConcurrentRequests;
    QHash<RequestId, ActiveRequest> m_activeRequests;
    RequestId m_nextRequestId{0};
};

}
