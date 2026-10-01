#include "Data/Database.h"

#include "MmLog.h"
#include "TextFold.h"

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

#include <utility>

namespace {

const int kTargetSchemaVersion = 22;

}

Database::Database()
    : m_connectionName(QStringLiteral("makimedia-%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

Database::~Database()
{
    close();
}

int Database::targetSchemaVersion()
{
    return kTargetSchemaVersion;
}

QString Database::defaultFilePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/library.sqlite");
}

QString Database::filePath() const
{
    return m_filePath;
}

bool Database::isOpen() const
{
    return m_open;
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

bool Database::open(const QString &filePath)
{
    if (m_open) {
        return true;
    }

    m_filePath = filePath.isEmpty() ? defaultFilePath() : filePath;

    const QString parent = QFileInfo(m_filePath).absolutePath();
    if (!QDir().mkpath(parent)) {
        MM_LOG_E() << "could not create the database directory" << parent;
        return false;
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    m_connectionName);
        db.setDatabaseName(m_filePath);

        if (db.open()) {
            m_open = true;
        } else {
            MM_LOG_E() << "could not open the library database" << m_filePath
                       << db.lastError().text();
        }
    }

    if (!m_open) {
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    MM_LOG_I() << "library database opened" << m_filePath;

    if (!applyPragmas()) {
        close();
        return false;
    }

    if (!migrate()) {
        close();
        return false;
    }

    return true;
}

void Database::close()
{
    if (!m_open) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }

    QSqlDatabase::removeDatabase(m_connectionName);
    m_open = false;
    MM_LOG_I() << "library database closed";
}

bool Database::execOrLog(const QString &statement)
{
    QSqlQuery query(handle());
    if (!query.exec(statement)) {
        MM_LOG_E() << "sql failed:" << statement.left(120)
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool Database::applyPragmas()
{
    if (!execOrLog(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        return false;
    }
    if (!execOrLog(QStringLiteral("PRAGMA synchronous = NORMAL"))) {
        return false;
    }
    if (!execOrLog(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        return false;
    }
    return true;
}

int Database::schemaVersion() const
{
    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        MM_LOG_E() << "could not read the schema version"
                   << query.lastError().text();
        return -1;
    }
    return query.value(0).toInt();
}

bool Database::setSchemaVersion(int version)
{
    return execOrLog(QStringLiteral("PRAGMA user_version = %1").arg(version));
}

bool Database::migrate()
{
    const int current = schemaVersion();
    if (current < 0) {
        return false;
    }

    if (current == kTargetSchemaVersion) {
        MM_LOG_I() << "database schema is current at version" << current;
        return true;
    }

    if (current > kTargetSchemaVersion) {
        MM_LOG_E() << "database schema version" << current
                   << "is newer than this build understands"
                   << kTargetSchemaVersion;
        return false;
    }

    MM_LOG_I() << "migrating database schema from" << current
               << "to" << kTargetSchemaVersion;

    if (current < 1) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion1() || !setSchemaVersion(1)) {
            rollback();
            MM_LOG_E() << "migration to version 1 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 1";
    }

    if (current < 2) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion2() || !setSchemaVersion(2)) {
            rollback();
            MM_LOG_E() << "migration to version 2 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 2";
    }

    if (current < 3) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion3() || !setSchemaVersion(3)) {
            rollback();
            MM_LOG_E() << "migration to version 3 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 3";
    }

    if (current < 4) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion4() || !setSchemaVersion(4)) {
            rollback();
            MM_LOG_E() << "migration to version 4 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 4";
    }

    if (current < 5) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion5() || !setSchemaVersion(5)) {
            rollback();
            MM_LOG_E() << "migration to version 5 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 5";
    }

    if (current < 6) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion6() || !setSchemaVersion(6)) {
            rollback();
            MM_LOG_E() << "migration to version 6 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 6";
    }

    if (current < 7) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion7() || !setSchemaVersion(7)) {
            rollback();
            MM_LOG_E() << "migration to version 7 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 7";
    }

    if (current < 8) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion8() || !setSchemaVersion(8)) {
            rollback();
            MM_LOG_E() << "migration to version 8 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 8";
    }

    if (current < 9) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion9() || !setSchemaVersion(9)) {
            rollback();
            MM_LOG_E() << "migration to version 9 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 9";
    }

    if (current < 10) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion10() || !setSchemaVersion(10)) {
            rollback();
            MM_LOG_E() << "migration to version 10 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 10";
    }

    if (current < 11) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion11() || !setSchemaVersion(11)) {
            rollback();
            MM_LOG_E() << "migration to version 11 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 11";
    }

    if (current < 12) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion12() || !setSchemaVersion(12)) {
            rollback();
            MM_LOG_E() << "migration to version 12 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 12";
    }

    if (current < 13) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion13() || !setSchemaVersion(13)) {
            rollback();
            MM_LOG_E() << "migration to version 13 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 13";
    }

    if (current < 14) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion14() || !setSchemaVersion(14)) {
            rollback();
            MM_LOG_E() << "migration to version 14 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 14";
    }

    if (current < 15) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion15() || !setSchemaVersion(15)) {
            rollback();
            MM_LOG_E() << "migration to version 15 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 15";
    }

    if (current < 16) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion16() || !setSchemaVersion(16)) {
            rollback();
            MM_LOG_E() << "migration to version 16 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 16";
    }

    if (current < 17) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion17() || !setSchemaVersion(17)) {
            rollback();
            MM_LOG_E() << "migration to version 17 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 17";
    }

    if (current < 18) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion18() || !setSchemaVersion(18)) {
            rollback();
            MM_LOG_E() << "migration to version 18 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 18";
    }

    if (current < 19) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion19() || !setSchemaVersion(19)) {
            rollback();
            MM_LOG_E() << "migration to version 19 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 19";
    }

    if (current < 20) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion20() || !setSchemaVersion(20)) {
            rollback();
            MM_LOG_E() << "migration to version 20 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 20";
    }

    if (current < 21) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion21() || !setSchemaVersion(21)) {
            rollback();
            MM_LOG_E() << "migration to version 21 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 21";
    }

    if (current < 22) {
        if (!transaction()) {
            return false;
        }
        if (!migrateToVersion22() || !setSchemaVersion(22)) {
            rollback();
            MM_LOG_E() << "migration to version 22 failed, rolled back";
            return false;
        }
        if (!commit()) {
            return false;
        }
        MM_LOG_I() << "migrated database schema to version 22";
    }

    return true;
}

