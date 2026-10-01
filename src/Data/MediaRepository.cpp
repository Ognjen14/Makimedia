#include "Data/MediaRepository.h"

#include "Data/Database.h"
#include "MmLog.h"
#include "TextFold.h"

#include <algorithm>
#include <utility>

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace {

struct MediaColumns
{
    explicit MediaColumns(const QSqlQuery &query)
    {
        const QSqlRecord record = query.record();
        id = record.indexOf(QStringLiteral("id"));
        tmdbId = record.indexOf(QStringLiteral("tmdb_id"));
        kind = record.indexOf(QStringLiteral("kind"));
        title = record.indexOf(QStringLiteral("title"));
        originalTitle = record.indexOf(QStringLiteral("original_title"));
        year = record.indexOf(QStringLiteral("year"));
        overview = record.indexOf(QStringLiteral("overview"));
        rating = record.indexOf(QStringLiteral("rating"));
        certification = record.indexOf(QStringLiteral("certification"));
        runtimeMinutes = record.indexOf(QStringLiteral("runtime_minutes"));
        genres = record.indexOf(QStringLiteral("genres"));
        posterPath = record.indexOf(QStringLiteral("poster_path"));
        backdropPath = record.indexOf(QStringLiteral("backdrop_path"));
    }

    int id, tmdbId, kind, title, originalTitle, year, overview, rating;
    int certification, runtimeMinutes, genres, posterPath, backdropPath;
};

MediaRecord mediaFromQuery(const QSqlQuery &query, const MediaColumns &at)
{
    MediaRecord record;
    record.id = query.value(at.id).toLongLong();
    record.tmdbId = query.value(at.tmdbId).toLongLong();
    record.kind = query.value(at.kind).toString();
    record.title = query.value(at.title).toString();
    record.originalTitle = query.value(at.originalTitle).toString();
    record.year = query.value(at.year).toInt();
    record.overview = query.value(at.overview).toString();
    record.rating = query.value(at.rating).toDouble();
    record.certification = query.value(at.certification).toString();
    record.runtimeMinutes = query.value(at.runtimeMinutes).toInt();
    record.genres = query.value(at.genres).toString();
    record.posterPath = query.value(at.posterPath).toString();
    record.backdropPath = query.value(at.backdropPath).toString();
    return record;
}

MediaRecord mediaFromQuery(const QSqlQuery &query)
{
    return mediaFromQuery(query, MediaColumns(query));
}

const QString kListedSelect = QStringLiteral(
    "SELECT m.*, "
    "  (SELECT COUNT(*) FROM file_media fm"
    "     WHERE fm.media_id = m.id) AS file_count, "
    "  (SELECT COUNT(*) FROM file_media fm"
    "     WHERE fm.media_id = m.id AND fm.suggested = 1) AS suggested_count, "
    "  (SELECT COUNT(DISTINCT e.season) FROM episodes e"
    "     WHERE e.media_id = m.id AND e.file_id IS NOT NULL) AS season_count, "
    "  (SELECT COALESCE(MIN(CASE WHEN f.missing = 0 THEN f.handle END),"
    "                   MIN(f.handle)) FROM file_media fm"
    "     JOIN files f ON f.id = fm.file_id"
    "     WHERE fm.media_id = m.id) AS first_handle, "
    "  (SELECT COUNT(*) FROM file_media fm"
    "     JOIN playback_state p ON p.file_id = fm.file_id"
    "     WHERE fm.media_id = m.id AND p.watched = 1) AS watched_count, "
    "  (SELECT MAX(CASE WHEN p.duration_seconds > 0"
    "                   THEN p.position_seconds / p.duration_seconds"
    "                   ELSE 0 END)"
    "     FROM file_media fm"
    "     JOIN playback_state p ON p.file_id = fm.file_id"
    "     WHERE fm.media_id = m.id) AS max_progress "
    "FROM media m ");

enum class ListOrder { ByTitle, AsQueried };

QList<MediaRecord> listedFromQuery(QSqlQuery &query, ListOrder order = ListOrder::ByTitle)
{
    QList<MediaRecord> result;

    const MediaColumns at(query);
    const QSqlRecord columns = query.record();
    const int fileCount = columns.indexOf(QStringLiteral("file_count"));
    const int suggestedCount = columns.indexOf(QStringLiteral("suggested_count"));
    const int seasonCount = columns.indexOf(QStringLiteral("season_count"));
    const int firstHandle = columns.indexOf(QStringLiteral("first_handle"));
    const int watchedCount = columns.indexOf(QStringLiteral("watched_count"));
    const int maxProgress = columns.indexOf(QStringLiteral("max_progress"));

    QList<std::pair<QString, MediaRecord>> keyed;
    while (query.next()) {
        MediaRecord record = mediaFromQuery(query, at);
        record.fileCount = query.value(fileCount).toInt();
        record.suggestedFileCount = query.value(suggestedCount).toInt();
        record.seasonCount = query.value(seasonCount).toInt();
        record.firstFileHandle = query.value(firstHandle).toString();
        record.watchedCount = query.value(watchedCount).toInt();
        record.partialProgress = query.value(maxProgress).toDouble();
        keyed.append({TextFold::key(record.title), record});
    }

    if (order == ListOrder::ByTitle) {
        std::sort(keyed.begin(), keyed.end(),
                  [](const std::pair<QString, MediaRecord> &a,
                     const std::pair<QString, MediaRecord> &b) {
            if (a.first != b.first) {
                return a.first < b.first;
            }
            const int exact = a.second.title.compare(b.second.title);
            if (exact != 0) {
                return exact < 0;
            }
            return a.second.id < b.second.id;
        });
    }

    result.reserve(keyed.size());
    for (const auto &entry : std::as_const(keyed)) {
        result.append(entry.second);
    }
    return result;
}

const QString kListedAggregateSelect = QStringLiteral(
    "WITH counts AS ("
    "  SELECT media_id, COUNT(*) AS file_count, SUM(suggested) AS suggested_count"
    "  FROM file_media GROUP BY media_id),"
    " handles AS ("
    "  SELECT fm.media_id,"
    "         COALESCE(MIN(CASE WHEN f.missing = 0 THEN f.handle END),"
    "                  MIN(f.handle)) AS first_handle"
    "  FROM file_media fm JOIN files f ON f.id = fm.file_id"
    "  GROUP BY fm.media_id),"
    " seasons AS ("
    "  SELECT media_id, COUNT(DISTINCT season) AS season_count"
    "  FROM episodes WHERE file_id IS NOT NULL GROUP BY media_id),"
    " progress AS ("
    "  SELECT fm.media_id,"
    "         SUM(CASE WHEN p.watched = 1 THEN 1 ELSE 0 END) AS watched_count,"
    "         MAX(CASE WHEN p.duration_seconds > 0"
    "                  THEN p.position_seconds / p.duration_seconds"
    "                  ELSE 0 END) AS max_progress"
    "  FROM file_media fm JOIN playback_state p ON p.file_id = fm.file_id"
    "  GROUP BY fm.media_id) "
    "SELECT m.*,"
    "  COALESCE(counts.file_count, 0) AS file_count,"
    "  COALESCE(counts.suggested_count, 0) AS suggested_count,"
    "  COALESCE(seasons.season_count, 0) AS season_count,"
    "  handles.first_handle AS first_handle,"
    "  COALESCE(progress.watched_count, 0) AS watched_count,"
    "  progress.max_progress AS max_progress "
    "FROM media m"
    " LEFT JOIN counts ON counts.media_id = m.id"
    " LEFT JOIN handles ON handles.media_id = m.id"
    " LEFT JOIN seasons ON seasons.media_id = m.id"
    " LEFT JOIN progress ON progress.media_id = m.id ");

}

MediaRepository::MediaRepository(Database &database)
    : m_database(database)
{
}

qint64 MediaRepository::upsertMedia(const MediaRecord &record)
{
    if (record.tmdbId <= 0 || record.kind.isEmpty()) {
        MM_LOG_W() << "refusing to store media without a tmdb id and kind";
        return -1;
    }

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO media (tmdb_id, kind, title, title_key, original_title, year,"
        "                   overview, rating, certification, runtime_minutes,"
        "                   genres, poster_path, backdrop_path, updated) "
        "VALUES (:tmdb, :kind, :title, :title_key, :original, :year, :overview,"
        "        :rating, :certification, :runtime, :genres, :poster,"
        "        :backdrop, :updated) "
        "ON CONFLICT(tmdb_id, kind) DO UPDATE SET "
        "  title = excluded.title,"
        "  title_key = excluded.title_key,"
        "  original_title = excluded.original_title,"
        "  year = excluded.year,"
        "  overview = excluded.overview,"
        "  rating = excluded.rating,"
        "  certification = CASE WHEN excluded.certification <> ''"
        "                       THEN excluded.certification"
        "                       ELSE certification END,"
        "  runtime_minutes = CASE WHEN excluded.runtime_minutes > 0"
        "                         THEN excluded.runtime_minutes"
        "                         ELSE runtime_minutes END,"
        "  genres = CASE WHEN excluded.genres <> ''"
        "                THEN excluded.genres ELSE genres END,"
        "  poster_path = CASE WHEN excluded.poster_path <> ''"
        "                     THEN excluded.poster_path ELSE poster_path END,"
        "  backdrop_path = CASE WHEN excluded.backdrop_path <> ''"
        "                       THEN excluded.backdrop_path"
        "                       ELSE backdrop_path END,"
        "  updated = excluded.updated "
        "RETURNING id"));

    query.bindValue(QStringLiteral(":tmdb"), record.tmdbId);
    query.bindValue(QStringLiteral(":kind"), record.kind);
    query.bindValue(QStringLiteral(":title"), record.title);
    query.bindValue(QStringLiteral(":title_key"), TextFold::key(record.title));
    query.bindValue(QStringLiteral(":original"), record.originalTitle);
    query.bindValue(QStringLiteral(":year"), record.year);
    query.bindValue(QStringLiteral(":overview"), record.overview);
    query.bindValue(QStringLiteral(":rating"), record.rating);
    query.bindValue(QStringLiteral(":certification"), record.certification);
    query.bindValue(QStringLiteral(":runtime"), record.runtimeMinutes);
    query.bindValue(QStringLiteral(":genres"), record.genres);
    query.bindValue(QStringLiteral(":poster"), record.posterPath);
    query.bindValue(QStringLiteral(":backdrop"), record.backdropPath);
    query.bindValue(QStringLiteral(":updated"),
                    QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not store media" << record.tmdbId
                   << query.lastError().text();
        return -1;
    }

    if (query.next()) {
        const qint64 id = query.value(0).toLongLong();
        query.finish();
        if (id > 0) {
            return id;
        }
    }
    query.finish();

    const MediaRecord stored = mediaByTmdbId(record.tmdbId, record.kind);
    return stored.isValid() ? stored.id : -1;
}

