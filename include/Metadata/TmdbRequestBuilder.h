#pragma once

#include "TmdbDtos.h"
#include "TmdbRuntimeConfig.h"

#include <QNetworkRequest>

namespace Makimedia::Tmdb
{

class TmdbRequestBuilder final
{
public:
    explicit TmdbRequestBuilder(const TmdbRuntimeConfig &runtimeConfig) noexcept;

    [[nodiscard]] QNetworkRequest build(
        const TmdbDiscoverRequestDto &request) const;

    [[nodiscard]] QNetworkRequest build(
        const TmdbConfigurationRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbGenreListRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbTrendingRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbPopularRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbVideosRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbSearchRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbWatchProvidersRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbTitleDetailsRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbSeasonRequestDto &request) const;
    [[nodiscard]] QNetworkRequest build(
        const TmdbCollectionRequestDto &request) const;

private:
    const TmdbRuntimeConfig &m_runtimeConfig;
};

}