bool Database::migrateToVersion22()
{
    if (!execOrLog(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS added_collection_films ("
            "  scope TEXT NOT NULL,"
            "  tmdb_id INTEGER NOT NULL,"
            "  title TEXT,"
            "  collection_name TEXT,"
            "  added_at INTEGER NOT NULL,"
            "  PRIMARY KEY (scope, tmdb_id))"))) {
        return false;
    }
    MM_LOG_I() << "films put into a collection by hand are remembered from here on";
    return true;
}

bool Database::migrateToVersion21()
{
    if (!execOrLog(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS hidden_collection_films ("
            "  scope TEXT NOT NULL,"
            "  tmdb_id INTEGER NOT NULL,"
            "  title TEXT,"
            "  collection_name TEXT,"
            "  hidden_at INTEGER NOT NULL,"
            "  PRIMARY KEY (scope, tmdb_id))"))) {
        return false;
    }
    MM_LOG_I() << "films taken out of a collection are remembered from here on";
    return true;
}

bool Database::migrateToVersion20()
{
    if (!hasColumn(QStringLiteral("external_subtitles"), QStringLiteral("origin"))
        && !execOrLog(QStringLiteral(
               "ALTER TABLE external_subtitles ADD COLUMN origin TEXT NOT NULL "
               "DEFAULT 'manual'"))) {
        return false;
    }
    MM_LOG_I() << "a subtitle now remembers where it came from";
    return true;
}