MediaRecord MediaRepository::mediaByTmdbId(qint64 tmdbId, const QString &kind) const
{
    MediaRecord record;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT * FROM media WHERE tmdb_id = :tmdb AND kind = :kind"));
    query.bindValue(QStringLiteral(":tmdb"), tmdbId);
    query.bindValue(QStringLiteral(":kind"), kind);

    if (query.exec() && query.next()) {
        record = mediaFromQuery(query);
    }
    return record;
}

MediaRecord MediaRepository::mediaForFile(qint64 fileId) const
{
    MediaRecord record;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT media.* FROM media "
        "JOIN file_media ON file_media.media_id = media.id "
        "WHERE file_media.file_id = :file"));
    query.bindValue(QStringLiteral(":file"), fileId);

    if (query.exec() && query.next()) {
        record = mediaFromQuery(query);
    }
    return record;
}

MediaRecord MediaRepository::mediaById(qint64 mediaId) const
{
    MediaRecord record;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT * FROM media WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), mediaId);

    if (query.exec() && query.next()) {
        record = mediaFromQuery(query);
    }
    return record;
}

QList<MediaRecord> MediaRepository::withSuggestions() const
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(kListedAggregateSelect + QStringLiteral(
            "WHERE counts.suggested_count > 0"
            "  AND handles.first_handle IS NOT NULL"))) {
        MM_LOG_E() << "could not list suggested titles"
                   << query.lastError().text();
        return QList<MediaRecord>();
    }

    const QList<MediaRecord> result = listedFromQuery(query);
    MM_LOG_D() << "suggested titles" << result.size();
    return result;
}

QList<MediaRecord> MediaRepository::listedByIds(const QList<qint64> &ids) const
{
    constexpr qsizetype kChunk = 500;

    QList<MediaRecord> result;
    for (qsizetype start = 0; start < ids.size(); start += kChunk) {
        const qsizetype end = qMin(ids.size(), start + kChunk);
        QStringList numbers;
        numbers.reserve(end - start);
        for (qsizetype i = start; i < end; ++i) {
            numbers.append(QString::number(ids.at(i)));
        }

        QSqlQuery query(m_database.handle());
        if (!query.exec(kListedSelect + QStringLiteral("WHERE m.id IN (")
                        + numbers.join(QLatin1Char(',')) + QLatin1Char(')'))) {
            MM_LOG_E() << "could not read" << numbers.size() << "titles by id"
                       << query.lastError().text();
            continue;
        }
        result += listedFromQuery(query);
    }
    return result;
}

QList<MediaRecord> MediaRepository::recentlyAddedTitles(int limit, qint64 freshSeconds) const
{
    if (limit <= 0) {
        return QList<MediaRecord>();
    }

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "WITH added AS ("
        "  SELECT fm.media_id, MAX(f.added) AS last_added"
        "  FROM file_media fm JOIN files f ON f.id = fm.file_id"
        "  WHERE fm.suggested = 0 AND f.missing = 0"
        "  GROUP BY fm.media_id) "
        "SELECT a.media_id, a.last_added,"
        "  (SELECT COUNT(*) FROM file_media fm JOIN files f ON f.id = fm.file_id"
        "     WHERE fm.media_id = a.media_id AND fm.suggested = 0 AND f.missing = 0"
        "       AND f.added >= a.last_added - :fresh) AS new_count "
        "FROM added a "
        "ORDER BY a.last_added DESC, a.media_id DESC "
        "LIMIT :limit"));
    query.bindValue(QStringLiteral(":fresh"), freshSeconds);
    query.bindValue(QStringLiteral(":limit"), limit);

    if (!query.exec()) {
        MM_LOG_E() << "could not list recently added titles" << query.lastError().text();
        return QList<MediaRecord>();
    }

    QList<qint64> order;
    QHash<qint64, std::pair<qint64, int>> added;
    while (query.next()) {
        const qint64 mediaId = query.value(0).toLongLong();
        order.append(mediaId);
        added.insert(mediaId, {query.value(1).toLongLong(), query.value(2).toInt()});
    }

    QHash<qint64, MediaRecord> byId;
    const QList<MediaRecord> listed = listedByIds(order);
    for (const MediaRecord &record : listed) {
        byId.insert(record.id, record);
    }

    QList<MediaRecord> result;
    result.reserve(order.size());
    for (const qint64 mediaId : std::as_const(order)) {
        auto found = byId.find(mediaId);
        if (found == byId.end()) {
            continue;
        }
        MediaRecord record = found.value();
        record.lastAdded = added.value(mediaId).first;
        record.newFileCount = added.value(mediaId).second;
        result.append(record);
    }
    return result;
}

QHash<qint64, qint64> MediaRepository::lastAddedByTitle() const
{
    QHash<qint64, qint64> added;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT fm.media_id, MAX(f.added) FROM file_media fm"
            " JOIN files f ON f.id = fm.file_id"
            " WHERE f.missing = 0"
            " GROUP BY fm.media_id"))) {
        MM_LOG_E() << "could not read when titles were added" << query.lastError().text();
        return added;
    }
    while (query.next()) {
        added.insert(query.value(0).toLongLong(), query.value(1).toLongLong());
    }
    return added;
}

QList<qint64> MediaRepository::fileIdsFor(qint64 mediaId) const
{
    QList<qint64> ids;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT file_id FROM file_media WHERE media_id = :media"));
    query.bindValue(QStringLiteral(":media"), mediaId);

    if (!query.exec()) {
        MM_LOG_E() << "could not list the files of title" << mediaId
                   << query.lastError().text();
        return ids;
    }

    while (query.next()) {
        ids.append(query.value(0).toLongLong());
    }
    return ids;
}

bool MediaRepository::listsBefore(const MediaRecord &a, const MediaRecord &b)
{
    const int order = TextFold::compare(a.title, b.title);
    return order != 0 ? order < 0 : a.id < b.id;
}

bool MediaRepository::detachFileFromOtherEpisodes(qint64 fileId, qint64 keepMediaId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE episodes SET file_id = NULL"
        " WHERE file_id = :file AND media_id <> :media"));
    query.bindValue(QStringLiteral(":file"), fileId);
    query.bindValue(QStringLiteral(":media"), keepMediaId);

    if (!query.exec()) {
        MM_LOG_E() << "could not detach file" << fileId
                   << "from the episodes of other titles"
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool MediaRepository::hasSuggestions(qint64 mediaId) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT 1 FROM file_media"
        " WHERE media_id = :id AND suggested = 1 LIMIT 1"));
    query.bindValue(QStringLiteral(":id"), mediaId);
    return query.exec() && query.next();
}

int MediaRepository::settleSuggestions(qint64 mediaId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE file_media SET suggested = 0"
        " WHERE media_id = :id AND suggested = 1"));
    query.bindValue(QStringLiteral(":id"), mediaId);

    if (!query.exec()) {
        MM_LOG_E() << "could not settle the suggestions under media" << mediaId
                   << query.lastError().text();
        return 0;
    }

    const int settled = query.numRowsAffected();
    if (settled > 0) {
        MM_LOG_I() << settled << "file(s) under media" << mediaId
                   << "are no longer a guess";
    }
    return settled;
}

QList<MediaRecord> MediaRepository::allOfKind(const QString &kind) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(kListedAggregateSelect + QStringLiteral(
        "WHERE m.kind = :kind"
        "  AND handles.first_handle IS NOT NULL"));
    query.bindValue(QStringLiteral(":kind"), kind);

    if (!query.exec()) {
        MM_LOG_E() << "could not list media of kind" << kind
                   << query.lastError().text();
        return QList<MediaRecord>();
    }

    return listedFromQuery(query);
}

