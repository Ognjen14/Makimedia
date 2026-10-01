#include "Data/PlaybackStateRepository.h"

#include "Data/Database.h"
#include "Metadata/FileNameParser.h"
#include "MmLog.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace {

const double kFinishedThreshold = 0.95;
const double kResumeAfterSeconds = 30.0;

struct Stranded
{
    qint64 fileId = 0;
    double position = 0.0;
    double duration = 0.0;
    double watchedSeconds = 0.0;
    int watched = 0;
    QVariant lastPlayed;
    QVariant season;
    QVariant episode;
};

}

double PlaybackStateRepository::resumeSeconds(double positionSeconds,
                                              double durationSeconds,
                                              bool watched)
{
    if (watched || positionSeconds <= kResumeAfterSeconds) {
        return 0.0;
    }

    if (isFinished(positionSeconds, durationSeconds)) {
        return 0.0;
    }

    return positionSeconds;
}

bool PlaybackStateRepository::resumable(double positionSeconds,
                                        double durationSeconds,
                                        bool watched)
{
    return resumeSeconds(positionSeconds, durationSeconds, watched) > 0.0;
}

double PlaybackStateRepository::resumeAfterSeconds()
{
    return kResumeAfterSeconds;
}

double PlaybackStateRepository::finishedThresholdFraction()
{
    return kFinishedThreshold;
}

bool PlaybackStateRepository::isFinished(double positionSeconds, double durationSeconds)
{
    if (durationSeconds <= 0.0) {
        return false;
    }
    return (positionSeconds / durationSeconds) >= kFinishedThreshold;
}

PlaybackStateRepository::PlaybackStateRepository(Database &database)
    : m_database(database)
{
}

PlaybackState PlaybackStateRepository::forFile(qint64 fileId) const
{
    PlaybackState state;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT file_id, position_seconds, duration_seconds, watched_seconds,"
        "       watched, last_played "
        "FROM playback_state WHERE file_id = :file_id"));
    query.bindValue(QStringLiteral(":file_id"), fileId);

    if (!query.exec() || !query.next()) {
        return state;
    }

    state.fileId = query.value(0).toLongLong();
    state.positionSeconds = query.value(1).toDouble();
    state.durationSeconds = query.value(2).toDouble();
    state.watchedSeconds = query.value(3).toDouble();
    state.watched = query.value(4).toInt() != 0;

    const QVariant lastPlayed = query.value(5);
    if (!lastPlayed.isNull()) {
        state.lastPlayed = QDateTime::fromSecsSinceEpoch(lastPlayed.toLongLong());
    }
    return state;
}

QList<qint64> PlaybackStateRepository::mediaWithMissingHistory() const
{
    QList<qint64> mediaIds;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT DISTINCT fm.media_id FROM file_media fm"
            " JOIN files f ON f.id = fm.file_id"
            " JOIN playback_state p ON p.file_id = f.id"
            " WHERE f.missing = 1 AND (p.position_seconds > 0 OR p.watched = 1)"))) {
        MM_LOG_W() << "could not look for watch history left on missing files"
                   << query.lastError().text();
        return mediaIds;
    }
    while (query.next()) {
        mediaIds.append(query.value(0).toLongLong());
    }
    return mediaIds;
}