bool Database::migrateToVersion19()
{
    QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS fetched_credits ("
            "  media_id INTEGER PRIMARY KEY REFERENCES media(id) ON DELETE CASCADE,"
            "  fetched_at INTEGER NOT NULL)")
    };

    if (!hasColumn(QStringLiteral("fetched_seasons"),
                   QStringLiteral("credits_fetched"))) {
        statements.append(QStringLiteral(
            "ALTER TABLE fetched_seasons ADD COLUMN credits_fetched"
            " INTEGER NOT NULL DEFAULT 0"));
    }

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }
    MM_LOG_I() << "asking who made a title is remembered from here on";
    return true;
}

bool Database::migrateToVersion18()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS credits ("
            "  media_id INTEGER NOT NULL REFERENCES media(id) ON DELETE CASCADE,"
            "  episode_id INTEGER NOT NULL DEFAULT 0,"
            "  kind INTEGER NOT NULL,"
            "  name TEXT NOT NULL,"
            "  role TEXT,"
            "  profile_path TEXT,"
            "  sort INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_credits_media "
            "ON credits(media_id, episode_id)")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }
    MM_LOG_I() << "who made a title is remembered from here on";
    return true;
}

bool Database::migrateToVersion17()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS removed_files ("
            "  file_handle TEXT PRIMARY KEY,"
            "  folder_id INTEGER NOT NULL REFERENCES folders(id) ON DELETE CASCADE,"
            "  display_name TEXT NOT NULL,"
            "  folder TEXT,"
            "  removed_at INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_removed_files_folder "
            "ON removed_files(folder_id)")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }
    MM_LOG_I() << "files taken out of the library are remembered from here on";
    return true;
}

bool Database::migrateToVersion16()
{
    const bool created = execOrLog(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS discarded_shows ("
        "  file_handle TEXT PRIMARY KEY,"
        "  title TEXT,"
        "  folder TEXT,"
        "  discarded_at INTEGER NOT NULL)"));
    if (created) {
        MM_LOG_I() << "folders refused as shows are remembered from here on";
    }
    return created;
}

bool Database::migrateToVersion15()
{
    const bool created = execOrLog(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS hidden_collections ("
        "  collection_id INTEGER PRIMARY KEY,"
        "  name TEXT,"
        "  hidden_at INTEGER NOT NULL)"));
    if (created) {
        MM_LOG_I() << "collections taken off the page are remembered from here on";
    }
    return created;
}

bool Database::migrateToVersion14()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS custom_collections ("
            "  id INTEGER PRIMARY KEY,"
            "  name TEXT NOT NULL,"
            "  description TEXT,"
            "  cover_mode TEXT,"
            "  created_at INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS custom_collection_items ("
            "  collection_id INTEGER NOT NULL"
            "    REFERENCES custom_collections(id) ON DELETE CASCADE,"
            "  media_id INTEGER NOT NULL REFERENCES media(id) ON DELETE CASCADE,"
            "  position INTEGER NOT NULL,"
            "  PRIMARY KEY (collection_id, media_id))")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }

    MM_LOG_I() << "collections the user makes are stored from here on";
    return true;
}

bool Database::migrateToVersion13()
{
    const bool created = execOrLog(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS film_details ("
        "  tmdb_id INTEGER PRIMARY KEY,"
        "  title TEXT,"
        "  release_date TEXT,"
        "  poster_path TEXT,"
        "  backdrop_path TEXT,"
        "  runtime_minutes INTEGER,"
        "  rating REAL,"
        "  fetched_at INTEGER NOT NULL)"));
    if (created) {
        MM_LOG_I() << "films of a collection that are not in the library now keep"
                   << "their runtime, rating and poster";
    }
    return created;
}