QList<MediaRecord> MediaRepository::searchOfKind(const QString &kind,
                                                 const QString &query,
                                                 int limit) const
{
    const QString needle = TextFold::key(query.trimmed());
    if (needle.isEmpty() || limit <= 0) {
        return QList<MediaRecord>();
    }

    const bool byGenre = needle.size() >= 3;
    const QString genreClause = byGenre
        ? QStringLiteral("       OR instr(', ' || lower(COALESCE(m.genres, '')),"
                         "               :genre) > 0")
        : QString();

    QSqlQuery sql(m_database.handle());
    sql.prepare(kListedAggregateSelect + QStringLiteral(
        "WHERE m.kind = :kind"
        "  AND handles.first_handle IS NOT NULL"
        "  AND (instr(COALESCE(m.title_key, ''), :needle) > 0"
        "       OR EXISTS (SELECT 1 FROM file_media fm2"
        "                  JOIN files f2 ON f2.id = fm2.file_id"
        "                  WHERE fm2.media_id = m.id"
        "                    AND f2.missing = 0"
        "                    AND instr(COALESCE(f2.name_key, ''), :needle) > 0)")
        + genreClause + QStringLiteral(") "
        "ORDER BY CASE WHEN instr(COALESCE(m.title_key, ''), :needle) = 1"
        "              THEN 0 ELSE 1 END,"
        "         CASE WHEN instr(COALESCE(m.title_key, ''), :needle) > 0"
        "              THEN 0 ELSE 1 END,"
        "         COALESCE(m.title_key, ''), m.year "
        "LIMIT :limit"));
    sql.bindValue(QStringLiteral(":kind"), kind);
    sql.bindValue(QStringLiteral(":needle"), needle);
    if (byGenre) {
        sql.bindValue(QStringLiteral(":genre"), QStringLiteral(", ") + needle);
    }
    sql.bindValue(QStringLiteral(":limit"), limit);

    if (!sql.exec()) {
        MM_LOG_E() << "could not search titles of kind" << kind
                   << sql.lastError().text();
        return QList<MediaRecord>();
    }

    return listedFromQuery(sql, ListOrder::AsQueried);
}

QList<EpisodeRecord> MediaRepository::episodesFor(qint64 mediaId) const
{
    QList<EpisodeRecord> result;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT e.*, f.handle, f.display_name, f.missing, "
        "       p.position_seconds, p.duration_seconds, p.watched, p.last_played "
        "FROM episodes e "
        "LEFT JOIN files f ON f.id = e.file_id "
        "LEFT JOIN playback_state p ON p.file_id = e.file_id "
        "WHERE e.media_id = :media "
        "ORDER BY e.season, e.episode"));
    query.bindValue(QStringLiteral(":media"), mediaId);

    if (!query.exec()) {
        MM_LOG_E() << "could not list episodes for media" << mediaId
                   << query.lastError().text();
        return result;
    }

    const QSqlRecord columns = query.record();
    const int season = columns.indexOf(QStringLiteral("season"));
    const int episode = columns.indexOf(QStringLiteral("episode"));
    const int title = columns.indexOf(QStringLiteral("title"));
    const int overview = columns.indexOf(QStringLiteral("overview"));
    const int stillPath = columns.indexOf(QStringLiteral("still_path"));
    const int airDate = columns.indexOf(QStringLiteral("air_date"));
    const int runtimeMinutes = columns.indexOf(QStringLiteral("runtime_minutes"));
    const int fileIdColumn = columns.indexOf(QStringLiteral("file_id"));
    const int handle = columns.indexOf(QStringLiteral("handle"));
    const int displayName = columns.indexOf(QStringLiteral("display_name"));
    const int missing = columns.indexOf(QStringLiteral("missing"));
    const int positionSeconds = columns.indexOf(QStringLiteral("position_seconds"));
    const int durationSeconds = columns.indexOf(QStringLiteral("duration_seconds"));
    const int watched = columns.indexOf(QStringLiteral("watched"));
    const int lastPlayed = columns.indexOf(QStringLiteral("last_played"));

    while (query.next()) {
        EpisodeRecord record;
        record.mediaId = mediaId;
        record.season = query.value(season).toInt();
        record.episode = query.value(episode).toInt();
        record.title = query.value(title).toString();
        record.overview = query.value(overview).toString();
        record.stillPath = query.value(stillPath).toString();
        record.airDate = query.value(airDate).toString();
        record.runtimeMinutes = query.value(runtimeMinutes).toInt();

        const QVariant fileId = query.value(fileIdColumn);
        record.fileId = fileId.isNull() ? -1 : fileId.toLongLong();
        record.fileHandle = query.value(handle).toString();
        record.fileName = query.value(displayName).toString();
        record.missing = query.value(missing).toInt() != 0;
        record.positionSeconds = query.value(positionSeconds).toDouble();
        record.durationSeconds = query.value(durationSeconds).toDouble();
        record.lastPlayed = query.value(lastPlayed).toLongLong();
        record.watched = query.value(watched).toInt() != 0;

        result.append(record);
    }

    return result;
}

bool MediaRepository::linkFile(qint64 fileId,
                               qint64 mediaId,
                               double confidence,
                               bool suggested)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO file_media (file_id, media_id, confidence, suggested, matched_at) "
        "VALUES (:file, :media, :confidence, :suggested, :matched) "
        "ON CONFLICT(file_id) DO UPDATE SET "
        "  media_id = excluded.media_id,"
        "  confidence = excluded.confidence,"
        "  suggested = excluded.suggested,"
        "  matched_at = excluded.matched_at"));

    query.bindValue(QStringLiteral(":file"), fileId);
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":confidence"), confidence);
    query.bindValue(QStringLiteral(":suggested"), suggested ? 1 : 0);
    query.bindValue(QStringLiteral(":matched"), QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not link file" << fileId << "to media" << mediaId
                   << query.lastError().text();
        return false;
    }
    return true;
}

FileMediaLink MediaRepository::linkForFile(qint64 fileId) const
{
    FileMediaLink link;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT file_id, media_id, confidence, suggested FROM file_media "
        "WHERE file_id = :file"));
    query.bindValue(QStringLiteral(":file"), fileId);

    if (query.exec() && query.next()) {
        link.fileId = query.value(QStringLiteral("file_id")).toLongLong();
        link.mediaId = query.value(QStringLiteral("media_id")).toLongLong();
        link.confidence = query.value(QStringLiteral("confidence")).toDouble();
        link.suggested = query.value(QStringLiteral("suggested")).toInt() != 0;
    }
    return link;
}

QList<LinkedFile> MediaRepository::unpinnedLinks() const
{
    QList<LinkedFile> result;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT fm.file_id, fm.media_id, fm.confidence, fm.suggested, f.handle"
            " FROM file_media fm"
            " JOIN files f ON f.id = fm.file_id"
            " LEFT JOIN match_overrides mo ON mo.file_handle = f.handle"
            " WHERE f.missing = 0 AND mo.file_handle IS NULL"
            " ORDER BY fm.file_id"))) {
        MM_LOG_E() << "could not list the automatic matches"
                   << query.lastError().text();
        return result;
    }

    while (query.next()) {
        LinkedFile entry;
        entry.link.fileId = query.value(0).toLongLong();
        entry.link.mediaId = query.value(1).toLongLong();
        entry.link.confidence = query.value(2).toDouble();
        entry.link.suggested = query.value(3).toInt() != 0;
        entry.fileHandle = query.value(4).toString();
        result.append(entry);
    }
    return result;
}

bool MediaRepository::detachUnlinkedEpisodes()
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "UPDATE episodes SET file_id = NULL"
            " WHERE file_id IS NOT NULL"
            "   AND file_id NOT IN (SELECT file_id FROM file_media)"))) {
        MM_LOG_E() << "could not release episodes whose file has no title"
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool MediaRepository::unlinkFile(qint64 fileId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("DELETE FROM file_media WHERE file_id = :file"));
    query.bindValue(QStringLiteral(":file"), fileId);

    if (!query.exec()) {
        MM_LOG_E() << "could not unlink file" << fileId << query.lastError().text();
        return false;
    }

    QSqlQuery detach(m_database.handle());
    detach.prepare(QStringLiteral(
        "UPDATE episodes SET file_id = NULL WHERE file_id = :file"));
    detach.bindValue(QStringLiteral(":file"), fileId);

    if (!detach.exec()) {
        MM_LOG_E() << "could not detach file" << fileId << "from its episode"
                   << detach.lastError().text();
        return false;
    }
    return true;
}