QList<qint64> PlaybackStateRepository::adoptFromMissing(const QList<qint64> &mediaIds)
{
    QList<qint64> adopted;
    if (mediaIds.isEmpty()) {
        return adopted;
    }

    for (const qint64 mediaId : mediaIds) {
        QSqlQuery source(m_database.handle());
        source.prepare(QStringLiteral(
            "SELECT p.file_id, p.position_seconds, p.duration_seconds,"
            "       p.watched_seconds, p.watched, p.last_played,"
            "       se.season, se.episode, f.handle"
            " FROM file_media fm"
            " JOIN files f ON f.id = fm.file_id"
            " JOIN playback_state p ON p.file_id = f.id"
            " LEFT JOIN episodes se ON se.file_id = f.id"
            " WHERE fm.media_id = :media AND f.missing = 1"
            "   AND (p.position_seconds > 0 OR p.watched = 1)"
            " ORDER BY p.last_played DESC"));
        source.bindValue(QStringLiteral(":media"), mediaId);
        if (!source.exec()) {
            MM_LOG_W() << "could not read the history left on missing files of media"
                       << mediaId << source.lastError().text();
            continue;
        }

        QList<Stranded> stranded;
        while (source.next()) {
            Stranded entry;
            entry.fileId = source.value(0).toLongLong();
            entry.position = source.value(1).toDouble();
            entry.duration = source.value(2).toDouble();
            entry.watchedSeconds = source.value(3).toDouble();
            entry.watched = source.value(4).toInt();
            entry.lastPlayed = source.value(5);
            entry.season = source.value(6);
            entry.episode = source.value(7);

            if (entry.season.isNull() || entry.episode.isNull()) {
                const ParsedFileName parsed =
                    FileNameParser::parsePath(source.value(8).toString());
                if (parsed.season > 0 && parsed.episode > 0) {
                    entry.season = parsed.season;
                    entry.episode = parsed.episode;
                }
            }

            stranded.append(entry);
        }

        for (const Stranded &entry : std::as_const(stranded)) {
            const bool ofAnEpisode = !entry.season.isNull() && !entry.episode.isNull();

            QSqlQuery targets(m_database.handle());
            targets.prepare(QStringLiteral(
                "SELECT f.id FROM file_media fm"
                " JOIN files f ON f.id = fm.file_id"
                " LEFT JOIN playback_state p ON p.file_id = f.id"
                " LEFT JOIN episodes te ON te.file_id = f.id"
                " WHERE fm.media_id = :media AND f.missing = 0 AND f.id <> :from"
                "   AND (p.file_id IS NULL"
                "        OR (p.watched = 0 AND p.position_seconds <= :after))"
                "   AND ((:ofAnEpisode = 0 AND te.id IS NULL)"
                "        OR (:ofAnEpisode = 1 AND te.season = :season"
                "            AND te.episode = :episode))"));
            targets.bindValue(QStringLiteral(":media"), mediaId);
            targets.bindValue(QStringLiteral(":from"), entry.fileId);
            targets.bindValue(QStringLiteral(":after"), kResumeAfterSeconds);
            targets.bindValue(QStringLiteral(":ofAnEpisode"), ofAnEpisode ? 1 : 0);
            targets.bindValue(QStringLiteral(":season"), entry.season);
            targets.bindValue(QStringLiteral(":episode"), entry.episode);
            if (!targets.exec()) {
                MM_LOG_W() << "could not look for a replacement for file" << entry.fileId
                           << targets.lastError().text();
                continue;
            }

            QList<qint64> takers;
            while (targets.next()) {
                takers.append(targets.value(0).toLongLong());
            }

            for (const qint64 fileId : std::as_const(takers)) {
                QSqlQuery write(m_database.handle());
                write.prepare(QStringLiteral(
                    "INSERT INTO playback_state (file_id, position_seconds,"
                    "                            duration_seconds, watched_seconds,"
                    "                            watched, last_played) "
                    "VALUES (:file, :position, :duration, :watchedSeconds,"
                    "        :watched, :played) "
                    "ON CONFLICT(file_id) DO UPDATE SET"
                    "  position_seconds = excluded.position_seconds,"
                    "  duration_seconds = excluded.duration_seconds,"
                    "  watched_seconds = excluded.watched_seconds,"
                    "  watched = excluded.watched,"
                    "  last_played = excluded.last_played"));
                write.bindValue(QStringLiteral(":file"), fileId);
                write.bindValue(QStringLiteral(":position"), entry.position);
                write.bindValue(QStringLiteral(":duration"), entry.duration);
                write.bindValue(QStringLiteral(":watchedSeconds"), entry.watchedSeconds);
                write.bindValue(QStringLiteral(":watched"), entry.watched);
                write.bindValue(QStringLiteral(":played"), entry.lastPlayed);

                if (!write.exec()) {
                    MM_LOG_E() << "could not carry watch history to file" << fileId
                               << write.lastError().text();
                    continue;
                }

                MM_LOG_I() << "file" << fileId
                           << "took over the watch history of missing file"
                           << entry.fileId << "at" << entry.position
                           << "seconds, watched" << bool(entry.watched);
                adopted.append(fileId);
            }
        }
    }

    return adopted;
}