bool Database::migrateToVersion12()
{
    QStringList statements;
    if (!hasColumn(QStringLiteral("media"), QStringLiteral("collection_id"))) {
        statements.append(QStringLiteral("ALTER TABLE media ADD COLUMN collection_id INTEGER"));
    }
    if (!hasColumn(QStringLiteral("media"), QStringLiteral("collection_checked"))) {
        statements.append(QStringLiteral("ALTER TABLE media ADD COLUMN collection_checked INTEGER"));
    }
    statements.append({
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS collections ("
            "  tmdb_id INTEGER PRIMARY KEY,"
            "  name TEXT,"
            "  overview TEXT,"
            "  poster_path TEXT,"
            "  backdrop_path TEXT,"
            "  fetched_at INTEGER)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS collection_parts ("
            "  collection_id INTEGER NOT NULL REFERENCES collections(tmdb_id) ON DELETE CASCADE,"
            "  tmdb_id INTEGER NOT NULL,"
            "  position INTEGER NOT NULL,"
            "  title TEXT,"
            "  release_date TEXT,"
            "  poster_path TEXT,"
            "  backdrop_path TEXT,"
            "  PRIMARY KEY (collection_id, tmdb_id))"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_media_collection ON media(collection_id)")
    });

    for (const QString &statement : std::as_const(statements)) {
        if (!execOrLog(statement)) {
            return false;
        }
    }

    MM_LOG_I() << "films now remember their TMDB collection; films matched before this"
               << "have their details asked for once more";
    return true;
}

bool Database::migrateToVersion11()
{
    if (hasColumn(QStringLiteral("fetched_seasons"), QStringLiteral("poster_path"))) {
        return true;
    }

    const bool added = execOrLog(QStringLiteral(
        "ALTER TABLE fetched_seasons ADD COLUMN poster_path TEXT"));
    if (added) {
        MM_LOG_I() << "seasons now keep their own poster; seasons fetched before"
                   << "this are asked for once more";
    }
    return added;
}

bool Database::migrateToVersion9()
{
    const bool created = execOrLog(QStringLiteral(
        "CREATE TRIGGER IF NOT EXISTS trg_files_forget_subtitles"
        " AFTER DELETE ON files"
        " BEGIN"
        "   DELETE FROM external_subtitles WHERE file_handle = OLD.handle;"
        " END"));

    if (created) {
        MM_LOG_I() << "subtitles now leave the library with their file";
    }
    return created;
}

bool Database::migrateToVersion10()
{
    const QList<std::pair<QString, QString>> keyColumns = {
        {QStringLiteral("files"), QStringLiteral("name_key")},
        {QStringLiteral("media"), QStringLiteral("title_key")},
        {QStringLiteral("episodes"), QStringLiteral("title_key")}
    };

    for (const auto &column : keyColumns) {
        if (hasColumn(column.first, column.second)) {
            continue;
        }
        if (!execOrLog(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 TEXT")
                           .arg(column.first, column.second))) {
            return false;
        }
    }

    const QStringList statements = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_match_attempted"
                       " ON files(match_attempted)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_probe_attempted"
                       " ON files(probe_attempted)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_file_media_suggested"
                       " ON file_media(suggested)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_media_kind ON media(kind)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_display_name"
                       " ON files(display_name COLLATE NOCASE)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_handle_nocase"
                       " ON files(handle COLLATE NOCASE)")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }

    if (!foldColumn(QStringLiteral("files"), QStringLiteral("display_name"),
                    QStringLiteral("name_key"))
        || !foldColumn(QStringLiteral("media"), QStringLiteral("title"),
                       QStringLiteral("title_key"))
        || !foldColumn(QStringLiteral("episodes"), QStringLiteral("title"),
                       QStringLiteral("title_key"))) {
        return false;
    }

    MM_LOG_I() << "indexes added for matching, probing and sorting, and names"
               << "folded for search";
    return true;
}