bool MediaRepository::upsertEpisode(const EpisodeRecord &record, qint64 fileId)
{
    QSqlQuery query(m_database.handle());

    query.prepare(QStringLiteral(
        "INSERT INTO episodes (media_id, file_id, season, episode, title, title_key,"
        "                      overview, still_path, air_date, runtime_minutes) "
        "VALUES (:media, :file, :season, :episode, :title, :title_key,"
        "        :overview, :still, :air, :runtime) "
        "ON CONFLICT(media_id, season, episode) DO UPDATE SET "
        "  file_id = COALESCE(excluded.file_id, episodes.file_id),"
        "  title = COALESCE(NULLIF(episodes.title, ''), NULLIF(excluded.title, '')),"
        "  title_key = CASE WHEN COALESCE(episodes.title, '') <> ''"
        "                   THEN episodes.title_key ELSE excluded.title_key END,"
        "  overview = COALESCE(NULLIF(episodes.overview, ''), NULLIF(excluded.overview, '')),"
        "  still_path = COALESCE(NULLIF(episodes.still_path, ''), NULLIF(excluded.still_path, '')),"
        "  air_date = COALESCE(NULLIF(episodes.air_date, ''), NULLIF(excluded.air_date, '')),"
        "  runtime_minutes = MAX(excluded.runtime_minutes,"
        "                        COALESCE(episodes.runtime_minutes, 0))"));

    query.bindValue(QStringLiteral(":media"), record.mediaId);
    query.bindValue(QStringLiteral(":file"), fileId > 0 ? QVariant(fileId) : QVariant());
    query.bindValue(QStringLiteral(":season"), record.season);
    query.bindValue(QStringLiteral(":episode"), record.episode);
    query.bindValue(QStringLiteral(":title"), record.title);
    query.bindValue(QStringLiteral(":title_key"), TextFold::key(record.title));
    query.bindValue(QStringLiteral(":overview"), record.overview);
    query.bindValue(QStringLiteral(":still"), record.stillPath);
    query.bindValue(QStringLiteral(":air"), record.airDate);
    query.bindValue(QStringLiteral(":runtime"), record.runtimeMinutes);

    if (!query.exec()) {
        MM_LOG_E() << "could not store episode" << record.season << record.episode
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool MediaRepository::overwriteEpisodeFromTmdb(const EpisodeRecord &record)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO episodes (media_id, file_id, season, episode, title, title_key,"
        "                      overview, still_path, air_date, runtime_minutes) "
        "VALUES (:media, NULL, :season, :episode, :title, :title_key,"
        "        :overview, :still, :air, :runtime) "
        "ON CONFLICT(media_id, season, episode) DO UPDATE SET "
        "  title = excluded.title,"
        "  title_key = excluded.title_key,"
        "  overview = excluded.overview,"
        "  still_path = excluded.still_path,"
        "  air_date = excluded.air_date,"
        "  runtime_minutes = excluded.runtime_minutes"));

    query.bindValue(QStringLiteral(":media"), record.mediaId);
    query.bindValue(QStringLiteral(":season"), record.season);
    query.bindValue(QStringLiteral(":episode"), record.episode);
    query.bindValue(QStringLiteral(":title"), record.title);
    query.bindValue(QStringLiteral(":title_key"), TextFold::key(record.title));
    query.bindValue(QStringLiteral(":overview"), record.overview);
    query.bindValue(QStringLiteral(":still"), record.stillPath);
    query.bindValue(QStringLiteral(":air"), record.airDate);
    query.bindValue(QStringLiteral(":runtime"), record.runtimeMinutes);

    if (!query.exec()) {
        MM_LOG_E() << "could not store episode from tmdb"
                   << record.season << record.episode
                   << query.lastError().text();
        return false;
    }
    return true;
}

EpisodeRecord MediaRepository::episodeForFile(qint64 fileId) const
{
    EpisodeRecord record;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT media_id, file_id, season, episode, title, overview, still_path,"
        "       air_date, runtime_minutes "
        "FROM episodes WHERE file_id = :file"));
    query.bindValue(QStringLiteral(":file"), fileId);

    if (query.exec() && query.next()) {
        record.mediaId = query.value(QStringLiteral("media_id")).toLongLong();
        const QVariant stored = query.value(QStringLiteral("file_id"));
        record.fileId = stored.isNull() ? -1 : stored.toLongLong();
        record.season = query.value(QStringLiteral("season")).toInt();
        record.episode = query.value(QStringLiteral("episode")).toInt();
        record.title = query.value(QStringLiteral("title")).toString();
        record.overview = query.value(QStringLiteral("overview")).toString();
        record.stillPath = query.value(QStringLiteral("still_path")).toString();
        record.airDate = query.value(QStringLiteral("air_date")).toString();
        record.runtimeMinutes =
            query.value(QStringLiteral("runtime_minutes")).toInt();
    }
    return record;
}

EpisodeRecord MediaRepository::nextEpisodeWithFile(qint64 mediaId,
                                                   int season,
                                                   int episode) const
{
    EpisodeRecord record;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT e.*, f.handle, f.display_name, f.missing, "
        "       p.position_seconds, p.duration_seconds, p.watched "
        "FROM episodes e "
        "JOIN files f ON f.id = e.file_id "
        "LEFT JOIN playback_state p ON p.file_id = e.file_id "
        "WHERE e.media_id = :media "
        "  AND f.missing = 0 "
        "  AND (e.season > :season"
        "       OR (e.season = :season AND e.episode > :episode)) "
        "ORDER BY e.season, e.episode "
        "LIMIT 1"));
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":season"), season);
    query.bindValue(QStringLiteral(":episode"), episode);

    if (!query.exec()) {
        MM_LOG_E() << "could not find the next episode after" << season
                   << episode << "of media" << mediaId
                   << query.lastError().text();
        return record;
    }

    if (!query.next()) {
        return record;
    }

    record.mediaId = mediaId;
    record.season = query.value(QStringLiteral("season")).toInt();
    record.episode = query.value(QStringLiteral("episode")).toInt();
    record.title = query.value(QStringLiteral("title")).toString();
    record.overview = query.value(QStringLiteral("overview")).toString();
    record.stillPath = query.value(QStringLiteral("still_path")).toString();
    record.airDate = query.value(QStringLiteral("air_date")).toString();
    record.runtimeMinutes =
        query.value(QStringLiteral("runtime_minutes")).toInt();
    record.fileId = query.value(QStringLiteral("file_id")).toLongLong();
    record.fileHandle = query.value(QStringLiteral("handle")).toString();
    record.fileName = query.value(QStringLiteral("display_name")).toString();
    record.missing = false;
    record.positionSeconds =
        query.value(QStringLiteral("position_seconds")).toDouble();
    record.durationSeconds =
        query.value(QStringLiteral("duration_seconds")).toDouble();
    record.watched = query.value(QStringLiteral("watched")).toInt() != 0;

    return record;
}

bool MediaRepository::replaceCredits(qint64 mediaId, qint64 episodeId,
                                     const QList<CreditRecord> &credits)
{
    if (mediaId <= 0) {
        return false;
    }

    QSqlQuery clear(m_database.handle());
    clear.prepare(QStringLiteral(
        "DELETE FROM credits WHERE media_id = :media AND episode_id = :episode"));
    clear.bindValue(QStringLiteral(":media"), mediaId);
    clear.bindValue(QStringLiteral(":episode"), episodeId);
    if (!clear.exec()) {
        MM_LOG_E() << "could not clear the credits of media" << mediaId
                   << clear.lastError().text();
        return false;
    }

    if (credits.isEmpty()) {
        return true;
    }

    QSqlQuery insert(m_database.handle());
    insert.prepare(QStringLiteral(
        "INSERT INTO credits (media_id, episode_id, kind, name, role, "
        "profile_path, sort) "
        "VALUES (:media, :episode, :kind, :name, :role, :profile, :sort)"));

    int sort = 0;
    for (const CreditRecord &credit : credits) {
        insert.bindValue(QStringLiteral(":media"), mediaId);
        insert.bindValue(QStringLiteral(":episode"), episodeId);
        insert.bindValue(QStringLiteral(":kind"), credit.kind);
        insert.bindValue(QStringLiteral(":name"), credit.name);
        insert.bindValue(QStringLiteral(":role"), credit.role);
        insert.bindValue(QStringLiteral(":profile"), credit.profilePath);
        insert.bindValue(QStringLiteral(":sort"), sort++);

        if (!insert.exec()) {
            MM_LOG_E() << "could not store" << credit.name << "on media"
                       << mediaId << insert.lastError().text();
            return false;
        }
    }

    MM_LOG_D() << "stored" << credits.size() << "names on media" << mediaId
               << (episodeId > 0 ? "episode " + QString::number(episodeId)
                                 : QStringLiteral("itself"));
    return true;
}

QList<CreditRecord> MediaRepository::creditsFor(qint64 mediaId,
                                                qint64 episodeId) const
{
    QList<CreditRecord> credits;
    if (mediaId <= 0) {
        return credits;
    }

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT kind, name, role, profile_path FROM credits "
        "WHERE media_id = :media AND episode_id = :episode "
        "ORDER BY kind, sort"));
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":episode"), episodeId);

    if (!query.exec()) {
        MM_LOG_W() << "could not read the credits of media" << mediaId
                   << query.lastError().text();
        return credits;
    }

    while (query.next()) {
        CreditRecord credit;
        credit.kind = query.value(0).toInt();
        credit.name = query.value(1).toString();
        credit.role = query.value(2).toString();
        credit.profilePath = query.value(3).toString();
        credits.append(credit);
    }
    return credits;
}

bool MediaRepository::markCreditsFetched(qint64 mediaId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO fetched_credits (media_id, fetched_at) VALUES (:media, :now) "
        "ON CONFLICT(media_id) DO UPDATE SET fetched_at = excluded.fetched_at"));
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":now"),
                    QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_W() << "could not record the credits of media" << mediaId
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool MediaRepository::markSeasonCreditsFetched(qint64 mediaId, int season)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE fetched_seasons SET credits_fetched = 1 "
        "WHERE media_id = :media AND season = :season"));
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":season"), season);

    if (!query.exec()) {
        MM_LOG_W() << "could not record the credits of season" << season
                   << "of media" << mediaId << query.lastError().text();
        return false;
    }
    return true;
}

QList<qint64> MediaRepository::titlesWithoutCredits(int limit) const
{
    QList<qint64> titles;
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT m.id FROM media m "
        "LEFT JOIN fetched_credits c ON c.media_id = m.id "
        "WHERE c.media_id IS NULL AND m.tmdb_id > 0 "
        "ORDER BY m.id LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), limit);

    if (!query.exec()) {
        MM_LOG_W() << "could not list the titles with nobody credited"
                   << query.lastError().text();
        return titles;
    }

    while (query.next()) {
        titles.append(query.value(0).toLongLong());
    }
    return titles;
}

QList<QPair<qint64, int>> MediaRepository::seasonsWithoutCredits(int limit) const
{
    QList<QPair<qint64, int>> seasons;
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT media_id, season FROM fetched_seasons "
        "WHERE credits_fetched = 0 ORDER BY media_id, season LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), limit);

    if (!query.exec()) {
        MM_LOG_W() << "could not list the seasons with nobody credited"
                   << query.lastError().text();
        return seasons;
    }

    while (query.next()) {
        seasons.append({ query.value(0).toLongLong(), query.value(1).toInt() });
    }
    return seasons;
}

