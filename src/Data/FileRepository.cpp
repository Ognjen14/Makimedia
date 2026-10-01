#include "Data/FileRepository.h"

#include "Data/Database.h"
#include "Data/PlaybackStateRepository.h"
#include "MmLog.h"
#include "TextFold.h"

#include <algorithm>
#include <utility>

#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace {

const QString kSelectColumns = QStringLiteral(
    "f.id, f.folder_id, f.handle, f.parent_handle, f.display_name, "
    "f.size_bytes, f.modified, f.duration_seconds, f.container, "
    "f.video_codec, f.audio_codec, f.width, f.height, f.hdr, "
    "f.audio_tracks, f.subtitle_tracks, f.match_attempted, f.missing, "
    "p.position_seconds, p.duration_seconds AS played_duration, "
    "p.watched_seconds, p.watched, p.last_played, "
    "m.title AS matched_title, m.year AS matched_year, "
    "m.poster_path AS matched_poster, fm.suggested AS match_suggested, "
    "m.backdrop_path AS matched_backdrop, "
    "m.kind AS matched_kind, "
    "e.season AS episode_season, e.episode AS episode_number, "
    "e.title AS episode_title, e.still_path AS episode_still");

const QString kFromClause = QStringLiteral(
    " FROM files f"
    " LEFT JOIN playback_state p ON p.file_id = f.id"
    " LEFT JOIN file_media fm ON fm.file_id = f.id"
    " LEFT JOIN media m ON m.id = fm.media_id"
    " LEFT JOIN episodes e ON e.file_id = f.id");

struct FileColumns
{
    explicit FileColumns(const QSqlQuery &query)
    {
        const QSqlRecord record = query.record();
        id = record.indexOf(QStringLiteral("id"));
        folderId = record.indexOf(QStringLiteral("folder_id"));
        handle = record.indexOf(QStringLiteral("handle"));
        parentHandle = record.indexOf(QStringLiteral("parent_handle"));
        displayName = record.indexOf(QStringLiteral("display_name"));
        sizeBytes = record.indexOf(QStringLiteral("size_bytes"));
        modified = record.indexOf(QStringLiteral("modified"));
        durationSeconds = record.indexOf(QStringLiteral("duration_seconds"));
        container = record.indexOf(QStringLiteral("container"));
        videoCodec = record.indexOf(QStringLiteral("video_codec"));
        audioCodec = record.indexOf(QStringLiteral("audio_codec"));
        width = record.indexOf(QStringLiteral("width"));
        height = record.indexOf(QStringLiteral("height"));
        hdr = record.indexOf(QStringLiteral("hdr"));
        audioTracks = record.indexOf(QStringLiteral("audio_tracks"));
        subtitleTracks = record.indexOf(QStringLiteral("subtitle_tracks"));
        matchAttempted = record.indexOf(QStringLiteral("match_attempted"));
        missing = record.indexOf(QStringLiteral("missing"));
        positionSeconds = record.indexOf(QStringLiteral("position_seconds"));
        playedDuration = record.indexOf(QStringLiteral("played_duration"));
        watchedSeconds = record.indexOf(QStringLiteral("watched_seconds"));
        watched = record.indexOf(QStringLiteral("watched"));
        lastPlayed = record.indexOf(QStringLiteral("last_played"));
        matchedTitle = record.indexOf(QStringLiteral("matched_title"));
        matchedYear = record.indexOf(QStringLiteral("matched_year"));
        matchedPoster = record.indexOf(QStringLiteral("matched_poster"));
        matchedBackdrop = record.indexOf(QStringLiteral("matched_backdrop"));
        matchSuggested = record.indexOf(QStringLiteral("match_suggested"));
        matchedKind = record.indexOf(QStringLiteral("matched_kind"));
        episodeSeason = record.indexOf(QStringLiteral("episode_season"));
        episodeNumber = record.indexOf(QStringLiteral("episode_number"));
        episodeTitle = record.indexOf(QStringLiteral("episode_title"));
        episodeStill = record.indexOf(QStringLiteral("episode_still"));
    }

    int id, folderId, handle, parentHandle, displayName, sizeBytes, modified;
    int durationSeconds, container, videoCodec, audioCodec, width, height, hdr;
    int audioTracks, subtitleTracks, matchAttempted, missing;
    int positionSeconds, playedDuration, watchedSeconds, watched, lastPlayed;
    int matchedTitle, matchedYear, matchedPoster, matchedBackdrop;
    int matchSuggested, matchedKind;
    int episodeSeason, episodeNumber, episodeTitle, episodeStill;
};

