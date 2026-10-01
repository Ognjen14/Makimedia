#include <QtTest>

#include <QFile>
#include <QScopedPointer>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "Data/Database.h"
#include "Streaming/LibrarySnapshot.h"

namespace {

const QString kServerId = QStringLiteral("server-7");
const QString kServerName = QStringLiteral("DESKTOP-TEST");

}

class TestLibrarySnapshot : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void onlyConfirmedTitlesOnDiskAreExported();
    void episodesKeepTheWholeListWithoutForeignFiles();
    void collectionsFollowTheExportedFilms();
    void creditsTravelWithTheTitlesTheyBelongTo();
    void nothingPrivateLeavesThePc();
    void oneFolderStandsForThePc();
    void theSnapshotHoldsTogether();
    void theSameLibraryGivesTheSameRevision();
    void aChangedTitleGivesAnotherRevision();
    void aRecheckAloneKeepsTheRevision();
    void theServedFilesAreTheExportedOnes();

private:
    bool exec(const QString &statement);
    void seed();
    LibrarySnapshotResult buildTo(const QString &name);
    QSet<qint64> idsIn(const QString &path, const QString &statement);
    int countIn(const QString &path, const QString &table);

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_source;
};

void TestLibrarySnapshot::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY(m_dir->isValid());
    m_source.reset(new Database);
    QVERIFY(m_source->open(m_dir->filePath(QStringLiteral("library.sqlite"))));
    seed();
}

void TestLibrarySnapshot::cleanup()
{
    m_source.reset();
    m_dir.reset();
}

bool TestLibrarySnapshot::exec(const QString &statement)
{
    QSqlQuery query(m_source->handle());
    if (!query.exec(statement)) {
        qWarning() << statement << query.lastError().text();
        return false;
    }
    return true;
}

void TestLibrarySnapshot::seed()
{
    const QStringList rows = {
        "INSERT INTO folders (id, handle, display_name, last_scanned, available, added)"
        " VALUES (1, 'F:/Media', 'Media', 1, 1, 1)",

        "INSERT INTO media (id, tmdb_id, kind, title, year, collection_id) VALUES"
        " (10, 949, 'movie', 'Heat', 1995, 500),"
        " (11, 111, 'movie', 'Guess', 2001, NULL),"
        " (12, 1438, 'tv', 'The Wire', 2002, NULL),"
        " (13, 222, 'movie', 'Gone', 1990, 600)",

        "INSERT INTO files (id, folder_id, handle, parent_handle, display_name, added, missing)"
        " VALUES"
        " (1, 1, 'F:/Media/Heat.mkv', 'F:/Media', 'Heat.mkv', 100, 0),"
        " (2, 1, 'F:/Media/Guess.mkv', 'F:/Media', 'Guess.mkv', 100, 0),"
        " (3, 1, 'F:/Media/clip.mkv', 'F:/Media', 'clip.mkv', 100, 0),"
        " (4, 1, 'F:/Media/Gone.mkv', 'F:/Media', 'Gone.mkv', 100, 1),"
        " (5, 1, 'F:/Media/Wire/e01.mkv', 'F:/Media/Wire', 'e01.mkv', 100, 0),"
        " (6, 1, 'F:/Media/Wire/e03.mkv', 'F:/Media/Wire', 'e03.mkv', 100, 0)",

        "INSERT INTO file_media (file_id, media_id, confidence, suggested) VALUES"
        " (1, 10, 1.0, 0), (2, 11, 0.5, 1), (4, 13, 1.0, 0), (5, 12, 1.0, 0), (6, 12, 0.5, 1)",

        "INSERT INTO episodes (id, media_id, file_id, season, episode, title) VALUES"
        " (100, 12, 5, 1, 1, 'The Target'),"
        " (101, 12, NULL, 1, 2, 'The Detail'),"
        " (102, 12, 6, 1, 3, 'The Buys')",

        "INSERT INTO fetched_seasons (media_id, season, fetched_at) VALUES (12, 1, 1000)",

        "INSERT INTO credits (media_id, episode_id, kind, name, role, sort) VALUES"
        " (10, 0, 0, 'Al Pacino', 'Vincent Hanna', 0),"
        " (12, 0, 0, 'Dominic West', 'Jimmy McNulty', 0),"
        " (12, 100, 1, 'Clark Johnson', 'Director', 0),"
        " (13, 0, 0, 'Nobody', '', 0)",

        "INSERT INTO collections (tmdb_id, name, fetched_at) VALUES"
        " (500, 'Heat Collection', 1000), (600, 'Gone Collection', 1000)",
        "INSERT INTO collection_parts (collection_id, tmdb_id, position, title) VALUES"
        " (500, 949, 1, 'Heat'), (500, 9999, 2, 'Heat 2'), (600, 222, 1, 'Gone')",
        "INSERT INTO film_details (tmdb_id, title, fetched_at) VALUES"
        " (949, 'Heat', 1000), (9999, 'Heat 2', 1000), (222, 'Gone', 1000)",

        "INSERT INTO custom_collections (id, name, created_at) VALUES (1, 'Mine', 5)",
        "INSERT INTO custom_collection_items (collection_id, media_id, position) VALUES"
        " (1, 10, 0), (1, 11, 1)",
        "INSERT INTO hidden_collections (collection_id, name, hidden_at)"
        " VALUES (600, 'Gone Collection', 7)",

        "INSERT INTO playback_state (file_id, position_seconds, duration_seconds, watched)"
        " VALUES (1, 30, 6000, 0)",
        "INSERT INTO external_subtitles (file_handle, sub_handle, display_name, language, added_at)"
        " VALUES ('F:/Media/Heat.mkv', 'F:/Subs/Heat.srt', 'Heat.srt', 'en', '2026')",
        "INSERT INTO match_overrides (file_handle, tmdb_id, kind, pinned_at)"
        " VALUES ('F:/Media/Heat.mkv', 949, 'movie', 1)",
        "INSERT INTO discarded_shows (file_handle, title, folder, discarded_at)"
        " VALUES ('F:/Media/clip.mkv', 'clip', 'F:/Media', 1)",
        "INSERT INTO removed_files (file_handle, folder_id, display_name, folder, removed_at)"
        " VALUES ('F:/Media/old.mkv', 1, 'old.mkv', 'F:/Media', 1)"
    };

    for (const QString &row : rows) {
        QVERIFY2(exec(row), qPrintable(row.left(60)));
    }
}