int MediaRepository::titlesWithoutCreditsCount() const
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM media m "
            "LEFT JOIN fetched_credits c ON c.media_id = m.id "
            "WHERE c.media_id IS NULL AND m.tmdb_id > 0"))
        || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int MediaRepository::seasonsWithoutCreditsCount() const
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM fetched_seasons WHERE credits_fetched = 0"))
        || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

qint64 MediaRepository::episodeRowId(qint64 mediaId, int season,
                                     int episode) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT id FROM episodes "
        "WHERE media_id = :media AND season = :season AND episode = :episode"));
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":season"), season);
    query.bindValue(QStringLiteral(":episode"), episode);

    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toLongLong();
}

bool MediaRepository::markSeasonFetched(qint64 mediaId, int season,
                                        const QString &posterPath)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO fetched_seasons (media_id, season, fetched_at, poster_path) "
        "VALUES (:media, :season, :now, :poster) "
        "ON CONFLICT(media_id, season) DO UPDATE SET fetched_at = excluded.fetched_at, "
        "poster_path = excluded.poster_path"));
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":season"), season);
    query.bindValue(QStringLiteral(":poster"),
                    posterPath.isNull() ? QStringLiteral("") : posterPath);
    query.bindValue(QStringLiteral(":now"),
                    QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not record season" << season << "of media"
                   << mediaId << query.lastError().text();
        return false;
    }
    return true;
}

bool MediaRepository::seasonFetched(qint64 mediaId, int season) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT 1 FROM fetched_seasons "
        "WHERE media_id = :media AND season = :season AND poster_path IS NOT NULL"));
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":season"), season);

    return query.exec() && query.next();
}

QStringList MediaRepository::seasonBackdropPaths(const QString &mediaFilter) const
{
    QStringList paths;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT DISTINCT e.still_path FROM episodes e"
            " JOIN (SELECT media_id, season, MIN(episode) AS first FROM episodes"
            "       WHERE still_path IS NOT NULL AND still_path <> ''"
            "       GROUP BY media_id, season) f"
            "   ON f.media_id = e.media_id AND f.season = e.season AND f.first = e.episode"
            " WHERE e.still_path IS NOT NULL AND e.still_path <> ''"
            "   AND EXISTS (SELECT 1 FROM episodes w WHERE w.media_id = e.media_id"
            "               AND w.season = e.season AND w.file_id IS NOT NULL)")
            + mediaFilter)) {
        MM_LOG_W() << "could not list the season backdrops" << query.lastError().text();
        return paths;
    }
    while (query.next()) {
        paths.append(query.value(0).toString());
    }
    return paths;
}

QHash<int, QString> MediaRepository::seasonPosters(qint64 mediaId) const
{
    QHash<int, QString> posters;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT season, poster_path FROM fetched_seasons "
        "WHERE media_id = :media AND poster_path IS NOT NULL AND poster_path <> ''"));
    query.bindValue(QStringLiteral(":media"), mediaId);

    if (!query.exec()) {
        MM_LOG_W() << "could not read the season posters of media" << mediaId
                   << query.lastError().text();
        return posters;
    }
    while (query.next()) {
        posters.insert(query.value(0).toInt(), query.value(1).toString());
    }
    return posters;
}

bool MediaRepository::clearFetchedSeasons()
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral("DELETE FROM fetched_seasons"))) {
        MM_LOG_E() << "could not clear the fetched season records"
                   << query.lastError().text();
        return false;
    }
    return true;
}

ArtworkPaths MediaRepository::artworkPathsFor(const QList<qint64> &mediaIds) const
{
    ArtworkPaths paths;
    if (mediaIds.isEmpty()) {
        return paths;
    }

    QStringList numbers;
    numbers.reserve(mediaIds.size());
    for (const qint64 id : mediaIds) {
        numbers.append(QString::number(id));
    }
    const QString ids = numbers.join(QLatin1Char(','));

    QSqlQuery media(m_database.handle());
    if (media.exec(QStringLiteral(
            "SELECT poster_path, backdrop_path FROM media m"
            " WHERE m.id IN (") + ids + QStringLiteral(")"
            "   AND EXISTS (SELECT 1 FROM file_media fm WHERE fm.media_id = m.id)"))) {
        while (media.next()) {
            const QString poster = media.value(0).toString();
            const QString backdrop = media.value(1).toString();
            if (!poster.isEmpty()) {
                paths.posters.append(poster);
            }
            if (!backdrop.isEmpty()) {
                paths.backdrops.append(backdrop);
            }
        }
    } else {
        MM_LOG_W() << "could not list artwork paths for" << mediaIds.size()
                   << "titles" << media.lastError().text();
    }

    QSqlQuery stills(m_database.handle());
    if (stills.exec(QStringLiteral(
            "SELECT still_path FROM episodes"
            " WHERE media_id IN (") + ids + QStringLiteral(")"
            "   AND file_id IS NOT NULL AND still_path IS NOT NULL"
            "   AND still_path <> ''"))) {
        while (stills.next()) {
            paths.stills.append(stills.value(0).toString());
        }
    }

    QSqlQuery seasons(m_database.handle());
    if (seasons.exec(QStringLiteral(
            "SELECT DISTINCT fs.poster_path FROM fetched_seasons fs"
            " WHERE fs.media_id IN (") + ids + QStringLiteral(")"
            "   AND fs.poster_path IS NOT NULL AND fs.poster_path <> ''"
            "   AND EXISTS (SELECT 1 FROM episodes e WHERE e.media_id = fs.media_id"
            "               AND e.season = fs.season AND e.file_id IS NOT NULL)"))) {
        while (seasons.next()) {
            paths.seasonPosters.append(seasons.value(0).toString());
        }
    }

    QSqlQuery faces(m_database.handle());
    if (faces.exec(QStringLiteral(
            "SELECT DISTINCT c.profile_path FROM credits c"
            " WHERE c.media_id IN (") + ids + QStringLiteral(")"
            "   AND c.profile_path IS NOT NULL AND c.profile_path <> ''"))) {
        while (faces.next()) {
            paths.profiles.append(faces.value(0).toString());
        }
    }

    paths.seasonBackdrops = seasonBackdropPaths(
        QStringLiteral(" AND e.media_id IN (") + ids + QStringLiteral(")"));

    return paths;
}

ArtworkPaths MediaRepository::artworkPaths() const
{
    ArtworkPaths paths;

    QSqlQuery media(m_database.handle());
    if (media.exec(QStringLiteral(
            "SELECT poster_path, backdrop_path FROM media m "
            "WHERE EXISTS (SELECT 1 FROM file_media fm WHERE fm.media_id = m.id)"))) {
        while (media.next()) {
            const QString poster = media.value(0).toString();
            const QString backdrop = media.value(1).toString();
            if (!poster.isEmpty()) {
                paths.posters.append(poster);
            }
            if (!backdrop.isEmpty()) {
                paths.backdrops.append(backdrop);
            }
        }
    } else {
        MM_LOG_W() << "could not list artwork paths" << media.lastError().text();
    }

    QSqlQuery stills(m_database.handle());
    if (stills.exec(QStringLiteral(
            "SELECT still_path FROM episodes "
            "WHERE file_id IS NOT NULL AND still_path IS NOT NULL "
            "  AND still_path <> ''"))) {
        while (stills.next()) {
            paths.stills.append(stills.value(0).toString());
        }
    }

    QSqlQuery seasons(m_database.handle());
    if (seasons.exec(QStringLiteral(
            "SELECT DISTINCT fs.poster_path FROM fetched_seasons fs "
            "WHERE fs.poster_path IS NOT NULL AND fs.poster_path <> '' "
            "  AND EXISTS (SELECT 1 FROM episodes e WHERE e.media_id = fs.media_id "
            "              AND e.season = fs.season AND e.file_id IS NOT NULL)"))) {
        while (seasons.next()) {
            paths.seasonPosters.append(seasons.value(0).toString());
        }
    }

    QSqlQuery faces(m_database.handle());
    if (faces.exec(QStringLiteral(
            "SELECT DISTINCT c.profile_path FROM credits c "
            "WHERE c.profile_path IS NOT NULL AND c.profile_path <> '' "
            "  AND EXISTS (SELECT 1 FROM file_media fm WHERE fm.media_id = c.media_id)"))) {
        while (faces.next()) {
            paths.profiles.append(faces.value(0).toString());
        }
    }

    paths.seasonBackdrops = seasonBackdropPaths(QString());

    QSqlQuery collections(m_database.handle());
    if (collections.exec(QStringLiteral(
            "SELECT c.poster_path FROM collections c"
            " WHERE c.poster_path IS NOT NULL AND c.poster_path <> ''"
            "   AND EXISTS (SELECT 1 FROM media m JOIN file_media fm ON fm.media_id = m.id"
            "               WHERE m.collection_id = c.tmdb_id)"
            " UNION "
            "SELECT p.poster_path FROM collection_parts p"
            " WHERE p.poster_path IS NOT NULL AND p.poster_path <> ''"
            "   AND EXISTS (SELECT 1 FROM media m JOIN file_media fm ON fm.media_id = m.id"
            "               WHERE m.collection_id = p.collection_id)"
            " UNION "
            "SELECT d.poster_path FROM film_details d"
            " WHERE d.poster_path IS NOT NULL AND d.poster_path <> ''"))) {
        while (collections.next()) {
            paths.collectionPosters.append(collections.value(0).toString());
        }
    } else {
        MM_LOG_W() << "could not list the collection posters" << collections.lastError().text();
    }

    return paths;
}

