#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace Makimedia::Tmdb
{

class TmdbError final
{
public:
    enum class Category
    {
        Offline,
        Timeout,
        Dns,
        Network,
        Http,
        RateLimited,
        Parse,
        Tmdb,
        Cancelled,
        Unknown
    };

    using HttpStatus = int;
    using TmdbStatusCode = int;
    using RetryAfter = std::chrono::seconds;

    explicit TmdbError(
        Category category,
        std::string userSafeContext,
        std::optional<HttpStatus> httpStatus = std::nullopt,
        std::optional<TmdbStatusCode> tmdbStatusCode = std::nullopt,
        std::optional<RetryAfter> retryAfter = std::nullopt);

    [[nodiscard]] Category category() const noexcept;
    [[nodiscard]] const std::optional<HttpStatus> &httpStatus() const noexcept;
    [[nodiscard]] const std::optional<TmdbStatusCode> &tmdbStatusCode() const noexcept;
    [[nodiscard]] const std::optional<RetryAfter> &retryAfter() const noexcept;
    [[nodiscard]] const std::string &userSafeContext() const noexcept;

private:
    Category m_category;
    std::optional<HttpStatus> m_httpStatus;
    std::optional<TmdbStatusCode> m_tmdbStatusCode;
    std::optional<RetryAfter> m_retryAfter;
    std::string m_userSafeContext;
};

}
