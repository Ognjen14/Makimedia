#include "Metadata/TmdbReplyError.h"

#include "Metadata/TmdbJsonReader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>
#include <limits>

namespace Makimedia::Tmdb::TmdbReplyError
{

std::optional<TmdbError::TmdbStatusCode> statusCodeIn(const QByteArray &body)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    TmdbJsonReader reader(document.object());
    const auto statusCode = reader.optionalInteger(QStringLiteral("status_code"));
    if (reader.hasError()
        || !statusCode.has_value()
        || *statusCode < std::numeric_limits<TmdbError::TmdbStatusCode>::lowest()
        || *statusCode > std::numeric_limits<TmdbError::TmdbStatusCode>::max())
    {
        return std::nullopt;
    }

    return static_cast<TmdbError::TmdbStatusCode>(*statusCode);
}

std::optional<TmdbError::RetryAfter> retryAfterFrom(const QByteArray &header,
                                                    const QDateTime &now)
{
    if (header.isEmpty())
    {
        return std::nullopt;
    }

    bool secondsOk = false;
    const qint64 seconds = header.trimmed().toLongLong(&secondsOk);
    if (secondsOk && seconds >= 0)
    {
        return TmdbError::RetryAfter(seconds);
    }

    const QDateTime retryDate = QDateTime::fromString(
        QString::fromLatin1(header),
        Qt::RFC2822Date);
    if (!retryDate.isValid())
    {
        return std::nullopt;
    }

    const qint64 secondsUntilRetry = std::max<qint64>(
        0,
        now.toUTC().secsTo(retryDate.toUTC()));
    return TmdbError::RetryAfter(secondsUntilRetry);
}

TmdbError from(QNetworkReply::NetworkError networkError,
               std::optional<TmdbError::HttpStatus> httpStatus,
               std::optional<TmdbError::TmdbStatusCode> tmdbStatusCode,
               std::optional<TmdbError::RetryAfter> retryAfter)
{
    if (httpStatus == 429 || tmdbStatusCode == 25)
    {
        return TmdbError(TmdbError::Category::RateLimited,
                         "TMDB is temporarily limiting requests. Try again shortly.",
                         httpStatus,
                         tmdbStatusCode,
                         retryAfter);
    }

    switch (networkError)
    {
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::ProxyNotFoundError:
        return TmdbError(TmdbError::Category::Dns,
                         "The TMDB server address could not be found.",
                         httpStatus,
                         tmdbStatusCode);
    case QNetworkReply::TimeoutError:
    case QNetworkReply::ProxyTimeoutError:
        return TmdbError(TmdbError::Category::Timeout,
                         "The TMDB request timed out.",
                         httpStatus,
                         tmdbStatusCode);
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        return TmdbError(TmdbError::Category::Offline,
                         "A network connection is not currently available.",
                         httpStatus,
                         tmdbStatusCode);
    case QNetworkReply::OperationCanceledError:
        return TmdbError(TmdbError::Category::Cancelled,
                         "The TMDB request was cancelled.",
                         httpStatus,
                         tmdbStatusCode);
    default:
        break;
    }

    if (tmdbStatusCode.has_value() && *tmdbStatusCode != 1)
    {
        return TmdbError(TmdbError::Category::Tmdb,
                         "TMDB rejected the request.",
                         httpStatus,
                         tmdbStatusCode);
    }

    if (httpStatus.has_value() && (*httpStatus < 200 || *httpStatus >= 300))
    {
        return TmdbError(TmdbError::Category::Http,
                         "TMDB could not complete the request.",
                         httpStatus);
    }

    return TmdbError(TmdbError::Category::Network,
                     "The app could not connect to TMDB.",
                     httpStatus,
                     tmdbStatusCode);
}

}