bool MediaRepository::setMediaCollection(qint64 mediaId, qint64 collectionId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE media SET collection_id = :collection, collection_checked = :now "
        "WHERE id = :media"));
    query.bindValue(QStringLiteral(":collection"),
                    collectionId > 0 ? QVariant(collectionId) : QVariant(QMetaType::fromType<qint64>()));
    query.bindValue(QStringLiteral(":now"), QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    query.bindValue(QStringLiteral(":media"), mediaId);

    if (!query.exec()) {
        MM_LOG_E() << "could not record the collection of media" << mediaId
                   << query.lastError().text();
        return false;
    }
    return true;
}

qint64 MediaRepository::collectionIdFor(qint64 mediaId) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT collection_id FROM media WHERE id = :media"));
    query.bindValue(QStringLiteral(":media"), mediaId);

    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toLongLong();
}

QList<qint64> MediaRepository::filmsWithoutCollectionCheck() const
{
    QList<qint64> tmdbIds;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT m.tmdb_id FROM media m"
            " WHERE m.kind = 'movie' AND m.collection_checked IS NULL"
            "   AND EXISTS (SELECT 1 FROM file_media fm"
            "               WHERE fm.media_id = m.id AND fm.suggested = 0)"
            " ORDER BY m.id"))) {
        MM_LOG_W() << "could not list the films without a collection check"
                   << query.lastError().text();
        return tmdbIds;
    }
    while (query.next()) {
        tmdbIds.append(query.value(0).toLongLong());
    }
    return tmdbIds;
}

bool MediaRepository::noteCollection(const CollectionRecord &reference)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO collections (tmdb_id, name, poster_path, backdrop_path) "
        "VALUES (:id, :name, :poster, :backdrop) "
        "ON CONFLICT(tmdb_id) DO NOTHING"));
    query.bindValue(QStringLiteral(":id"), reference.tmdbId);
    query.bindValue(QStringLiteral(":name"), reference.name);
    query.bindValue(QStringLiteral(":poster"), reference.posterPath);
    query.bindValue(QStringLiteral(":backdrop"), reference.backdropPath);

    if (!query.exec()) {
        MM_LOG_E() << "could not note collection" << reference.tmdbId
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool MediaRepository::saveCollection(const CollectionRecord &collection)
{
    QSqlQuery row(m_database.handle());
    row.prepare(QStringLiteral(
        "INSERT INTO collections (tmdb_id, name, overview, poster_path, backdrop_path, fetched_at) "
        "VALUES (:id, :name, :overview, :poster, :backdrop, :now) "
        "ON CONFLICT(tmdb_id) DO UPDATE SET name = excluded.name, "
        "overview = excluded.overview, poster_path = excluded.poster_path, "
        "backdrop_path = excluded.backdrop_path, fetched_at = excluded.fetched_at"));
    row.bindValue(QStringLiteral(":id"), collection.tmdbId);
    row.bindValue(QStringLiteral(":name"), collection.name);
    row.bindValue(QStringLiteral(":overview"), collection.overview);
    row.bindValue(QStringLiteral(":poster"), collection.posterPath);
    row.bindValue(QStringLiteral(":backdrop"), collection.backdropPath);
    row.bindValue(QStringLiteral(":now"), collection.fetchedAt > 0
                      ? collection.fetchedAt
                      : QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!row.exec()) {
        MM_LOG_E() << "could not save collection" << collection.tmdbId
                   << row.lastError().text();
        return false;
    }

    QSqlQuery clear(m_database.handle());
    clear.prepare(QStringLiteral("DELETE FROM collection_parts WHERE collection_id = :id"));
    clear.bindValue(QStringLiteral(":id"), collection.tmdbId);
    if (!clear.exec()) {
        MM_LOG_E() << "could not replace the films of collection" << collection.tmdbId
                   << clear.lastError().text();
        return false;
    }

    QSqlQuery part(m_database.handle());
    part.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO collection_parts "
        "(collection_id, tmdb_id, position, title, release_date, poster_path, backdrop_path) "
        "VALUES (:collection, :id, :position, :title, :released, :poster, :backdrop)"));
    for (const CollectionPartRecord &item : collection.parts) {
        part.bindValue(QStringLiteral(":collection"), collection.tmdbId);
        part.bindValue(QStringLiteral(":id"), item.tmdbId);
        part.bindValue(QStringLiteral(":position"), item.position);
        part.bindValue(QStringLiteral(":title"), item.title);
        part.bindValue(QStringLiteral(":released"), item.releaseDate);
        part.bindValue(QStringLiteral(":poster"), item.posterPath);
        part.bindValue(QStringLiteral(":backdrop"), item.backdropPath);
        if (!part.exec()) {
            MM_LOG_E() << "could not save film" << item.tmdbId << "of collection"
                       << collection.tmdbId << part.lastError().text();
            return false;
        }
    }
    return true;
}

CollectionRecord MediaRepository::collectionById(qint64 collectionId) const
{
    CollectionRecord collection;

    QSqlQuery row(m_database.handle());
    row.prepare(QStringLiteral(
        "SELECT tmdb_id, name, overview, poster_path, backdrop_path, fetched_at "
        "FROM collections WHERE tmdb_id = :id"));
    row.bindValue(QStringLiteral(":id"), collectionId);
    if (!row.exec() || !row.next()) {
        return collection;
    }

    collection.tmdbId = row.value(0).toLongLong();
    collection.name = row.value(1).toString();
    collection.overview = row.value(2).toString();
    collection.posterPath = row.value(3).toString();
    collection.backdropPath = row.value(4).toString();
    collection.fetchedAt = row.value(5).toLongLong();

    QSqlQuery parts(m_database.handle());
    parts.prepare(QStringLiteral(
        "SELECT tmdb_id, position, title, release_date, poster_path, backdrop_path "
        "FROM collection_parts WHERE collection_id = :id ORDER BY position"));
    parts.bindValue(QStringLiteral(":id"), collectionId);
    if (!parts.exec()) {
        MM_LOG_W() << "could not read the films of collection" << collectionId
                   << parts.lastError().text();
        return collection;
    }
    while (parts.next()) {
        CollectionPartRecord part;
        part.tmdbId = parts.value(0).toLongLong();
        part.position = parts.value(1).toInt();
        part.title = parts.value(2).toString();
        part.releaseDate = parts.value(3).toString();
        part.posterPath = parts.value(4).toString();
        part.backdropPath = parts.value(5).toString();
        collection.parts.append(part);
    }
    return collection;
}

QList<qint64> MediaRepository::collectionsToFetch(qint64 fetchedBefore) const
{
    QList<qint64> ids;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT DISTINCT m.collection_id FROM media m"
        " LEFT JOIN collections c ON c.tmdb_id = m.collection_id"
        " WHERE m.collection_id IS NOT NULL"
        "   AND (c.fetched_at IS NULL OR c.fetched_at < :before)"
        "   AND EXISTS (SELECT 1 FROM file_media fm WHERE fm.media_id = m.id)"
        " ORDER BY m.collection_id"));
    query.bindValue(QStringLiteral(":before"), fetchedBefore);

    if (!query.exec()) {
        MM_LOG_W() << "could not list the collections to fetch" << query.lastError().text();
        return ids;
    }
    while (query.next()) {
        ids.append(query.value(0).toLongLong());
    }
    return ids;
}

QHash<qint64, qint64> MediaRepository::collectionsByMedia() const
{
    QHash<qint64, qint64> result;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT m.id, m.collection_id FROM media m"
            " WHERE m.kind = 'movie' AND m.collection_id IS NOT NULL"
            "   AND EXISTS (SELECT 1 FROM file_media fm WHERE fm.media_id = m.id)"))) {
        MM_LOG_W() << "could not read which films belong to a collection"
                   << query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.insert(query.value(0).toLongLong(), query.value(1).toLongLong());
    }
    return result;
}

QHash<qint64, qint64> MediaRepository::collectionsByOwnedFilm() const
{
    QHash<qint64, qint64> result;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT m.tmdb_id, COALESCE(m.collection_id, 0) FROM media m"
            " WHERE m.kind = 'movie'"
            "   AND EXISTS (SELECT 1 FROM file_media fm WHERE fm.media_id = m.id)"))) {
        MM_LOG_W() << "could not list the owned films and their collections"
                   << query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.insert(query.value(0).toLongLong(), query.value(1).toLongLong());
    }
    return result;
}

qint64 MediaRepository::createCustomCollection(const CustomCollectionRecord &collection)
{
    if (collection.name.trimmed().isEmpty() || collection.mediaIds.isEmpty()) {
        MM_LOG_W() << "a collection needs a name and at least one title";
        return 0;
    }

    QSqlQuery row(m_database.handle());
    row.prepare(QStringLiteral(
        "INSERT INTO custom_collections (name, description, cover_mode, created_at) "
        "VALUES (:name, :description, :cover, :now)"));
    row.bindValue(QStringLiteral(":name"), collection.name.trimmed());
    row.bindValue(QStringLiteral(":description"), collection.description);
    row.bindValue(QStringLiteral(":cover"), collection.coverMode);
    row.bindValue(QStringLiteral(":now"), QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!row.exec()) {
        MM_LOG_E() << "could not create the collection" << collection.name
                   << row.lastError().text();
        return 0;
    }

    const qint64 id = row.lastInsertId().toLongLong();

    QSqlQuery item(m_database.handle());
    item.prepare(QStringLiteral(
        "INSERT INTO custom_collection_items (collection_id, media_id, position) "
        "VALUES (:collection, :media, :position)"));
    int position = 0;
    for (const qint64 mediaId : collection.mediaIds) {
        item.bindValue(QStringLiteral(":collection"), id);
        item.bindValue(QStringLiteral(":media"), mediaId);
        item.bindValue(QStringLiteral(":position"), position++);
        if (!item.exec()) {
            MM_LOG_E() << "could not add title" << mediaId << "to collection" << id
                       << item.lastError().text();
            return 0;
        }
    }

    MM_LOG_I() << "collection" << id << collection.name << "created with"
               << collection.mediaIds.size() << "titles";
    return id;
}

