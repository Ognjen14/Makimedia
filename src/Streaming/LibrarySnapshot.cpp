#include "Streaming/LibrarySnapshot.h"

#include "Data/Database.h"
#include "MmLog.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

bool run(QSqlQuery &query, const QString &statement, QString *error)
{
    if (query.exec(statement)) {
        return true;
    }
    *error = statement.left(80) + QStringLiteral(": ") + query.lastError().text();
    return false;
}

void removeWithSidecars(const QString &path)
{
    QFile::remove(path);
    QFile::remove(path + QStringLiteral("-wal"));
    QFile::remove(path + QStringLiteral("-shm"));
    QFile::remove(path + QStringLiteral("-journal"));
}

int countOf(QSqlQuery &query, const QString &table)
{
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM ") + table) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

}

namespace LibrarySnapshot {

QString serverFolderHandle(const QString &serverId)
{
    return QStringLiteral("makimedia-server:") + serverId;
}

QString revisionOf(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file)) {
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex());
}

LibrarySnapshotResult build(const QString &sourcePath, const QString &outputPath,
                            const QString &serverId, const QString &serverName)
{
    LibrarySnapshotResult result;
    result.path = outputPath;

    QElapsedTimer timer;
    timer.start();

    removeWithSidecars(outputPath);

    Database snapshot;
    if (!snapshot.open(outputPath)) {
        result.error = QStringLiteral("could not create the snapshot file");
        return result;
    }

    {
        QSqlQuery query(snapshot.handle());
        QString error;

        QSqlQuery attach(snapshot.handle());
        attach.prepare(QStringLiteral("ATTACH DATABASE :path AS src"));
        attach.bindValue(QStringLiteral(":path"), sourcePath);
        if (!attach.exec()) {
            result.error = QStringLiteral("could not read the library: ")
                           + attach.lastError().text();
            snapshot.close();
            removeWithSidecars(outputPath);
            return result;
        }

        QSqlQuery folder(snapshot.handle());
        folder.prepare(QStringLiteral(
            "INSERT INTO folders (id, handle, display_name, last_scanned, available, added) "
            "VALUES (:id, :handle, :name, 0, 1, 0)"));
        folder.bindValue(QStringLiteral(":id"), ServerFolderId);
        folder.bindValue(QStringLiteral(":handle"), serverFolderHandle(serverId));
        folder.bindValue(QStringLiteral(":name"), serverName);

        const QStringList statements = {
            QStringLiteral(
                "CREATE TEMP TABLE exported_files AS"
                " SELECT f.id AS id, fm.media_id AS media_id FROM src.files f"
                " JOIN src.file_media fm ON fm.file_id = f.id"
                " JOIN src.media m ON m.id = fm.media_id"
                " WHERE f.missing = 0 AND fm.suggested = 0"),
            QStringLiteral(
                "CREATE TEMP TABLE exported_media AS"
                " SELECT DISTINCT media_id AS id FROM exported_files"),

            QStringLiteral(
                "INSERT INTO media SELECT * FROM src.media"
                " WHERE id IN (SELECT id FROM exported_media) ORDER BY id"),
            QStringLiteral(
                "INSERT INTO files (id, folder_id, handle, parent_handle, display_name,"
                "  size_bytes, modified, duration_seconds, container, video_codec,"
                "  audio_codec, width, height, hdr, added, missing, audio_tracks,"
                "  subtitle_tracks, match_attempted, probe_attempted, name_key)"
                " SELECT f.id, 1, f.handle, f.parent_handle, f.display_name,"
                "  f.size_bytes, f.modified, f.duration_seconds, f.container, f.video_codec,"
                "  f.audio_codec, f.width, f.height, f.hdr, f.added, 0, f.audio_tracks,"
                "  f.subtitle_tracks, f.match_attempted, f.probe_attempted, f.name_key"
                " FROM src.files f WHERE f.id IN (SELECT id FROM exported_files)"
                " ORDER BY f.id"),
            QStringLiteral(
                "INSERT INTO file_media (file_id, media_id, confidence, suggested, matched_at)"
                " SELECT file_id, media_id, confidence, suggested, matched_at"
                " FROM src.file_media WHERE file_id IN (SELECT id FROM exported_files)"
                " ORDER BY file_id"),
            QStringLiteral(
                "INSERT INTO episodes (id, media_id, file_id, season, episode, title,"
                "  overview, still_path, air_date, runtime_minutes, title_key)"
                " SELECT e.id, e.media_id,"
                "  CASE WHEN e.file_id IN (SELECT id FROM exported_files)"
                "       THEN e.file_id ELSE NULL END,"
                "  e.season, e.episode, e.title, e.overview, e.still_path, e.air_date,"
                "  e.runtime_minutes, e.title_key"
                " FROM src.episodes e WHERE e.media_id IN (SELECT id FROM exported_media)"
                " ORDER BY e.id"),
            QStringLiteral(
                "INSERT INTO fetched_seasons (media_id, season, fetched_at,"
                "  poster_path, credits_fetched)"
                " SELECT media_id, season, fetched_at, poster_path, credits_fetched"
                " FROM src.fetched_seasons"
                " WHERE media_id IN (SELECT id FROM exported_media)"
                " ORDER BY media_id, season"),
            QStringLiteral(
                "INSERT INTO credits (media_id, episode_id, kind, name, role,"
                "  profile_path, sort)"
                " SELECT media_id, episode_id, kind, name, role, profile_path, sort"
                " FROM src.credits WHERE media_id IN (SELECT id FROM exported_media)"
                " ORDER BY media_id, episode_id, kind, sort"),
            QStringLiteral(
                "INSERT INTO collections (tmdb_id, name, overview, poster_path,"
                "  backdrop_path, fetched_at)"
                " SELECT tmdb_id, name, overview, poster_path, backdrop_path, fetched_at"
                " FROM src.collections WHERE tmdb_id IN"
                "  (SELECT collection_id FROM main.media WHERE collection_id IS NOT NULL)"
                " ORDER BY tmdb_id"),
            QStringLiteral(
                "INSERT INTO collection_parts (collection_id, tmdb_id, position, title,"
                "  release_date, poster_path, backdrop_path)"
                " SELECT collection_id, tmdb_id, position, title, release_date,"
                "  poster_path, backdrop_path FROM src.collection_parts"
                " WHERE collection_id IN (SELECT tmdb_id FROM main.collections)"
                " ORDER BY collection_id, tmdb_id"),
            QStringLiteral(
                "INSERT INTO film_details (tmdb_id, title, release_date, poster_path,"
                "  backdrop_path, runtime_minutes, rating, fetched_at)"
                " SELECT tmdb_id, title, release_date, poster_path, backdrop_path,"
                "  runtime_minutes, rating, fetched_at FROM src.film_details"
                " WHERE tmdb_id IN (SELECT tmdb_id FROM main.collection_parts)"
                " ORDER BY tmdb_id"),
            QStringLiteral(
                "INSERT INTO custom_collections (id, name, description, cover_mode, created_at)"
                " SELECT id, name, description, cover_mode, created_at"
                " FROM src.custom_collections ORDER BY id"),
            QStringLiteral(
                "INSERT INTO custom_collection_items (collection_id, media_id, position)"
                " SELECT collection_id, media_id, position FROM src.custom_collection_items"
                " WHERE media_id IN (SELECT id FROM exported_media)"
                " ORDER BY collection_id, media_id"),
            QStringLiteral(
                "INSERT INTO hidden_collections (collection_id, name, hidden_at)"
                " SELECT collection_id, name, hidden_at FROM src.hidden_collections"
                " ORDER BY collection_id"),
            QStringLiteral(
                "INSERT INTO hidden_collection_films"
                " (scope, tmdb_id, title, collection_name, hidden_at)"
                " SELECT scope, tmdb_id, title, collection_name, hidden_at"
                " FROM src.hidden_collection_films ORDER BY scope, tmdb_id"),
            QStringLiteral(
                "INSERT INTO added_collection_films"
                " (scope, tmdb_id, title, collection_name, added_at)"
                " SELECT scope, tmdb_id, title, collection_name, added_at"
                " FROM src.added_collection_films ORDER BY scope, tmdb_id"),

            QStringLiteral(
                "UPDATE main.media SET"
                "  updated = CASE WHEN updated IS NULL THEN NULL ELSE 1 END,"
                "  collection_checked = CASE WHEN collection_checked IS NULL THEN NULL ELSE 1 END"),
            QStringLiteral(
                "UPDATE main.files SET"
                "  match_attempted = CASE WHEN match_attempted IS NULL THEN NULL ELSE 1 END,"
                "  probe_attempted = CASE WHEN probe_attempted IS NULL THEN NULL ELSE 1 END"),
            QStringLiteral(
                "UPDATE main.file_media SET"
                "  matched_at = CASE WHEN matched_at IS NULL THEN NULL ELSE 1 END"),
            QStringLiteral(
                "UPDATE main.collections SET"
                "  fetched_at = CASE WHEN fetched_at IS NULL THEN NULL ELSE 1 END"),
            QStringLiteral("UPDATE main.film_details SET fetched_at = 1"),
            QStringLiteral("UPDATE main.fetched_seasons SET fetched_at = 1")
        };

        bool ok = snapshot.transaction();
        if (ok && !folder.exec()) {
            error = QStringLiteral("the server folder: ") + folder.lastError().text();
            ok = false;
        }
        for (const QString &statement : statements) {
            if (!ok || !run(query, statement, &error)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            ok = snapshot.commit();
        } else {
            snapshot.rollback();
        }

        if (ok) {
            result.titles = countOf(query, QStringLiteral("main.media"));
            result.files = countOf(query, QStringLiteral("main.files"));
            if (query.exec(QStringLiteral("SELECT id, handle FROM main.files ORDER BY id"))) {
                while (query.next()) {
                    result.servedFiles.insert(query.value(0).toLongLong(),
                                              query.value(1).toString());
                }
            }
            if (query.exec(QStringLiteral(
                    "SELECT f.id, s.sub_handle, s.display_name FROM src.external_subtitles s"
                    " JOIN main.files f ON f.handle = s.file_handle ORDER BY f.id, s.id"))) {
                while (query.next()) {
                    result.attachedSubtitles[query.value(0).toLongLong()].append(
                        { query.value(1).toString(), query.value(2).toString() });
                }
            }
        }

        query.finish();
        const bool detached = run(query, QStringLiteral("DETACH DATABASE src"), &error);
        const bool tidy = ok && detached
                          && run(query, QStringLiteral("DROP TABLE temp.exported_files"), &error)
                          && run(query, QStringLiteral("DROP TABLE temp.exported_media"), &error)
                          && run(query, QStringLiteral("PRAGMA journal_mode = DELETE"), &error)
                          && run(query, QStringLiteral("VACUUM"), &error);
        if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next()) {
            result.schemaVersion = query.value(0).toInt();
        }
        query.finish();

        if (!ok || !tidy) {
            result.error = error;
        }
    }

    snapshot.close();

    if (!result.error.isEmpty()) {
        MM_LOG_E() << "snapshot: could not be built -" << result.error;
        removeWithSidecars(outputPath);
        result.servedFiles.clear();
        result.attachedSubtitles.clear();
        return result;
    }

    result.bytes = QFile(outputPath).size();
    result.revision = revisionOf(outputPath);
    result.ok = !result.revision.isEmpty();
    MM_LOG_I() << "snapshot: built with" << result.titles << "titles and" << result.files
               << "files," << result.bytes << "bytes, revision" << result.revision.left(12)
               << "in" << timer.elapsed() << "ms";
    return result;
}

}