LibraryFile fileFromQuery(const QSqlQuery &query, const FileColumns &at)
{
    LibraryFile file;
    file.id = query.value(at.id).toLongLong();
    file.folderId = query.value(at.folderId).toLongLong();
    file.handle = query.value(at.handle).toString();
    file.parentHandle = query.value(at.parentHandle).toString();
    file.displayName = query.value(at.displayName).toString();
    file.sizeBytes = query.value(at.sizeBytes).toLongLong();

    const QVariant modified = query.value(at.modified);
    if (!modified.isNull()) {
        file.modified = QDateTime::fromSecsSinceEpoch(modified.toLongLong());
    }

    file.durationSeconds = query.value(at.durationSeconds).toDouble();
    file.container = query.value(at.container).toString();
    file.videoCodec = query.value(at.videoCodec).toString();
    file.audioCodec = query.value(at.audioCodec).toString();
    file.width = query.value(at.width).toInt();
    file.height = query.value(at.height).toInt();
    file.hdr = query.value(at.hdr).toInt() != 0;
    file.audioTrackCount = query.value(at.audioTracks).toInt();
    file.subtitleTrackCount = query.value(at.subtitleTracks).toInt();
    file.matchAttempted = !query.value(at.matchAttempted).isNull();
    file.missing = query.value(at.missing).toInt() != 0;

    const QVariant lastPlayed = query.value(at.lastPlayed);
    if (!lastPlayed.isNull()) {
        file.playback.fileId = file.id;
        file.playback.positionSeconds = query.value(at.positionSeconds).toDouble();
        file.playback.durationSeconds = query.value(at.playedDuration).toDouble();
        file.playback.watchedSeconds = query.value(at.watchedSeconds).toDouble();
        file.playback.watched = query.value(at.watched).toInt() != 0;
        file.playback.lastPlayed =
            QDateTime::fromSecsSinceEpoch(lastPlayed.toLongLong());
    }

    file.matchedTitle = query.value(at.matchedTitle).toString();
    file.matchedYear = query.value(at.matchedYear).toInt();
    file.matchedPosterPath = query.value(at.matchedPoster).toString();
    file.matchedBackdropPath = query.value(at.matchedBackdrop).toString();
    file.matchSuggested = query.value(at.matchSuggested).toInt() != 0;
    file.matchedKind = query.value(at.matchedKind).toString();
    file.season = query.value(at.episodeSeason).toInt();
    file.episode = query.value(at.episodeNumber).toInt();
    file.episodeTitle = query.value(at.episodeTitle).toString();
    file.episodeStillPath = query.value(at.episodeStill).toString();

    return file;
}

QList<LibraryFile> runSelect(Database &database, const QString &sql,
                             const QVariantMap &bindings = QVariantMap())
{
    QList<LibraryFile> files;

    QSqlQuery query(database.handle());
    query.prepare(sql);
    for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }

    if (!query.exec()) {
        MM_LOG_E() << "file query failed" << query.lastError().text();
        return files;
    }

    const FileColumns at(query);

    while (query.next()) {
        files.append(fileFromQuery(query, at));
    }
    return files;
}

QList<LibraryFile> runPlainSelect(Database &database, const QString &sql,
                                  const QVariantMap &bindings = QVariantMap())
{
    QList<LibraryFile> files;

    QSqlQuery query(database.handle());
    query.prepare(sql);
    for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }

    if (!query.exec()) {
        MM_LOG_E() << "file query failed" << query.lastError().text();
        return files;
    }

    while (query.next()) {
        LibraryFile file;
        file.id = query.value(0).toLongLong();
        file.folderId = query.value(1).toLongLong();
        file.handle = query.value(2).toString();
        file.parentHandle = query.value(3).toString();
        file.displayName = query.value(4).toString();
        files.append(file);
    }
    return files;
}

}

FileRepository::FileRepository(Database &database)
    : m_database(database)
{
}

int FileRepository::upsertBatch(qint64 folderId, const QList<MediaFileInfo> &files)
{
    if (files.isEmpty()) {
        return 0;
    }

    ScanIndex index = scanIndex(folderId);
    return upsertBatch(folderId, files, index).written();
}

ScanIndex FileRepository::scanIndex(qint64 folderId) const
{
    ScanIndex index;

    QSqlQuery lookup(m_database.handle());
    lookup.prepare(QStringLiteral(
        "SELECT handle, size_bytes, modified, missing FROM files "
        "WHERE folder_id = :folder_id"));
    lookup.bindValue(QStringLiteral(":folder_id"), folderId);
    if (!lookup.exec()) {
        MM_LOG_E() << "could not read the files already in folder" << folderId
                   << lookup.lastError().text();
        return index;
    }

    while (lookup.next()) {
        ScanIndexRow row;
        row.sizeBytes = lookup.value(1).toLongLong();
        row.modified = lookup.value(2).toLongLong();
        row.missing = lookup.value(3).toInt() != 0;
        index.insert(lookup.value(0).toString(), row);
    }

    QSqlQuery removed(m_database.handle());
    removed.prepare(QStringLiteral(
        "SELECT file_handle FROM removed_files WHERE folder_id = :folder_id"));
    removed.bindValue(QStringLiteral(":folder_id"), folderId);
    if (!removed.exec()) {
        MM_LOG_E() << "could not read the files taken out of folder" << folderId
                   << removed.lastError().text();
        return index;
    }

    while (removed.next()) {
        ScanIndexRow row;
        row.removed = true;
        index.insert(removed.value(0).toString(), row);
    }
    return index;
}