bool MediaRepository::hideCollection(qint64 collectionId, const QString &name)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO hidden_collections (collection_id, name, hidden_at) "
        "VALUES (:id, :name, :now) "
        "ON CONFLICT(collection_id) DO UPDATE SET name = excluded.name, "
        "hidden_at = excluded.hidden_at"));
    query.bindValue(QStringLiteral(":id"), collectionId);
    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":now"), QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not take collection" << collectionId << "off the page"
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "collection" << collectionId << name << "taken off the page";
    return true;
}

bool MediaRepository::restoreCollection(qint64 collectionId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "DELETE FROM hidden_collections WHERE collection_id = :id"));
    query.bindValue(QStringLiteral(":id"), collectionId);

    if (!query.exec()) {
        MM_LOG_E() << "could not put collection" << collectionId << "back"
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "collection" << collectionId << "is back on the page";
    return true;
}

QList<HiddenCollectionRecord> MediaRepository::hiddenCollections() const
{
    QList<HiddenCollectionRecord> result;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT collection_id, name, hidden_at FROM hidden_collections"
            " ORDER BY hidden_at DESC"))) {
        MM_LOG_W() << "could not list the collections taken off the page"
                   << query.lastError().text();
        return result;
    }
    while (query.next()) {
        HiddenCollectionRecord hidden;
        hidden.collectionId = query.value(0).toLongLong();
        hidden.name = query.value(1).toString();
        hidden.hiddenAt = query.value(2).toLongLong();
        result.append(hidden);
    }
    return result;
}

bool MediaRepository::hideCollectionFilm(const QString &scope,
                                        qint64 tmdbId,
                                        const QString &title,
                                        const QString &collectionName)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO hidden_collection_films"
        " (scope, tmdb_id, title, collection_name, hidden_at) "
        "VALUES (:scope, :tmdb, :title, :collection, :now) "
        "ON CONFLICT(scope, tmdb_id) DO UPDATE SET title = excluded.title, "
        "collection_name = excluded.collection_name, hidden_at = excluded.hidden_at"));
    query.bindValue(QStringLiteral(":scope"), scope);
    query.bindValue(QStringLiteral(":tmdb"), tmdbId);
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":collection"), collectionName);
    query.bindValue(QStringLiteral(":now"), QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not take film" << tmdbId << "out of" << scope
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "film" << tmdbId << title << "taken out of" << scope;
    return true;
}

bool MediaRepository::restoreCollectionFilm(const QString &scope, qint64 tmdbId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "DELETE FROM hidden_collection_films WHERE scope = :scope AND tmdb_id = :tmdb"));
    query.bindValue(QStringLiteral(":scope"), scope);
    query.bindValue(QStringLiteral(":tmdb"), tmdbId);

    if (!query.exec()) {
        MM_LOG_E() << "could not put film" << tmdbId << "back into" << scope
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "film" << tmdbId << "is back in" << scope;
    return true;
}

QList<HiddenCollectionFilmRecord> MediaRepository::hiddenCollectionFilms() const
{
    QList<HiddenCollectionFilmRecord> result;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT scope, tmdb_id, title, collection_name, hidden_at"
            " FROM hidden_collection_films ORDER BY hidden_at DESC"))) {
        MM_LOG_W() << "could not list the films taken out of collections"
                   << query.lastError().text();
        return result;
    }
    while (query.next()) {
        HiddenCollectionFilmRecord hidden;
        hidden.scope = query.value(0).toString();
        hidden.tmdbId = query.value(1).toLongLong();
        hidden.title = query.value(2).toString();
        hidden.collectionName = query.value(3).toString();
        hidden.hiddenAt = query.value(4).toLongLong();
        result.append(hidden);
    }
    return result;
}

QHash<QString, QSet<qint64>> MediaRepository::hiddenCollectionFilmsByScope() const
{
    QHash<QString, QSet<qint64>> byScope;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral("SELECT scope, tmdb_id FROM hidden_collection_films"))) {
        MM_LOG_W() << "could not read which films are out of their collections"
                   << query.lastError().text();
        return byScope;
    }
    while (query.next()) {
        byScope[query.value(0).toString()].insert(query.value(1).toLongLong());
    }
    return byScope;
}

bool MediaRepository::addCollectionFilm(const QString &scope,
                                       qint64 tmdbId,
                                       const QString &title,
                                       const QString &collectionName)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO added_collection_films"
        " (scope, tmdb_id, title, collection_name, added_at) "
        "VALUES (:scope, :tmdb, :title, :collection, :now) "
        "ON CONFLICT(scope, tmdb_id) DO UPDATE SET title = excluded.title, "
        "collection_name = excluded.collection_name, added_at = excluded.added_at"));
    query.bindValue(QStringLiteral(":scope"), scope);
    query.bindValue(QStringLiteral(":tmdb"), tmdbId);
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":collection"), collectionName);
    query.bindValue(QStringLiteral(":now"), QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not put film" << tmdbId << "into" << scope
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "film" << tmdbId << title << "put into" << scope;
    return true;
}

bool MediaRepository::removeAddedCollectionFilm(const QString &scope, qint64 tmdbId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "DELETE FROM added_collection_films WHERE scope = :scope AND tmdb_id = :tmdb"));
    query.bindValue(QStringLiteral(":scope"), scope);
    query.bindValue(QStringLiteral(":tmdb"), tmdbId);

    if (!query.exec()) {
        MM_LOG_E() << "could not take film" << tmdbId << "back out of" << scope
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "film" << tmdbId << "taken back out of" << scope;
    return true;
}

QHash<QString, QSet<qint64>> MediaRepository::addedCollectionFilmsByScope() const
{
    QHash<QString, QSet<qint64>> byScope;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral("SELECT scope, tmdb_id FROM added_collection_films"))) {
        MM_LOG_W() << "could not read which films were put into a collection"
                   << query.lastError().text();
        return byScope;
    }
    while (query.next()) {
        byScope[query.value(0).toString()].insert(query.value(1).toLongLong());
    }
    return byScope;
}

QSet<qint64> MediaRepository::hiddenCollectionIds() const
{
    QSet<qint64> ids;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral("SELECT collection_id FROM hidden_collections"))) {
        MM_LOG_W() << "could not read which collections are off the page"
                   << query.lastError().text();
        return ids;
    }
    while (query.next()) {
        ids.insert(query.value(0).toLongLong());
    }
    return ids;
}

bool MediaRepository::discardShow(const QStringList &fileHandles,
                                  const QString &title, const QString &folder)
{
    if (fileHandles.isEmpty()) {
        return false;
    }

    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const bool grouped = m_database.transaction();

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO discarded_shows (file_handle, title, folder, discarded_at) "
        "VALUES (:handle, :title, :folder, :now) "
        "ON CONFLICT(file_handle) DO UPDATE SET title = excluded.title, "
        "folder = excluded.folder, discarded_at = excluded.discarded_at"));

    for (const QString &handle : fileHandles) {
        query.bindValue(QStringLiteral(":handle"), handle);
        query.bindValue(QStringLiteral(":title"), title);
        query.bindValue(QStringLiteral(":folder"), folder);
        query.bindValue(QStringLiteral(":now"), now);
        if (!query.exec()) {
            MM_LOG_E() << "could not refuse" << title << "as a show"
                       << query.lastError().text();
            if (grouped) {
                m_database.rollback();
            }
            return false;
        }
    }

    if (grouped && !m_database.commit()) {
        return false;
    }

    MM_LOG_I() << title << "is not a show:" << fileHandles.size()
               << "files will not be grouped again";
    return true;
}

bool MediaRepository::restoreShow(const QString &title)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "DELETE FROM discarded_shows WHERE title = :title"));
    query.bindValue(QStringLiteral(":title"), title);

    if (!query.exec()) {
        MM_LOG_E() << "could not put" << title << "back on Identify shows"
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << title << "can be identified again";
    return true;
}

QList<DiscardedShowRecord> MediaRepository::discardedShows() const
{
    QList<DiscardedShowRecord> result;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT title, folder, COUNT(*), MAX(discarded_at) "
            "FROM discarded_shows GROUP BY title ORDER BY MAX(discarded_at) DESC"))) {
        MM_LOG_W() << "could not list the folders refused as shows"
                   << query.lastError().text();
        return result;
    }
    while (query.next()) {
        DiscardedShowRecord discarded;
        discarded.title = query.value(0).toString();
        discarded.folder = query.value(1).toString();
        discarded.fileCount = query.value(2).toInt();
        discarded.discardedAt = query.value(3).toLongLong();
        result.append(discarded);
    }
    return result;
}

QSet<QString> MediaRepository::discardedShowFiles() const
{
    QSet<QString> handles;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral("SELECT file_handle FROM discarded_shows"))) {
        MM_LOG_W() << "could not read which files were refused as shows"
                   << query.lastError().text();
        return handles;
    }
    while (query.next()) {
        handles.insert(query.value(0).toString());
    }
    return handles;
}

