#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Makimedia::Tmdb
{

enum class
    TmdbMediaType : std::uint8_t
{
    Movie,
    Tv
};

enum class TmdbGenreMatchMode : std::uint8_t
{
    Or,
    And
};

using TmdbId = std::int64_t;
using TmdbGenreId = std::int32_t;
using TmdbPageNumber = std::uint32_t;
using TmdbResultCount = std::uint64_t;

struct TmdbGenreDto final
{
    TmdbGenreId id{0};
    std::string name;
};

struct TmdbDiscoverRequestDto final
{
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    TmdbPageNumber page{1};
    std::optional<std::string> language;
    std::optional<int> minimumYear;
    std::optional<int> maximumYear;
    double minimumRating{0.0};
    std::vector<TmdbGenreId> genreIds;
    TmdbGenreMatchMode genreMatchMode{TmdbGenreMatchMode::Or};
    std::optional<std::string> originalLanguage{"en"};
};

struct TmdbConfigurationRequestDto final
{
};

struct TmdbGenreListRequestDto final
{
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    std::optional<std::string> language;
};

struct TmdbTrendingRequestDto final
{
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    TmdbPageNumber page{1};
};

struct TmdbPopularRequestDto final
{
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    TmdbPageNumber page{1};
};

struct TmdbSearchRequestDto final
{
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    std::string query;
    TmdbPageNumber page{1};
    std::optional<int> year;
};

struct TmdbVideosRequestDto final
{
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    TmdbId id{0};
    std::optional<std::string> language;
};

struct TmdbVideoDto final
{
    std::string key;
    std::string site;
    std::string type;
    bool official{false};
};

struct TmdbVideosResponseDto final
{
    std::vector<TmdbVideoDto> results;
};

struct TmdbWatchProvidersRequestDto final
{
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    TmdbId id{0};
};

struct TmdbWatchProviderDto final
{
    TmdbId providerId{0};
    std::string providerName;
    std::string logoPath;
};

struct TmdbWatchProvidersResponseDto final
{
    std::vector<TmdbWatchProviderDto> providers;
};

struct TmdbTitleDetailsRequestDto final
{
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    TmdbId id{0};
};

struct TmdbSeasonRequestDto final
{
    TmdbId showId{0};
    int seasonNumber{1};
    std::optional<std::string> language;
};

struct TmdbCreditDto final
{
    std::string name;
    std::string role;
    std::optional<std::string> profilePath;
};

struct TmdbEpisodeDto final
{
    TmdbId id{0};
    int seasonNumber{0};
    int episodeNumber{0};
    std::string name;
    std::string overview;
    std::optional<std::string> airDate;
    std::optional<std::string> stillPath;
    int runtimeMinutes{0};
    double rating{0.0};
    std::vector<TmdbCreditDto> directors;
    std::vector<TmdbCreditDto> writers;
};

struct TmdbSeasonDto final
{
    TmdbId id{0};
    TmdbId showId{0};
    int seasonNumber{0};
    std::string name;
    std::string overview;
    std::optional<std::string> airDate;
    std::optional<std::string> posterPath;
    std::vector<TmdbEpisodeDto> episodes;
};

struct TmdbCollectionRefDto final
{
    TmdbId id{0};
    std::string name;
    std::optional<std::string> posterPath;
    std::optional<std::string> backdropPath;
};

struct TmdbCollectionRequestDto final
{
    TmdbId id{0};
};

struct TmdbCollectionPartDto final
{
    TmdbId id{0};
    std::string title;
    std::optional<std::string> releaseDate;
    std::optional<std::string> posterPath;
    std::optional<std::string> backdropPath;
};

struct TmdbCollectionDto final
{
    TmdbId id{0};
    std::string name;
    std::string overview;
    std::optional<std::string> posterPath;
    std::optional<std::string> backdropPath;
    std::vector<TmdbCollectionPartDto> parts;
};

struct TmdbTitleDetailsDto final
{
    TmdbId id{0};
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    std::string title;
    std::optional<std::string> releaseDate;
    std::vector<std::string> genreNames;
    double rating{0.0};
    std::string overview;
    std::optional<std::string> posterPath;
    std::optional<std::string> backdropPath;
    int runtimeMinutes{0};
    std::optional<TmdbCollectionRefDto> collection;

    std::string certification;

    std::vector<TmdbCreditDto> cast;
    std::vector<TmdbCreditDto> directors;
    std::vector<TmdbCreditDto> writers;
    std::vector<TmdbCreditDto> creators;
};

struct TmdbTitleResultDto final
{
    TmdbId id{0};
    TmdbMediaType mediaType{TmdbMediaType::Movie};
    std::string title;
    std::string originalTitle;
    std::optional<std::string> releaseDate;
    std::vector<TmdbGenreId> genreIds;
    double rating{0.0};
    std::int64_t voteCount{0};
    double popularity{0.0};
    std::string overview;
    std::optional<std::string> posterPath;
};

struct TmdbDiscoverPageDto final
{
    std::vector<TmdbTitleResultDto> results;
    TmdbPageNumber page{1};
    TmdbPageNumber totalPages{1};
    TmdbResultCount totalResults{0};
};

struct TmdbImageConfigurationDto final
{
    std::string baseUrl;
    std::string secureBaseUrl;
    std::vector<std::string> posterSizes;

    std::vector<std::string> stillSizes;

    std::vector<std::string> backdropSizes;

    std::vector<std::string> profileSizes;
};

struct TmdbConfigurationResponseDto final
{
    TmdbImageConfigurationDto images;
};

struct TmdbGenreListResponseDto final
{
    std::vector<TmdbGenreDto> genres;
};

}