BatchWrite FileRepository::upsertBatch(qint64 folderId,
                                       const QList<MediaFileInfo> &files,
                                       ScanIndex &index)
{
    BatchWrite result;
    if (files.isEmpty()) {
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    if (!m_database.transaction()) {
        return result;
    }

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO files (folder_id, handle, parent_handle, display_name, "
        "                   name_key, size_bytes, modified, added, missing) "
        "VALUES (:folder_id, :handle, :parent_handle, :display_name, "
        "        :name_key, :size_bytes, :modified, :added, 0) "
        "ON CONFLICT(handle) DO UPDATE SET "
        "  parent_handle = excluded.parent_handle,"
        "  display_name = excluded.display_name,"
        "  name_key = excluded.name_key,"
        "  size_bytes = excluded.size_bytes,"
        "  modified = excluded.modified,"
        "  match_attempted = NULL,"
        "  probe_attempted = NULL,"
        "  duration_seconds = NULL,"
        "  missing = 0"));

    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

    for (const MediaFileInfo &info : files) {
        const qint64 modified = info.modified.isValid()
            ? info.modified.toSecsSinceEpoch()
            : 0;

        const auto known = index.constFind(info.handle);
        if (known != index.constEnd() && known->removed) {
            ++result.skipped;
            continue;
        }
        const bool isNew = known == index.constEnd();
        const bool wasMissing = !isNew && known->missing;
        if (!isNew && !wasMissing
            && known->sizeBytes == info.sizeBytes && known->modified == modified) {
            ++result.skipped;
            continue;
        }

        query.bindValue(QStringLiteral(":folder_id"), folderId);
        query.bindValue(QStringLiteral(":handle"), info.handle);
        query.bindValue(QStringLiteral(":parent_handle"), info.parentHandle);
        query.bindValue(QStringLiteral(":display_name"), info.displayName);
        query.bindValue(QStringLiteral(":name_key"), TextFold::key(info.displayName));
        query.bindValue(QStringLiteral(":size_bytes"), info.sizeBytes);
        query.bindValue(QStringLiteral(":modified"),
                        info.modified.isValid()
                            ? QVariant(info.modified.toSecsSinceEpoch())
                            : QVariant());
        query.bindValue(QStringLiteral(":added"), now);

        if (!query.exec()) {
            MM_LOG_E() << "could not write file row" << info.handle
                       << query.lastError().text();
            continue;
        }

        if (isNew) {
            result.inserted.append(info.handle);
        } else if (wasMissing) {
            result.revived.append(info.handle);
        } else {
            result.changed.append(info.handle);
        }

        ScanIndexRow row;
        row.sizeBytes = info.sizeBytes;
        row.modified = modified;
        row.missing = false;
        index.insert(info.handle, row);
    }

    if (!m_database.commit()) {
        m_database.rollback();
        return BatchWrite();
    }

    MM_LOG_D() << "skipped" << result.skipped << "unchanged files;"
               << "wrote" << result.written() << "of" << files.size()
               << "files for folder" << folderId << "-"
               << result.inserted.size() << "new," << result.changed.size()
               << "changed," << result.revived.size() << "back - in"
               << timer.elapsed() << "ms";
    return result;
}

int FileRepository::markMissingOutside(qint64 folderId, const QList<MediaFileInfo> &seen)
{
    QSet<QString> handles;
    handles.reserve(seen.size());
    for (const MediaFileInfo &info : seen) {
        handles.insert(info.handle);
    }

    ScanIndex index = scanIndex(folderId);
    return int(markMissingOutside(folderId, handles, index).size());
}

QStringList FileRepository::markMissingOutside(qint64 folderId,
                                               const QSet<QString> &seen,
                                               ScanIndex &index)
{
    if (seen.isEmpty()) {
        MM_LOG_W() << "a scan of folder" << folderId
                   << "saw no files at all, so nothing is marked missing";
        return QStringList();
    }

    QStringList missing;
    for (auto it = index.constBegin(); it != index.constEnd(); ++it) {
        if (!it->missing && !it->removed && !seen.contains(it.key())) {
            missing.append(it.key());
        }
    }

    if (missing.isEmpty()) {
        return missing;
    }

    if (!m_database.transaction()) {
        return QStringList();
    }

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE files SET missing = 1 WHERE handle = :handle"));

    QStringList marked;
    for (const QString &handle : std::as_const(missing)) {
        query.bindValue(QStringLiteral(":handle"), handle);
        if (query.exec()) {
            marked.append(handle);
        }
    }

    if (!m_database.commit()) {
        m_database.rollback();
        return QStringList();
    }

    for (const QString &handle : std::as_const(marked)) {
        index[handle].missing = true;
    }

    MM_LOG_I() << "marked" << marked.size() << "files missing in folder" << folderId;
    return marked;
}

