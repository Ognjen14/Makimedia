#include "Metadata/TmdbAsk.h"

namespace TmdbAsk {

QString searchKey(const ParsedFileName &parsed, bool useYear)
{
    const int year = (useYear && parsed.year > 0) ? parsed.year : 0;
    return (parsed.looksLikeEpisode() ? QStringLiteral("tv:")
                                      : QStringLiteral("movie:"))
           + parsed.title.toLower()
           + QLatin1Char('|') + QString::number(year);
}

QString detailsKey(qint64 tmdbId, const QString &kind)
{
    return QStringLiteral("%1/%2").arg(tmdbId).arg(kind);
}

QString seasonKey(qint64 tmdbId, int seasonNumber)
{
    return QStringLiteral("%1/%2").arg(tmdbId).arg(seasonNumber);
}

QString collectionKey(qint64 collectionId)
{
    return QStringLiteral("collection/%1").arg(collectionId);
}

bool isDefinitive(const std::optional<int> &httpStatus)
{
    return httpStatus.has_value() && *httpStatus == 404;
}

}
