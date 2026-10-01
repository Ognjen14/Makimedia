#include "Metadata/TmdbError.h"

#include <utility>

namespace Makimedia::Tmdb
{

TmdbError::TmdbError(Category category,
                     std::string userSafeContext,
                     std::optional<HttpStatus> httpStatus,
                     std::optional<TmdbStatusCode> tmdbStatusCode,
                     std::optional<RetryAfter> retryAfter)
    : m_category(category)
    , m_httpStatus(httpStatus)
    , m_tmdbStatusCode(tmdbStatusCode)
    , m_retryAfter(retryAfter)
    , m_userSafeContext(std::move(userSafeContext))
{
}

TmdbError::Category TmdbError::category() const noexcept
{
    return m_category;
}

const std::optional<TmdbError::HttpStatus> &TmdbError::httpStatus() const noexcept
{
    return m_httpStatus;
}

const std::optional<TmdbError::TmdbStatusCode> &TmdbError::tmdbStatusCode() const noexcept
{
    return m_tmdbStatusCode;
}

const std::optional<TmdbError::RetryAfter> &TmdbError::retryAfter() const noexcept
{
    return m_retryAfter;
}

const std::string &TmdbError::userSafeContext() const noexcept
{
    return m_userSafeContext;
}

}