QList<QPair<QString, QString>> FileRepository::settleMoves(
    const QStringList &missingHandles, const QStringList &writtenHandles)
{
    QList<QPair<QString, QString>> settled;
    if (missingHandles.isEmpty() || writtenHandles.isEmpty()) {
        return settled;
    }

    const auto fingerprint = [](const LibraryFile &file) {
        return QStringLiteral("%1|%2|%3")
            .arg(file.sizeBytes)
            .arg(file.modified.toSecsSinceEpoch())
            .arg(file.displayName.toLower());
    };

    QHash<QString, QList<LibraryFile>> gone;
    for (const LibraryFile &file : byHandles(missingHandles)) {
        if (file.missing && file.sizeBytes > 0 && file.modified.isValid()) {
            gone[fingerprint(file)].append(file);
        }
    }
    if (gone.isEmpty()) {
        return settled;
    }

    QHash<QString, QList<LibraryFile>> arrived;
    for (const LibraryFile &file : byHandles(writtenHandles)) {
        if (!file.missing && file.sizeBytes > 0 && file.modified.isValid()
            && gone.contains(fingerprint(file))) {
            arrived[fingerprint(file)].append(file);
        }
    }

    for (auto it = arrived.cbegin(); it != arrived.cend(); ++it) {
        const QList<LibraryFile> &from = gone.value(it.key());
        const QList<LibraryFile> &to = it.value();
        if (from.size() != 1 || to.size() != 1) {
            MM_LOG_I() << "not settling a move for" << it.key()
                       << "-" << from.size() << "gone and" << to.size()
                       << "arrived, which is ambiguous";
            continue;
        }

        const LibraryFile &before = from.first();
        const LibraryFile &after = to.first();
        if (before.id == after.id) {
            continue;
        }

        if (!m_database.transaction()) {
            return settled;
        }

        bool ok = true;
        const auto run = [&](const QString &sql, const QVariantMap &bindings) {
            if (!ok) {
                return;
            }
            QSqlQuery query(m_database.handle());
            query.prepare(sql);
            for (auto b = bindings.cbegin(); b != bindings.cend(); ++b) {
                query.bindValue(b.key(), b.value());
            }
            if (!query.exec()) {
                MM_LOG_E() << "could not settle the move of" << before.handle
                           << "to" << after.handle << query.lastError().text();
                ok = false;
            }
        };

        run(QStringLiteral("UPDATE episodes SET file_id = :before"
                           " WHERE file_id = :after"),
            {{QStringLiteral(":before"), before.id}, {QStringLiteral(":after"), after.id}});

        run(QStringLiteral("DELETE FROM files WHERE id = :after"),
            {{QStringLiteral(":after"), after.id}});

        run(QStringLiteral("UPDATE files SET handle = :handle,"
                           " parent_handle = :parent, folder_id = :folder,"
                           " missing = 0 WHERE id = :before"),
            {{QStringLiteral(":handle"), after.handle},
             {QStringLiteral(":parent"), after.parentHandle},
             {QStringLiteral(":folder"), after.folderId},
             {QStringLiteral(":before"), before.id}});

        run(QStringLiteral("UPDATE OR REPLACE external_subtitles"
                           " SET file_handle = :handle WHERE file_handle = :was"),
            {{QStringLiteral(":handle"), after.handle},
             {QStringLiteral(":was"), before.handle}});

        if (!ok) {
            m_database.rollback();
            continue;
        }
        if (!m_database.commit()) {
            continue;
        }

        MM_LOG_I() << "file" << before.id << "moved rather than vanished:"
                   << before.handle << "->" << after.handle;
        settled.append({before.handle, after.handle});
    }

    return settled;
}

QList<LibraryFile> FileRepository::byHandles(const QStringList &handles) const
{
    constexpr qsizetype kChunk = 500;

    QList<LibraryFile> files;
    for (qsizetype start = 0; start < handles.size(); start += kChunk) {
        const qsizetype end = qMin(handles.size(), start + kChunk);

        QStringList marks;
        QVariantMap bindings;
        for (qsizetype i = start; i < end; ++i) {
            const QString name = QStringLiteral(":h%1").arg(i - start);
            marks.append(name);
            bindings.insert(name, handles.at(i));
        }

        files += runSelect(m_database,
            QStringLiteral("SELECT ") + kSelectColumns + kFromClause
            + QStringLiteral(" WHERE f.handle IN (")
            + marks.join(QLatin1Char(',')) + QLatin1Char(')'),
            bindings);
    }
    return files;
}

QList<LibraryFile> FileRepository::matchCandidates(MatchScope scope,
                                                   const QStringList &handles) const
{
    constexpr qsizetype kChunk = 500;

    QString where = QStringLiteral(
        " WHERE f.missing = 0 AND mo.file_handle IS NULL");
    if (scope == MatchScope::Unasked) {
        where += QStringLiteral(
            " AND f.match_attempted IS NULL AND fm.file_id IS NULL");
    }

    const QString select = QStringLiteral(
        "SELECT f.id, f.folder_id, f.handle, f.parent_handle, f.display_name"
        " FROM files f"
        " LEFT JOIN file_media fm ON fm.file_id = f.id"
        " LEFT JOIN match_overrides mo ON mo.file_handle = f.handle") + where;

    const auto read = [this](const QString &sql, const QVariantMap &bindings,
                             QList<LibraryFile> &into) {
        into += runPlainSelect(m_database, sql, bindings);
    };

    QList<LibraryFile> files;

    if (handles.isEmpty()) {
        read(select + QStringLiteral(" ORDER BY f.id"), QVariantMap(), files);
        return files;
    }

    for (qsizetype start = 0; start < handles.size(); start += kChunk) {
        const qsizetype end = qMin(handles.size(), start + kChunk);

        QStringList marks;
        QVariantMap bindings;
        for (qsizetype i = start; i < end; ++i) {
            const QString name = QStringLiteral(":h%1").arg(i - start);
            marks.append(name);
            bindings.insert(name, handles.at(i));
        }

        read(select + QStringLiteral(" AND f.handle IN (")
                 + marks.join(QLatin1Char(',')) + QStringLiteral(") ORDER BY f.id"),
             bindings, files);
    }
    return files;
}

LibraryFile FileRepository::plainById(qint64 id) const
{
    LibraryFile file;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT id, folder_id, handle, parent_handle, display_name, missing,"
        "       match_attempted"
        " FROM files WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec() || !query.next()) {
        return file;
    }

    file.id = query.value(0).toLongLong();
    file.folderId = query.value(1).toLongLong();
    file.handle = query.value(2).toString();
    file.parentHandle = query.value(3).toString();
    file.displayName = query.value(4).toString();
    file.missing = query.value(5).toInt() != 0;
    file.matchAttempted = !query.value(6).isNull();
    return file;
}