bool PlaybackStateRepository::save(const PlaybackState &state)
{
    if (!state.isValid()) {
        MM_LOG_W() << "refusing to save playback state without a file id";
        return false;
    }

    const bool finished =
        state.watched || isFinished(state.positionSeconds, state.durationSeconds);

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO playback_state (file_id, position_seconds, duration_seconds,"
        "                            watched_seconds, watched, last_played) "
        "VALUES (:file_id, :position,"
        "        CASE WHEN :duration > 0 THEN :duration_again"
        "             ELSE COALESCE((SELECT duration_seconds FROM files"
        "                            WHERE id = :duration_file), 0) END,"
        "        :watched_seconds, :watched, :last_played) "
        "ON CONFLICT(file_id) DO UPDATE SET "
        "  position_seconds = excluded.position_seconds,"
        "  duration_seconds = CASE WHEN :duration_check > 0"
        "                          THEN excluded.duration_seconds"
        "                          WHEN duration_seconds > 0 THEN duration_seconds"
        "                          ELSE excluded.duration_seconds END,"
        "  watched_seconds = excluded.watched_seconds,"
        "  watched = excluded.watched,"
        "  last_played = excluded.last_played"));

    const QDateTime when = state.lastPlayed.isValid()
        ? state.lastPlayed
        : QDateTime::currentDateTimeUtc();

    query.bindValue(QStringLiteral(":file_id"), state.fileId);
    query.bindValue(QStringLiteral(":position"), state.positionSeconds);
    query.bindValue(QStringLiteral(":duration"), state.durationSeconds);
    query.bindValue(QStringLiteral(":duration_again"), state.durationSeconds);
    query.bindValue(QStringLiteral(":duration_file"), state.fileId);
    query.bindValue(QStringLiteral(":duration_check"), state.durationSeconds);
    query.bindValue(QStringLiteral(":watched_seconds"), state.watchedSeconds);
    query.bindValue(QStringLiteral(":watched"), finished ? 1 : 0);
    query.bindValue(QStringLiteral(":last_played"), when.toSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not save playback state for file" << state.fileId
                   << query.lastError().text();
        return false;
    }

    MM_LOG_D() << "playback state saved, file" << state.fileId
               << "position" << state.positionSeconds
               << "of" << state.durationSeconds
               << (finished ? "watched" : "partial");
    return true;
}

bool PlaybackStateRepository::setWatched(qint64 fileId, bool watched)
{
    return setWatched(QList<qint64>{fileId}, watched);
}

bool PlaybackStateRepository::setWatched(const QList<qint64> &fileIds, bool watched)
{
    if (fileIds.isEmpty()) {
        return true;
    }

    if (!m_database.transaction()) {
        return false;
    }

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO playback_state (file_id, position_seconds, duration_seconds,"
        "                            watched_seconds, watched, last_played) "
        "VALUES (:file_id, 0,"
        "        COALESCE((SELECT duration_seconds FROM files"
        "                  WHERE id = :duration_file), 0),"
        "        0, :watched, :last_played) "
        "ON CONFLICT(file_id) DO UPDATE SET "
        "  watched = excluded.watched,"
        "  last_played = excluded.last_played,"
        "  duration_seconds = CASE WHEN duration_seconds > 0 THEN duration_seconds"
        "                          ELSE excluded.duration_seconds END,"
        "  position_seconds = CASE WHEN excluded.watched = 1"
        "                          THEN position_seconds ELSE 0 END,"
        "  watched_seconds = CASE WHEN excluded.watched = 1"
        "                         THEN watched_seconds ELSE 0 END"));

    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    for (const qint64 fileId : fileIds) {
        query.bindValue(QStringLiteral(":file_id"), fileId);
        query.bindValue(QStringLiteral(":duration_file"), fileId);
        query.bindValue(QStringLiteral(":watched"), watched ? 1 : 0);
        query.bindValue(QStringLiteral(":last_played"), now);

        if (!query.exec()) {
            MM_LOG_E() << "could not set watched flag for file" << fileId
                       << query.lastError().text() << "- none of the"
                       << fileIds.size() << "files were changed";
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        m_database.rollback();
        return false;
    }

    MM_LOG_I() << fileIds.size()
               << (watched ? "file(s) marked watched" : "file(s) marked unwatched");
    return true;
}

bool PlaybackStateRepository::clear(qint64 fileId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "DELETE FROM playback_state WHERE file_id = :file_id"));
    query.bindValue(QStringLiteral(":file_id"), fileId);

    if (!query.exec()) {
        MM_LOG_E() << "could not clear playback state for file" << fileId
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "playback state cleared for file" << fileId;
    return true;
}