QList<CustomCollectionRecord> MediaRepository::customCollections() const
{
    QList<CustomCollectionRecord> result;
    QHash<qint64, int> rows;

    QSqlQuery collections(m_database.handle());
    if (!collections.exec(QStringLiteral(
            "SELECT id, name, description, cover_mode, created_at FROM custom_collections"
            " ORDER BY id"))) {
        MM_LOG_W() << "could not list the collections you made" << collections.lastError().text();
        return result;
    }
    while (collections.next()) {
        CustomCollectionRecord collection;
        collection.id = collections.value(0).toLongLong();
        collection.name = collections.value(1).toString();
        collection.description = collections.value(2).toString();
        collection.coverMode = collections.value(3).toString();
        collection.createdAt = collections.value(4).toLongLong();
        rows.insert(collection.id, int(result.size()));
        result.append(collection);
    }

    QSqlQuery items(m_database.handle());
    if (!items.exec(QStringLiteral(
            "SELECT collection_id, media_id FROM custom_collection_items"
            " ORDER BY collection_id, position"))) {
        MM_LOG_W() << "could not list the titles of the collections you made"
                   << items.lastError().text();
        return result;
    }
    while (items.next()) {
        const int row = rows.value(items.value(0).toLongLong(), -1);
        if (row >= 0) {
            result[row].mediaIds.append(items.value(1).toLongLong());
        }
    }
    return result;
}

bool MediaRepository::saveFilmDetails(const FilmDetailsRecord &details)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO film_details (tmdb_id, title, release_date, poster_path, backdrop_path,"
        "                          runtime_minutes, rating, fetched_at) "
        "VALUES (:id, :title, :released, :poster, :backdrop, :runtime, :rating, :now) "
        "ON CONFLICT(tmdb_id) DO UPDATE SET title = excluded.title, "
        "release_date = excluded.release_date, poster_path = excluded.poster_path, "
        "backdrop_path = excluded.backdrop_path, runtime_minutes = excluded.runtime_minutes, "
        "rating = excluded.rating, fetched_at = excluded.fetched_at"));
    query.bindValue(QStringLiteral(":id"), details.tmdbId);
    query.bindValue(QStringLiteral(":title"), details.title);
    query.bindValue(QStringLiteral(":released"), details.releaseDate);
    query.bindValue(QStringLiteral(":poster"), details.posterPath);
    query.bindValue(QStringLiteral(":backdrop"), details.backdropPath);
    query.bindValue(QStringLiteral(":runtime"), details.runtimeMinutes);
    query.bindValue(QStringLiteral(":rating"), details.rating);
    query.bindValue(QStringLiteral(":now"), details.fetchedAt > 0
                        ? details.fetchedAt
                        : QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not save the details of film" << details.tmdbId
                   << query.lastError().text();
        return false;
    }
    return true;
}

QHash<qint64, FilmDetailsRecord> MediaRepository::filmDetails() const
{
    QHash<qint64, FilmDetailsRecord> result;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT tmdb_id, title, release_date, poster_path, backdrop_path,"
            "       runtime_minutes, rating, fetched_at FROM film_details"))) {
        MM_LOG_W() << "could not read the film details" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        FilmDetailsRecord details;
        details.tmdbId = query.value(0).toLongLong();
        details.title = query.value(1).toString();
        details.releaseDate = query.value(2).toString();
        details.posterPath = query.value(3).toString();
        details.backdropPath = query.value(4).toString();
        details.runtimeMinutes = query.value(5).toInt();
        details.rating = query.value(6).toDouble();
        details.fetchedAt = query.value(7).toLongLong();
        result.insert(details.tmdbId, details);
    }
    return result;
}

QList<CollectionRecord> MediaRepository::ownedCollections() const
{
    QList<CollectionRecord> result;
    QHash<qint64, int> rows;

    const QString owned = QStringLiteral(
        "SELECT DISTINCT m.collection_id FROM media m"
        " WHERE m.kind = 'movie' AND m.collection_id IS NOT NULL"
        "   AND EXISTS (SELECT 1 FROM file_media fm WHERE fm.media_id = m.id)");

    QSqlQuery collections(m_database.handle());
    if (!collections.exec(QStringLiteral(
            "SELECT tmdb_id, name, overview, poster_path, backdrop_path, fetched_at"
            " FROM collections WHERE tmdb_id IN (") + owned + QStringLiteral(")"))) {
        MM_LOG_W() << "could not list the owned collections" << collections.lastError().text();
        return result;
    }
    while (collections.next()) {
        CollectionRecord collection;
        collection.tmdbId = collections.value(0).toLongLong();
        collection.name = collections.value(1).toString();
        collection.overview = collections.value(2).toString();
        collection.posterPath = collections.value(3).toString();
        collection.backdropPath = collections.value(4).toString();
        collection.fetchedAt = collections.value(5).toLongLong();
        rows.insert(collection.tmdbId, int(result.size()));
        result.append(collection);
    }

    QSqlQuery parts(m_database.handle());
    if (!parts.exec(QStringLiteral(
            "SELECT collection_id, tmdb_id, position, title, release_date, poster_path,"
            "       backdrop_path"
            " FROM collection_parts WHERE collection_id IN (") + owned + QStringLiteral(")"
            " ORDER BY collection_id, position"))) {
        MM_LOG_W() << "could not list the films of the owned collections"
                   << parts.lastError().text();
        return result;
    }
    while (parts.next()) {
        const int row = rows.value(parts.value(0).toLongLong(), -1);
        if (row < 0) {
            continue;
        }
        CollectionPartRecord part;
        part.tmdbId = parts.value(1).toLongLong();
        part.position = parts.value(2).toInt();
        part.title = parts.value(3).toString();
        part.releaseDate = parts.value(4).toString();
        part.posterPath = parts.value(5).toString();
        part.backdropPath = parts.value(6).toString();
        result[row].parts.append(part);
    }
    return result;
}

MatchOverride MediaRepository::overrideFor(const QString &fileHandle) const
{
    MatchOverride value;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT * FROM match_overrides WHERE file_handle = :handle"));
    query.bindValue(QStringLiteral(":handle"), fileHandle);

    if (query.exec() && query.next()) {
        value.fileHandle = query.value(QStringLiteral("file_handle")).toString();
        value.tmdbId = query.value(QStringLiteral("tmdb_id")).toLongLong();
        value.kind = query.value(QStringLiteral("kind")).toString();
        value.season = query.value(QStringLiteral("season")).toInt();
        value.episode = query.value(QStringLiteral("episode")).toInt();
    }
    return value;
}

bool MediaRepository::setOverride(const MatchOverride &value)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO match_overrides (file_handle, tmdb_id, kind, season, episode, pinned_at) "
        "VALUES (:handle, :tmdb, :kind, :season, :episode, :pinned) "
        "ON CONFLICT(file_handle) DO UPDATE SET "
        "  tmdb_id = excluded.tmdb_id,"
        "  kind = excluded.kind,"
        "  season = excluded.season,"
        "  episode = excluded.episode,"
        "  pinned_at = excluded.pinned_at"));

    query.bindValue(QStringLiteral(":handle"), value.fileHandle);
    query.bindValue(QStringLiteral(":tmdb"), value.tmdbId);
    query.bindValue(QStringLiteral(":kind"), value.kind);
    query.bindValue(QStringLiteral(":season"), value.season);
    query.bindValue(QStringLiteral(":episode"), value.episode);
    query.bindValue(QStringLiteral(":pinned"), QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not pin a match for" << value.fileHandle
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "match pinned by hand for" << value.fileHandle
               << "tmdb" << value.tmdbId;
    return true;
}

bool MediaRepository::clearOverride(const QString &fileHandle)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "DELETE FROM match_overrides WHERE file_handle = :handle"));
    query.bindValue(QStringLiteral(":handle"), fileHandle);

    if (!query.exec()) {
        MM_LOG_E() << "could not clear the pinned match for" << fileHandle
                   << query.lastError().text();
        return false;
    }
    return true;
}

qint64 MediaRepository::pin(const PinRecord &request)
{
    if (!m_database.transaction()) {
        return -1;
    }

    const qint64 mediaId = upsertMedia(request.media);
    bool written = mediaId > 0;

    written = written && linkFile(request.fileId, mediaId, 1.0, false);

    written = written && detachFileFromOtherEpisodes(request.fileId, mediaId);

    if (written && request.media.kind == QLatin1String("tv")
        && request.season > 0 && request.episode > 0) {
        EpisodeRecord episode;
        episode.mediaId = mediaId;
        episode.season = request.season;
        episode.episode = request.episode;
        episode.title = request.episodeTitle;
        written = upsertEpisode(episode, request.fileId);
    }

    if (written) {
        MatchOverride value;
        value.fileHandle = request.fileHandle;
        value.tmdbId = request.media.tmdbId;
        value.kind = request.media.kind;
        value.season = request.season;
        value.episode = request.episode;
        written = setOverride(value);
    }

    if (!written || !m_database.commit()) {
        m_database.rollback();
        MM_LOG_E() << "pin for" << request.fileHandle
                   << "failed and was rolled back, nothing was written";
        return -1;
    }

    return mediaId;
}

int MediaRepository::matchedCount() const
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM file_media WHERE suggested = 0 AND media_id IS NOT NULL"))
        || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int MediaRepository::suggestedCount() const
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM file_media WHERE suggested = 1"))
        || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}