LibraryFile FileRepository::probeFieldsFor(const QString &handle) const
{
    LibraryFile file;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT id, duration_seconds, container, video_codec, audio_codec,"
        "       width, height, hdr, audio_tracks, subtitle_tracks "
        "FROM files WHERE handle = :handle"));
    query.bindValue(QStringLiteral(":handle"), handle);

    if (!query.exec() || !query.next()) {
        return file;
    }

    file.id = query.value(0).toLongLong();
    file.handle = handle;
    file.durationSeconds = query.value(1).toDouble();
    file.container = query.value(2).toString();
    file.videoCodec = query.value(3).toString();
    file.audioCodec = query.value(4).toString();
    file.width = query.value(5).toInt();
    file.height = query.value(6).toInt();
    file.hdr = query.value(7).toInt() != 0;
    file.audioTrackCount = query.value(8).toInt();
    file.subtitleTrackCount = query.value(9).toInt();
    return file;
}

bool FileRepository::markProbeAttemptedFor(const QString &handle)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE files SET probe_attempted = :at WHERE handle = :handle"));
    query.bindValue(QStringLiteral(":at"),
                    QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    query.bindValue(QStringLiteral(":handle"), handle);

    if (!query.exec()) {
        MM_LOG_E() << "could not record a probe attempt for" << handle
                   << query.lastError().text();
        return false;
    }
    return true;
}

QStringList FileRepository::failedProbeHandles(const QList<qint64> &folderIds) const
{
    QStringList handles;
    if (folderIds.isEmpty()) {
        return handles;
    }

    QStringList numbers;
    numbers.reserve(folderIds.size());
    for (const qint64 id : folderIds) {
        numbers.append(QString::number(id));
    }

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT handle FROM files"
            " WHERE folder_id IN (") + numbers.join(QLatin1Char(','))
            + QStringLiteral(")"
            "   AND missing = 0"
            "   AND probe_attempted IS NOT NULL"
            "   AND (duration_seconds IS NULL OR duration_seconds <= 0)"))) {
        MM_LOG_E() << "could not list files that failed to probe"
                   << query.lastError().text();
        return handles;
    }

    while (query.next()) {
        handles.append(query.value(0).toString());
    }
    return handles;
}

QList<LibraryFile> FileRepository::all() const
{
    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.missing = 0"));
}

QList<LibraryFile> FileRepository::inFolder(qint64 folderId) const
{
    QVariantMap bindings;
    bindings.insert(QStringLiteral(":folder_id"), folderId);
    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.folder_id = :folder_id"
                         " ORDER BY f.display_name COLLATE NOCASE"),
        bindings);
}

QList<LibraryFile> FileRepository::withParentHandle(const QString &parentHandle) const
{
    QVariantMap bindings;
    bindings.insert(QStringLiteral(":parent_handle"), parentHandle);
    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.parent_handle = :parent_handle"
                         "   AND f.missing = 0"
                         " ORDER BY f.display_name COLLATE NOCASE"),
        bindings);
}

QList<LibraryFile> FileRepository::copiesOfFilm(const QString &handle) const
{
    QVariantMap bindings;
    bindings.insert(QStringLiteral(":handle"), handle);
    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.missing = 0"
                         "   AND fm.suggested = 0"
                         "   AND m.kind = 'movie'"
                         "   AND fm.media_id = ("
                         "     SELECT fm2.media_id FROM files f2"
                         "     JOIN file_media fm2 ON fm2.file_id = f2.id"
                         "     WHERE f2.handle = :handle AND fm2.suggested = 0)"
                         " ORDER BY f.height DESC, f.size_bytes DESC,"
                         "          f.display_name COLLATE NOCASE"),
        bindings);
}

QList<LibraryFile> FileRepository::continueWatching(int limit) const
{
    QVariantMap bindings;
    bindings.insert(QStringLiteral(":limit"), limit);
    bindings.insert(QStringLiteral(":after"),
                    PlaybackStateRepository::resumeAfterSeconds());
    bindings.insert(QStringLiteral(":finished"),
                    PlaybackStateRepository::finishedThresholdFraction());
    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.missing = 0"
                         "   AND p.last_played IS NOT NULL"
                         "   AND p.watched = 0"
                         "   AND p.position_seconds > :after"
                         "   AND (p.duration_seconds <= 0"
                         "        OR p.position_seconds / p.duration_seconds < :finished)"
                         " ORDER BY p.last_played DESC"
                         " LIMIT :limit"),
        bindings);
}

QList<LibraryFile> FileRepository::search(const QString &query, int limit) const
{
    const QString needle = TextFold::key(query.trimmed());
    if (needle.isEmpty() || limit <= 0) {
        return QList<LibraryFile>();
    }

    QVariantMap bindings;
    bindings.insert(QStringLiteral(":in_name"), needle);
    bindings.insert(QStringLiteral(":in_title"), needle);
    bindings.insert(QStringLiteral(":in_episode"), needle);
    bindings.insert(QStringLiteral(":limit"), limit);

    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(
            " WHERE f.missing = 0"
            "   AND (instr(COALESCE(f.name_key, ''), :in_name) > 0"
            "        OR instr(COALESCE(m.title_key, ''), :in_title) > 0"
            "        OR instr(COALESCE(e.title_key, ''), :in_episode) > 0)"
            " ORDER BY CASE WHEN COALESCE(m.title, '') <> ''"
            "               THEN m.title_key ELSE f.name_key END,"
            "          COALESCE(e.season, 0), COALESCE(e.episode, 0), f.id"
            " LIMIT :limit"),
        bindings);
}