LibrarySnapshotResult TestLibrarySnapshot::buildTo(const QString &name)
{
    return LibrarySnapshot::build(m_source->filePath(), m_dir->filePath(name),
                                  kServerId, kServerName);
}

QSet<qint64> TestLibrarySnapshot::idsIn(const QString &path, const QString &statement)
{
    QSet<qint64> ids;
    const QString connection = QStringLiteral("snapshot-check");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(statement)) {
                while (query.next()) {
                    ids.insert(query.value(0).toLongLong());
                }
            } else {
                qWarning() << statement << query.lastError().text();
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return ids;
}

int TestLibrarySnapshot::countIn(const QString &path, const QString &table)
{
    const QSet<qint64> rowids = idsIn(path, QStringLiteral("SELECT rowid FROM ") + table);
    return int(rowids.size());
}

void TestLibrarySnapshot::onlyConfirmedTitlesOnDiskAreExported()
{
    const LibrarySnapshotResult result = buildTo(QStringLiteral("snapshot.sqlite"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.titles, 2);
    QCOMPARE(result.files, 2);

    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT id FROM media")),
             QSet<qint64>({10, 12}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT id FROM files")),
             QSet<qint64>({1, 5}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT file_id FROM file_media")),
             QSet<qint64>({1, 5}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT file_id FROM file_media WHERE suggested = 1")),
             QSet<qint64>());
}

void TestLibrarySnapshot::episodesKeepTheWholeListWithoutForeignFiles()
{
    const LibrarySnapshotResult result = buildTo(QStringLiteral("snapshot.sqlite"));
    QVERIFY2(result.ok, qPrintable(result.error));

    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT id FROM episodes")),
             QSet<qint64>({100, 101, 102}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT id FROM episodes WHERE file_id IS NOT NULL")),
             QSet<qint64>({100}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT media_id FROM fetched_seasons")),
             QSet<qint64>({12}));
}

void TestLibrarySnapshot::collectionsFollowTheExportedFilms()
{
    const LibrarySnapshotResult result = buildTo(QStringLiteral("snapshot.sqlite"));
    QVERIFY2(result.ok, qPrintable(result.error));

    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT tmdb_id FROM collections")),
             QSet<qint64>({500}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT tmdb_id FROM collection_parts")),
             QSet<qint64>({949, 9999}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT tmdb_id FROM film_details")),
             QSet<qint64>({949, 9999}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT id FROM custom_collections")),
             QSet<qint64>({1}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT media_id FROM custom_collection_items")),
             QSet<qint64>({10}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT collection_id FROM hidden_collections")),
             QSet<qint64>({600}));
}

void TestLibrarySnapshot::creditsTravelWithTheTitlesTheyBelongTo()
{
    const LibrarySnapshotResult result = buildTo(QStringLiteral("snapshot.sqlite"));
    QVERIFY2(result.ok, qPrintable(result.error));

    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT media_id FROM credits")),
             QSet<qint64>({10, 12}));
    QCOMPARE(idsIn(result.path, QStringLiteral(
                 "SELECT episode_id FROM credits WHERE media_id = 12")),
             QSet<qint64>({0, 100}));
}

void TestLibrarySnapshot::nothingPrivateLeavesThePc()
{
    const LibrarySnapshotResult result = buildTo(QStringLiteral("snapshot.sqlite"));
    QVERIFY2(result.ok, qPrintable(result.error));

    for (const QString &table : { QStringLiteral("playback_state"),
                                  QStringLiteral("external_subtitles"),
                                  QStringLiteral("match_overrides"),
                                  QStringLiteral("discarded_shows"),
                                  QStringLiteral("removed_files"),
                                  QStringLiteral("images") }) {
        QVERIFY2(countIn(result.path, table) == 0, qPrintable(table));
    }
}

void TestLibrarySnapshot::oneFolderStandsForThePc()
{
    const LibrarySnapshotResult result = buildTo(QStringLiteral("snapshot.sqlite"));
    QVERIFY2(result.ok, qPrintable(result.error));

    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT id FROM folders")),
             QSet<qint64>({LibrarySnapshot::ServerFolderId}));
    QCOMPARE(idsIn(result.path, QStringLiteral(
                 "SELECT id FROM folders WHERE handle = 'makimedia-server:server-7'"
                 " AND display_name = 'DESKTOP-TEST' AND added = 0")),
             QSet<qint64>({LibrarySnapshot::ServerFolderId}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT DISTINCT folder_id FROM files")),
             QSet<qint64>({LibrarySnapshot::ServerFolderId}));
}

void TestLibrarySnapshot::theSnapshotHoldsTogether()
{
    const LibrarySnapshotResult result = buildTo(QStringLiteral("snapshot.sqlite"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.schemaVersion, Database::targetSchemaVersion());
    QCOMPARE(idsIn(result.path, QStringLiteral("PRAGMA user_version")),
             QSet<qint64>({qint64(Database::targetSchemaVersion())}));
    QCOMPARE(idsIn(result.path, QStringLiteral("SELECT 1 FROM pragma_foreign_key_check")),
             QSet<qint64>());
    QVERIFY(!QFile::exists(result.path + QStringLiteral("-wal")));
    QCOMPARE(result.bytes, QFile(result.path).size());
}

void TestLibrarySnapshot::theSameLibraryGivesTheSameRevision()
{
    const LibrarySnapshotResult first = buildTo(QStringLiteral("first.sqlite"));
    const LibrarySnapshotResult second = buildTo(QStringLiteral("second.sqlite"));
    QVERIFY2(first.ok && second.ok, qPrintable(first.error + second.error));
    QCOMPARE(first.revision.size(), qsizetype(40));
    QCOMPARE(second.revision, first.revision);

    const LibrarySnapshotResult again = buildTo(QStringLiteral("first.sqlite"));
    QCOMPARE(again.revision, first.revision);
    QCOMPARE(LibrarySnapshot::revisionOf(again.path), first.revision);
}

void TestLibrarySnapshot::aChangedTitleGivesAnotherRevision()
{
    const LibrarySnapshotResult before = buildTo(QStringLiteral("before.sqlite"));
    QVERIFY(exec(QStringLiteral("UPDATE media SET title = 'Heat (1995)' WHERE id = 10")));
    const LibrarySnapshotResult after = buildTo(QStringLiteral("after.sqlite"));
    QVERIFY(before.ok && after.ok);
    QVERIFY(after.revision != before.revision);
}

void TestLibrarySnapshot::aRecheckAloneKeepsTheRevision()
{
    QVERIFY(exec(QStringLiteral(
        "UPDATE files SET probe_attempted = 1000, match_attempted = 1000")));
    QVERIFY(exec(QStringLiteral(
        "UPDATE media SET updated = 1000, collection_checked = 1000")));
    QVERIFY(exec(QStringLiteral("UPDATE file_media SET matched_at = 1000")));
    const LibrarySnapshotResult before = buildTo(QStringLiteral("before.sqlite"));

    QVERIFY(exec(QStringLiteral(
        "UPDATE files SET probe_attempted = 2000, match_attempted = 2000")));
    QVERIFY(exec(QStringLiteral(
        "UPDATE media SET updated = 2000, collection_checked = 2000")));
    QVERIFY(exec(QStringLiteral("UPDATE file_media SET matched_at = 2000")));
    QVERIFY(exec(QStringLiteral("UPDATE collections SET fetched_at = 2000")));
    QVERIFY(exec(QStringLiteral("UPDATE film_details SET fetched_at = 2000")));
    QVERIFY(exec(QStringLiteral("UPDATE fetched_seasons SET fetched_at = 2000")));
    const LibrarySnapshotResult after = buildTo(QStringLiteral("after.sqlite"));

    QVERIFY(before.ok && after.ok);
    QCOMPARE(after.revision, before.revision);
}

void TestLibrarySnapshot::theServedFilesAreTheExportedOnes()
{
    const LibrarySnapshotResult result = buildTo(QStringLiteral("snapshot.sqlite"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.servedFiles.size(), qsizetype(2));
    QCOMPARE(result.servedFiles.value(1), QStringLiteral("F:/Media/Heat.mkv"));
    QCOMPARE(result.servedFiles.value(5), QStringLiteral("F:/Media/Wire/e01.mkv"));
}

QTEST_GUILESS_MAIN(TestLibrarySnapshot)
#include "tst_librarysnapshot.moc"