bool Database::hasColumn(const QString &table, const QString &column)
{
    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == column) {
            return true;
        }
    }
    return false;
}

bool Database::foldColumn(const QString &table, const QString &textColumn,
                          const QString &keyColumn)
{
    QList<std::pair<qint64, QString>> rows;
    {
        QSqlQuery read(handle());
        if (!read.exec(QStringLiteral("SELECT id, %1 FROM %2").arg(textColumn, table))) {
            MM_LOG_E() << "could not read" << table << "to fold for search"
                       << read.lastError().text();
            return false;
        }
        while (read.next()) {
            rows.append({read.value(0).toLongLong(),
                         TextFold::key(read.value(1).toString())});
        }
    }

    QSqlQuery write(handle());
    write.prepare(QStringLiteral("UPDATE %1 SET %2 = :key WHERE id = :id")
                      .arg(table, keyColumn));
    for (const auto &row : std::as_const(rows)) {
        write.bindValue(QStringLiteral(":key"), row.second);
        write.bindValue(QStringLiteral(":id"), row.first);
        if (!write.exec()) {
            MM_LOG_E() << "could not fold a row of" << table
                       << write.lastError().text();
            return false;
        }
    }

    MM_LOG_I() << "folded" << rows.size() << "rows of" << table << "for search";
    return true;
}

bool Database::migrateToVersion6()
{
    return execOrLog(
        QStringLiteral("ALTER TABLE files ADD COLUMN probe_attempted INTEGER"));
}

bool Database::migrateToVersion7()
{
#ifdef Q_OS_WIN
    const QStringList statements = {
        QStringLiteral(
            "UPDATE OR IGNORE external_subtitles"
            " SET file_handle = replace(file_handle, '\\', '/'),"
            "     sub_handle = replace(sub_handle, '\\', '/')"
            " WHERE instr(file_handle, '\\') > 0 OR instr(sub_handle, '\\') > 0"),
        QStringLiteral(
            "DELETE FROM external_subtitles"
            " WHERE instr(file_handle, '\\') > 0 OR instr(sub_handle, '\\') > 0")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }

    MM_LOG_I() << "subtitle handles rewritten with forward slashes";
#endif
    return true;
}

bool Database::migrateToVersion8()
{
    const QStringList statements = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_file_media_media"
                       " ON file_media(media_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_added"
                       " ON files(added DESC, id DESC)")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }

    MM_LOG_I() << "indexes added for the media lists and recently added";
    return true;
}

bool Database::migrateToVersion5()
{
    const QStringList statements = {
        QStringLiteral("ALTER TABLE files ADD COLUMN match_attempted INTEGER"),
        QStringLiteral(
            "CREATE TABLE fetched_seasons ("
            "  media_id INTEGER NOT NULL REFERENCES media(id) ON DELETE CASCADE,"
            "  season INTEGER NOT NULL,"
            "  fetched_at INTEGER NOT NULL,"
            "  PRIMARY KEY (media_id, season))")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }
    return true;
}

bool Database::migrateToVersion4()
{
    const QStringList statements = {
        QStringLiteral("ALTER TABLE files ADD COLUMN audio_tracks"
                       " INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("ALTER TABLE files ADD COLUMN subtitle_tracks"
                       " INTEGER NOT NULL DEFAULT 0")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }
    return true;
}

bool Database::migrateToVersion3()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE external_subtitles_new ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  file_handle TEXT NOT NULL,"
            "  sub_handle TEXT NOT NULL,"
            "  display_name TEXT NOT NULL,"
            "  language TEXT NOT NULL DEFAULT '',"
            "  added_at TEXT NOT NULL,"
            "  UNIQUE (file_handle, sub_handle)"
            ")"),
        QStringLiteral(
            "INSERT INTO external_subtitles_new "
            "  (file_handle, sub_handle, display_name, language, added_at) "
            "SELECT file_handle, sub_handle, display_name, '', added_at "
            "FROM external_subtitles"),
        QStringLiteral("DROP TABLE external_subtitles"),
        QStringLiteral(
            "ALTER TABLE external_subtitles_new RENAME TO external_subtitles"),
        QStringLiteral(
            "CREATE INDEX idx_external_subtitles_file "
            "ON external_subtitles (file_handle)")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }
    return true;
}