QList<LibraryFile> FileRepository::searchUnmatched(const QString &query,
                                                   int limit) const
{
    const QString needle = TextFold::key(query.trimmed());
    if (needle.isEmpty() || limit <= 0) {
        return QList<LibraryFile>();
    }

    QVariantMap bindings;
    bindings.insert(QStringLiteral(":in_name"), needle);
    bindings.insert(QStringLiteral(":limit"), limit);

    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(
            " WHERE f.missing = 0"
            "   AND fm.media_id IS NULL"
            "   AND instr(COALESCE(f.name_key, ''), :in_name) > 0"
            " ORDER BY f.name_key, f.id"
            " LIMIT :limit"),
        bindings);
}

QList<LibraryFile> FileRepository::searchEpisodes(const QString &query,
                                                  int limit) const
{
    const QString needle = TextFold::key(query.trimmed());
    if (needle.isEmpty() || limit <= 0) {
        return QList<LibraryFile>();
    }

    QVariantMap bindings;
    bindings.insert(QStringLiteral(":in_episode"), needle);
    bindings.insert(QStringLiteral(":in_name"), needle);
    bindings.insert(QStringLiteral(":in_show"), needle);
    bindings.insert(QStringLiteral(":limit"), limit);

    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(
            " WHERE f.missing = 0"
            "   AND e.id IS NOT NULL"
            "   AND (instr(COALESCE(e.title_key, ''), :in_episode) > 0"
            "        OR instr(COALESCE(f.name_key, ''), :in_name) > 0)"
            "   AND instr(COALESCE(m.title_key, ''), :in_show) = 0"
            " ORDER BY COALESCE(m.title_key, ''), e.season, e.episode"
            " LIMIT :limit"),
        bindings);
}

LibraryFile FileRepository::byHandle(const QString &handle) const
{
    QVariantMap bindings;
    bindings.insert(QStringLiteral(":handle"), handle);
    const QList<LibraryFile> files = runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.handle = :handle"),
        bindings);
    return files.isEmpty() ? LibraryFile() : files.first();
}

LibraryFile FileRepository::byId(qint64 id) const
{
    QVariantMap bindings;
    bindings.insert(QStringLiteral(":id"), id);
    const QList<LibraryFile> files = runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.id = :id"),
        bindings);
    return files.isEmpty() ? LibraryFile() : files.first();
}

QList<LibraryFile> FileRepository::byIds(const QList<qint64> &ids) const
{
    constexpr qsizetype kChunk = 500;

    QList<LibraryFile> files;
    for (qsizetype start = 0; start < ids.size(); start += kChunk) {
        const qsizetype end = qMin(ids.size(), start + kChunk);
        QStringList numbers;
        numbers.reserve(end - start);
        for (qsizetype i = start; i < end; ++i) {
            numbers.append(QString::number(ids.at(i)));
        }
        files += runSelect(m_database,
            QStringLiteral("SELECT ") + kSelectColumns + kFromClause
            + QStringLiteral(" WHERE f.id IN (")
            + numbers.join(QLatin1Char(',')) + QLatin1Char(')'));
    }
    return files;
}

bool FileRepository::updateProbeInfo(qint64 id,
                                     double durationSeconds,
                                     const QString &container,
                                     const QString &videoCodec,
                                     const QString &audioCodec,
                                     int width,
                                     int height,
                                     bool hdr,
                                     int audioTrackCount,
                                     int subtitleTrackCount)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE files SET duration_seconds = :duration, container = :container,"
        "                 video_codec = :video_codec, audio_codec = :audio_codec,"
        "                 width = :width, height = :height, hdr = :hdr,"
        "                 audio_tracks = :audio_tracks,"
        "                 subtitle_tracks = :subtitle_tracks "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":duration"), durationSeconds);
    query.bindValue(QStringLiteral(":container"), container);
    query.bindValue(QStringLiteral(":video_codec"), videoCodec);
    query.bindValue(QStringLiteral(":audio_codec"), audioCodec);
    query.bindValue(QStringLiteral(":width"), width);
    query.bindValue(QStringLiteral(":height"), height);
    query.bindValue(QStringLiteral(":hdr"), hdr ? 1 : 0);
    query.bindValue(QStringLiteral(":audio_tracks"), audioTrackCount);
    query.bindValue(QStringLiteral(":subtitle_tracks"), subtitleTrackCount);
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        MM_LOG_E() << "could not update probe info for file" << id
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool FileRepository::markMatchAttempted(qint64 id)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE files SET match_attempted = :now WHERE id = :id"));
    query.bindValue(QStringLiteral(":now"),
                    QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        MM_LOG_E() << "could not mark file" << id << "as asked about"
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool FileRepository::clearMatchAttempts()
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "UPDATE files SET match_attempted = NULL"))) {
        MM_LOG_E() << "could not clear the match attempt markers"
                   << query.lastError().text();
        return false;
    }
    MM_LOG_I() << "match attempt markers cleared, everything is askable again";
    return true;
}

