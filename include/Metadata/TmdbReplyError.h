#pragma once

#include "TmdbError.h"

#include <QByteArray>
#include <QDateTime>
#include <QNetworkReply>

#include <optional>

namespace Makimedia::Tmdb::TmdbReplyError
{

[[nodiscard]] std::optional<TmdbError::TmdbStatusCode> statusCodeIn(
    const QByteArray &body);

[[nodiscard]] std::optional<TmdbError::RetryAfter> retryAfterFrom(
    const QByteArray &header,
    const QDateTime &now);

[[nodiscard]] TmdbError from(
    QNetworkReply::NetworkError networkError,
    std::optional<TmdbError::HttpStatus> httpStatus,
    std::optional<TmdbError::TmdbStatusCode> tmdbStatusCode,
    std::optional<TmdbError::RetryAfter> retryAfter);

}