bool Database::migrateToVersion2()
{
    return execOrLog(QStringLiteral(
        "CREATE TABLE external_subtitles ("
        "  file_handle TEXT PRIMARY KEY,"
        "  sub_handle TEXT NOT NULL,"
        "  display_name TEXT NOT NULL,"
        "  added_at TEXT NOT NULL"
        ")"));
}

bool Database::migrateToVersion1()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE folders ("
            "  id INTEGER PRIMARY KEY,"
            "  handle TEXT NOT NULL UNIQUE,"
            "  display_name TEXT NOT NULL,"
            "  last_scanned INTEGER,"
            "  available INTEGER NOT NULL DEFAULT 1,"
            "  added INTEGER NOT NULL)"),

        QStringLiteral(
            "CREATE TABLE files ("
            "  id INTEGER PRIMARY KEY,"
            "  folder_id INTEGER NOT NULL REFERENCES folders(id) ON DELETE CASCADE,"
            "  handle TEXT NOT NULL UNIQUE,"
            "  parent_handle TEXT,"
            "  display_name TEXT NOT NULL,"
            "  size_bytes INTEGER NOT NULL DEFAULT 0,"
            "  modified INTEGER,"
            "  duration_seconds REAL,"
            "  container TEXT,"
            "  video_codec TEXT,"
            "  audio_codec TEXT,"
            "  width INTEGER,"
            "  height INTEGER,"
            "  hdr INTEGER NOT NULL DEFAULT 0,"
            "  added INTEGER NOT NULL,"
            "  missing INTEGER NOT NULL DEFAULT 0)"),

        QStringLiteral("CREATE INDEX idx_files_folder ON files(folder_id)"),
        QStringLiteral("CREATE INDEX idx_files_parent ON files(parent_handle)"),
        QStringLiteral("CREATE INDEX idx_files_missing ON files(missing)"),

        QStringLiteral(
            "CREATE TABLE playback_state ("
            "  file_id INTEGER PRIMARY KEY REFERENCES files(id) ON DELETE CASCADE,"
            "  position_seconds REAL NOT NULL DEFAULT 0,"
            "  duration_seconds REAL NOT NULL DEFAULT 0,"
            "  watched_seconds REAL NOT NULL DEFAULT 0,"
            "  watched INTEGER NOT NULL DEFAULT 0,"
            "  last_played INTEGER)"),

        QStringLiteral(
            "CREATE INDEX idx_playback_last_played "
            "ON playback_state(last_played DESC)"),

        QStringLiteral(
            "CREATE TABLE media ("
            "  id INTEGER PRIMARY KEY,"
            "  tmdb_id INTEGER NOT NULL,"
            "  kind TEXT NOT NULL,"
            "  title TEXT,"
            "  original_title TEXT,"
            "  year INTEGER,"
            "  overview TEXT,"
            "  rating REAL,"
            "  certification TEXT,"
            "  runtime_minutes INTEGER,"
            "  genres TEXT,"
            "  poster_path TEXT,"
            "  backdrop_path TEXT,"
            "  updated INTEGER,"
            "  UNIQUE(tmdb_id, kind))"),

        QStringLiteral(
            "CREATE TABLE episodes ("
            "  id INTEGER PRIMARY KEY,"
            "  media_id INTEGER NOT NULL REFERENCES media(id) ON DELETE CASCADE,"
            "  file_id INTEGER REFERENCES files(id) ON DELETE SET NULL,"
            "  season INTEGER NOT NULL,"
            "  episode INTEGER NOT NULL,"
            "  title TEXT,"
            "  overview TEXT,"
            "  still_path TEXT,"
            "  air_date TEXT,"
            "  runtime_minutes INTEGER,"
            "  UNIQUE(media_id, season, episode))"),

        QStringLiteral("CREATE INDEX idx_episodes_file ON episodes(file_id)"),

        QStringLiteral(
            "CREATE TABLE match_overrides ("
            "  file_handle TEXT PRIMARY KEY,"
            "  tmdb_id INTEGER NOT NULL,"
            "  kind TEXT NOT NULL,"
            "  season INTEGER,"
            "  episode INTEGER,"
            "  pinned_at INTEGER NOT NULL)"),

        QStringLiteral(
            "CREATE TABLE file_media ("
            "  file_id INTEGER PRIMARY KEY REFERENCES files(id) ON DELETE CASCADE,"
            "  media_id INTEGER REFERENCES media(id) ON DELETE SET NULL,"
            "  confidence REAL NOT NULL DEFAULT 0,"
            "  suggested INTEGER NOT NULL DEFAULT 0,"
            "  matched_at INTEGER)"),

        QStringLiteral(
            "CREATE TABLE images ("
            "  id INTEGER PRIMARY KEY,"
            "  remote_path TEXT NOT NULL UNIQUE,"
            "  local_path TEXT NOT NULL,"
            "  kind TEXT NOT NULL,"
            "  fetched_at INTEGER NOT NULL)")
    };

    for (const QString &statement : statements) {
        if (!execOrLog(statement)) {
            return false;
        }
    }
    return true;
}