QList<LibraryFile> FileRepository::neverAsked() const
{
    return runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.missing = 0"
                         "   AND f.match_attempted IS NULL"
                         " ORDER BY f.id"));
}

QList<LibraryFile> FileRepository::neverProbed() const
{
    return runPlainSelect(m_database,
        QStringLiteral("SELECT f.id, f.folder_id, f.handle, f.parent_handle,"
                       "       f.display_name"
                       " FROM files f"
                       " WHERE f.missing = 0"
                       "   AND f.probe_attempted IS NULL"
                       "   AND (f.duration_seconds IS NULL"
                       "        OR f.duration_seconds <= 0)"
                       " ORDER BY f.id"));
}

QList<LibraryFile> FileRepository::unmatched() const
{
    return runPlainSelect(m_database,
        QStringLiteral("SELECT f.id, f.folder_id, f.handle, f.parent_handle,"
                       "       f.display_name"
                       " FROM files f"
                       " LEFT JOIN file_media fm ON fm.file_id = f.id"
                       " WHERE f.missing = 0"
                       "   AND fm.file_id IS NULL"
                       " ORDER BY f.handle COLLATE NOCASE"));
}

int FileRepository::unmatchedCount() const
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM files f"
            " LEFT JOIN file_media fm ON fm.file_id = f.id"
            " WHERE f.missing = 0 AND fm.media_id IS NULL"))
        || !query.next()) {
        MM_LOG_E() << "could not count unmatched files"
                   << query.lastError().text();
        return 0;
    }
    return query.value(0).toInt();
}

QList<RemovedFileRecord> FileRepository::removeFromLibrary(
    const QList<LibraryFile> &files, const QHash<QString, QString> &folderPaths)
{
    QList<RemovedFileRecord> removed;
    if (files.isEmpty() || !m_database.transaction()) {
        return removed;
    }

    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

    QSqlQuery remember(m_database.handle());
    remember.prepare(QStringLiteral(
        "INSERT INTO removed_files (file_handle, folder_id, display_name, folder, removed_at) "
        "VALUES (:handle, :folder_id, :name, :folder, :now) "
        "ON CONFLICT(file_handle) DO UPDATE SET folder_id = excluded.folder_id, "
        "display_name = excluded.display_name, folder = excluded.folder, "
        "removed_at = excluded.removed_at"));

    QSqlQuery forget(m_database.handle());
    forget.prepare(QStringLiteral("DELETE FROM files WHERE id = :id"));

    for (const LibraryFile &file : files) {
        if (file.id < 0 || file.folderId < 0) {
            continue;
        }

        RemovedFileRecord record;
        record.handle = file.handle;
        record.folderId = file.folderId;
        record.displayName = file.displayName;
        record.folder = folderPaths.value(file.parentHandle, file.parentHandle);
        record.removedAt = now;

        remember.bindValue(QStringLiteral(":handle"), record.handle);
        remember.bindValue(QStringLiteral(":folder_id"), record.folderId);
        remember.bindValue(QStringLiteral(":name"), record.displayName);
        remember.bindValue(QStringLiteral(":folder"), record.folder);
        remember.bindValue(QStringLiteral(":now"), now);
        forget.bindValue(QStringLiteral(":id"), file.id);

        if (!remember.exec() || !forget.exec()) {
            MM_LOG_E() << "could not take" << file.handle << "out of the library"
                       << remember.lastError().text() << forget.lastError().text();
            m_database.rollback();
            return QList<RemovedFileRecord>();
        }
        removed.append(record);
    }

    if (!m_database.commit()) {
        m_database.rollback();
        return QList<RemovedFileRecord>();
    }

    MM_LOG_I() << "took" << removed.size() << "files out of the library;"
               << "scans will pass over them";
    return removed;
}

QList<RemovedFileRecord> FileRepository::removedFiles() const
{
    QList<RemovedFileRecord> result;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT file_handle, folder_id, display_name, folder, removed_at "
            "FROM removed_files "
            "ORDER BY removed_at DESC, display_name COLLATE NOCASE"))) {
        MM_LOG_W() << "could not list the files taken out of the library"
                   << query.lastError().text();
        return result;
    }
    while (query.next()) {
        RemovedFileRecord record;
        record.handle = query.value(0).toString();
        record.folderId = query.value(1).toLongLong();
        record.displayName = query.value(2).toString();
        record.folder = query.value(3).toString();
        record.removedAt = query.value(4).toLongLong();
        result.append(record);
    }
    return result;
}

QList<qint64> FileRepository::restoreRemoved(const QStringList &handles)
{
    QList<qint64> folderIds;
    if (handles.isEmpty() || !m_database.transaction()) {
        return folderIds;
    }
    int restored = 0;

    QSqlQuery folderOf(m_database.handle());
    folderOf.prepare(QStringLiteral(
        "SELECT folder_id FROM removed_files WHERE file_handle = :handle"));

    QSqlQuery forget(m_database.handle());
    forget.prepare(QStringLiteral(
        "DELETE FROM removed_files WHERE file_handle = :handle"));

    for (const QString &handle : handles) {
        folderOf.bindValue(QStringLiteral(":handle"), handle);
        if (!folderOf.exec()) {
            MM_LOG_E() << "could not read where" << handle << "was"
                       << folderOf.lastError().text();
            m_database.rollback();
            return QList<qint64>();
        }
        if (!folderOf.next()) {
            continue;
        }
        const qint64 folderId = folderOf.value(0).toLongLong();
        folderOf.finish();

        forget.bindValue(QStringLiteral(":handle"), handle);
        if (!forget.exec()) {
            MM_LOG_E() << "could not put" << handle << "back"
                       << forget.lastError().text();
            m_database.rollback();
            return QList<qint64>();
        }
        ++restored;
        if (!folderIds.contains(folderId)) {
            folderIds.append(folderId);
        }
    }

    if (!m_database.commit()) {
        m_database.rollback();
        return QList<qint64>();
    }

    MM_LOG_I() << "put" << restored << "files back; the next scan of"
               << folderIds.size() << "folders brings them in";
    return folderIds;
}

bool FileRepository::markProbeAttempted(qint64 id)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE files SET probe_attempted = :at WHERE id = :id"));
    query.bindValue(QStringLiteral(":at"),
                    QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        MM_LOG_E() << "could not record a probe attempt"
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool FileRepository::clearProbeAttempts()
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "UPDATE files SET probe_attempted = NULL"))) {
        MM_LOG_E() << "could not clear the probe attempt markers"
                   << query.lastError().text();
        return false;
    }
    MM_LOG_I() << "probe attempt markers cleared, everything is probeable again";
    return true;
}

QStringList FileRepository::handlesWithPoster() const
{
    QStringList handles;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT f.handle FROM files f "
            "JOIN file_media fm ON fm.file_id = f.id "
            "JOIN media m ON m.id = fm.media_id "
            "WHERE m.poster_path IS NOT NULL AND m.poster_path <> ''"))) {
        MM_LOG_E() << "could not list files with poster art"
                   << query.lastError().text();
        return handles;
    }

    while (query.next()) {
        handles.append(query.value(0).toString());
    }
    return handles;
}

int FileRepository::count() const
{
    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM files WHERE missing = 0"))
        || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

int FileRepository::continueWatchingCount() const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM files f"
        " JOIN playback_state p ON p.file_id = f.id"
        " WHERE f.missing = 0"
        "   AND p.last_played IS NOT NULL"
        "   AND p.watched = 0"
        "   AND p.position_seconds > :after"
        "   AND (p.duration_seconds <= 0"
        "        OR p.position_seconds / p.duration_seconds < :finished)"));
    query.bindValue(QStringLiteral(":after"),
                    PlaybackStateRepository::resumeAfterSeconds());
    query.bindValue(QStringLiteral(":finished"),
                    PlaybackStateRepository::finishedThresholdFraction());
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

FolderSummary FileRepository::summaryUnder(const QString &prefix) const
{
    FolderSummary summary;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT COUNT(*), COALESCE(SUM(size_bytes), 0) FROM files"
        " WHERE missing = 0 AND substr(handle, 1, :len) = :prefix"));
    query.bindValue(QStringLiteral(":len"), prefix.length());
    query.bindValue(QStringLiteral(":prefix"), prefix);

    if (!query.exec() || !query.next()) {
        MM_LOG_W() << "folder summary query failed for" << prefix
                   << query.lastError().text();
        return summary;
    }

    summary.fileCount = query.value(0).toInt();
    summary.totalBytes = query.value(1).toLongLong();
    return summary;
}

QHash<QString, FolderSummary> FileRepository::summariesBelow(
    const QString &folderHandle) const
{
    QHash<QString, FolderSummary> summaries;
    if (folderHandle.isEmpty()) {
        return summaries;
    }

    const QString prefix = folderHandle.endsWith(QLatin1Char('/'))
        ? folderHandle
        : folderHandle + QLatin1Char('/');
    const QString pastPrefix = prefix.chopped(1) + QLatin1Char('0');

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT parent_handle, COUNT(*), COALESCE(SUM(size_bytes), 0) FROM files"
        " WHERE missing = 0"
        "   AND parent_handle >= :prefix AND parent_handle < :past"
        " GROUP BY parent_handle"));
    query.bindValue(QStringLiteral(":prefix"), prefix);
    query.bindValue(QStringLiteral(":past"), pastPrefix);

    if (!query.exec()) {
        MM_LOG_W() << "folder summaries failed for" << folderHandle
                   << query.lastError().text();
        return summaries;
    }

    while (query.next()) {
        const QString parent = query.value(0).toString();
        if (parent.size() <= prefix.size()) {
            continue;
        }

        const qsizetype end = parent.indexOf(QLatin1Char('/'), prefix.size());
        const QString child = end < 0 ? parent : parent.left(end);

        FolderSummary &summary = summaries[child];
        summary.fileCount += query.value(1).toInt();
        summary.totalBytes += query.value(2).toLongLong();
    }

    return summaries;
}

QHash<QString, LibraryFile> FileRepository::filesIn(const QString &parentHandle) const
{
    QHash<QString, LibraryFile> files;

    QVariantMap bindings;
    bindings.insert(QStringLiteral(":parent_handle"), parentHandle);
    const QList<LibraryFile> rows = runSelect(m_database,
        QStringLiteral("SELECT ") + kSelectColumns + kFromClause
        + QStringLiteral(" WHERE f.parent_handle = :parent_handle"),
        bindings);

    files.reserve(rows.size());
    for (const LibraryFile &file : rows) {
        files.insert(file.handle, file);
    }
    return files;
}