bool Database::transaction()
{
    QSqlDatabase db = handle();

    if (m_transactionDepth == 0) {
        if (!db.transaction()) {
            MM_LOG_E() << "could not begin a transaction" << db.lastError().text();
            return false;
        }
        m_transactionDepth = 1;
        return true;
    }

    QSqlQuery savepoint(db);
    if (!savepoint.exec(QStringLiteral("SAVEPOINT mm_%1").arg(m_transactionDepth))) {
        MM_LOG_E() << "could not begin a nested transaction at depth"
                   << m_transactionDepth << savepoint.lastError().text();
        return false;
    }
    ++m_transactionDepth;
    return true;
}

bool Database::commit()
{
    QSqlDatabase db = handle();

    if (m_transactionDepth <= 0) {
        MM_LOG_W() << "commit asked for with no transaction open";
        return false;
    }

    if (m_transactionDepth == 1) {
        if (!db.commit()) {
            MM_LOG_E() << "could not commit a transaction" << db.lastError().text();
            return false;
        }
        m_transactionDepth = 0;
        return true;
    }

    QSqlQuery release(db);
    if (!release.exec(QStringLiteral("RELEASE SAVEPOINT mm_%1")
                          .arg(m_transactionDepth - 1))) {
        MM_LOG_E() << "could not commit a nested transaction at depth"
                   << m_transactionDepth << release.lastError().text();
        return false;
    }
    --m_transactionDepth;
    return true;
}

bool Database::rollback()
{
    QSqlDatabase db = handle();

    if (m_transactionDepth <= 0) {
        MM_LOG_W() << "rollback asked for with no transaction open";
        return false;
    }

    if (m_transactionDepth == 1) {
        m_transactionDepth = 0;
        if (!db.rollback()) {
            MM_LOG_E() << "could not roll back a transaction" << db.lastError().text();
            return false;
        }
        return true;
    }

    const QString name = QStringLiteral("mm_%1").arg(m_transactionDepth - 1);
    --m_transactionDepth;

    QSqlQuery undo(db);
    if (!undo.exec(QStringLiteral("ROLLBACK TO SAVEPOINT ") + name)
        || !undo.exec(QStringLiteral("RELEASE SAVEPOINT ") + name)) {
        MM_LOG_E() << "could not roll back a nested transaction"
                   << name << undo.lastError().text();
        return false;
    }
    return true;
}
