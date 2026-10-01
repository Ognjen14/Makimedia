#include <QtTest>

#include <QScopedPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>

#include "Data/Database.h"

namespace {

class RawConnection
{
public:
    explicit RawConnection(const QString &filePath)
        : m_name(QStringLiteral("tst-%1").arg(
              QUuid::createUuid().toString(QUuid::WithoutBraces)))
    {
        QSqlDatabase db =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        db.setDatabaseName(filePath);
        m_opened = db.open();
    }

    ~RawConnection()
    {
        if (m_opened) {
            QSqlDatabase::database(m_name, false).close();
        }
        QSqlDatabase::removeDatabase(m_name);
    }

    bool isOpen() const { return m_opened; }

    QSqlDatabase handle() const
    {
        return QSqlDatabase::database(m_name, false);
    }

    bool exec(const QString &statement)
    {
        QSqlQuery query(handle());
        return query.exec(statement);
    }

    int userVersion()
    {
        QSqlQuery query(handle());
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
            return -1;
        }
        return query.value(0).toInt();
    }

private:
    QString m_name;
    bool m_opened = false;
};

QStringList tablesIn(const QString &filePath)
{
    QStringList names;

    RawConnection raw(filePath);
    if (!raw.isOpen()) {
        return names;
    }

    QSqlQuery query(raw.handle());
    if (!query.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type = 'table'"
            " AND name NOT LIKE 'sqlite_%' ORDER BY name"))) {
        return names;
    }

    while (query.next()) {
        names.append(query.value(0).toString());
    }
    return names;
}

QStringList columnsOf(const QString &filePath, const QString &table)
{
    QStringList names;

    RawConnection raw(filePath);
    if (!raw.isOpen()) {
        return names;
    }

    QSqlQuery query(raw.handle());
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        return names;
    }

    while (query.next()) {
        names.append(query.value(1).toString());
    }
    return names;
}

bool addSubtitle(RawConnection &raw, const QString &fileHandle,
                 const QString &subHandle)
{
    QSqlQuery query(raw.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO external_subtitles"
        " (file_handle, sub_handle, display_name, added_at)"
        " VALUES (:file, :sub, 'English', '2026-09-11')"));
    query.bindValue(QStringLiteral(":file"), fileHandle);
    query.bindValue(QStringLiteral(":sub"), subHandle);
    return query.exec();
}

QStringList subtitleRowsIn(const QString &filePath)
{
    QStringList rows;

    RawConnection raw(filePath);
    if (!raw.isOpen()) {
        return rows;
    }

    QSqlQuery query(raw.handle());
    if (!query.exec(QStringLiteral(
            "SELECT file_handle, sub_handle FROM external_subtitles"
            " ORDER BY file_handle, sub_handle"))) {
        return rows;
    }

    while (query.next()) {
        rows.append(query.value(0).toString() + QStringLiteral(" | ")
                    + query.value(1).toString());
    }
    return rows;
}

}

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void aFreshDatabaseReachesTheCurrentVersion();
    void everyTableIsCreated();
    void theListQueriesHaveTheirIndexes();
    void reopeningMigratesNothing();
    void dataSurvivesTheUpgradeToSix();
    void theUpgradeToTenFoldsWhatIsAlreadyThere();
    void theUpgradeToElevenLeavesOldSeasonsWithoutAPoster();
    void theUpgradeToTwelveLeavesOldFilmsUnchecked();
    void backslashedSubtitleHandlesAreStraightened();
    void removingAFolderForgetsItsSubtitles();
    void aSchemaFromANewerBuildIsRefused();
    void anUnwritableLocationFailsRatherThanThrows();
    void anInnerRollbackKeepsTheOuterWrites();
    void anOuterRollbackTakesTheInnerWritesWithIt();

private:
    QString dbPath() const;
    bool seedOneFile(Database &database);

    QScopedPointer<QTemporaryDir> m_dir;
};

void TestDatabase::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));
}

QString TestDatabase::dbPath() const
{
    return m_dir->path() + QStringLiteral("/library.sqlite");
}

bool TestDatabase::seedOneFile(Database &database)
{
    QSqlQuery folder(database.handle());
    folder.prepare(QStringLiteral(
        "INSERT INTO folders (handle, display_name, added)"
        " VALUES ('F:/Media', 'Media', 1)"));
    if (!folder.exec()) {
        return false;
    }

    QSqlQuery file(database.handle());
    file.prepare(QStringLiteral(
        "INSERT INTO files (folder_id, handle, display_name, added)"
        " VALUES (1, 'F:/Media/The Wire/s01/e01.mkv', 'e01.mkv', 1)"));
    return file.exec();
}

void TestDatabase::aFreshDatabaseReachesTheCurrentVersion()
{
    Database database;

    QVERIFY(database.open(dbPath()));
    QVERIFY(database.isOpen());
    QCOMPARE(database.schemaVersion(), Database::targetSchemaVersion());
    QCOMPARE(database.schemaVersion(), 22);
}

void TestDatabase::removingAFolderForgetsItsSubtitles()
{
    Database database;
    QVERIFY(database.open(dbPath()));
    QVERIFY(seedOneFile(database));

    QSqlQuery attach(database.handle());
    QVERIFY(attach.exec(QStringLiteral(
        "INSERT INTO external_subtitles"
        " (file_handle, sub_handle, display_name, added_at) VALUES"
        " ('F:/Media/The Wire/s01/e01.mkv', 'F:/Media/e01.en.srt', 'English', 'x'),"
        " ('D:/Downloads/clip.mkv', 'D:/Downloads/clip.srt', 'English', 'x')")));

    QSqlQuery remove(database.handle());
    QVERIFY(remove.exec(QStringLiteral("DELETE FROM folders")));
    database.close();

    QCOMPARE(subtitleRowsIn(dbPath()),
             QStringList({QStringLiteral("D:/Downloads/clip.mkv | D:/Downloads/clip.srt")}));
}

void TestDatabase::everyTableIsCreated()
{
    Database database;
    QVERIFY(database.open(dbPath()));
    database.close();

    QCOMPARE(tablesIn(dbPath()),
             QStringList({QStringLiteral("added_collection_films"),
                          QStringLiteral("collection_parts"),
                          QStringLiteral("collections"),
                          QStringLiteral("credits"),
                          QStringLiteral("custom_collection_items"),
                          QStringLiteral("custom_collections"),
                          QStringLiteral("discarded_shows"),
                          QStringLiteral("episodes"),
                          QStringLiteral("external_subtitles"),
                          QStringLiteral("fetched_credits"),
                          QStringLiteral("fetched_seasons"),
                          QStringLiteral("file_media"),
                          QStringLiteral("files"),
                          QStringLiteral("film_details"),
                          QStringLiteral("folders"),
                          QStringLiteral("hidden_collection_films"),
                          QStringLiteral("hidden_collections"),
                          QStringLiteral("images"),
                          QStringLiteral("match_overrides"),
                          QStringLiteral("media"),
                          QStringLiteral("playback_state"),
                          QStringLiteral("removed_files")}));
}

void TestDatabase::theListQueriesHaveTheirIndexes()
{
    Database database;
    QVERIFY(database.open(dbPath()));

    QStringList indexes;
    QSqlQuery names(database.handle());
    QVERIFY(names.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type = 'index'")));
    while (names.next()) {
        indexes.append(names.value(0).toString());
    }

    QVERIFY2(indexes.contains(QStringLiteral("idx_file_media_media")),
             qPrintable(indexes.join(QStringLiteral(", "))));
    QVERIFY2(indexes.contains(QStringLiteral("idx_files_added")),
             qPrintable(indexes.join(QStringLiteral(", "))));
    for (const QString &name : {QStringLiteral("idx_files_match_attempted"),
                                QStringLiteral("idx_files_probe_attempted"),
                                QStringLiteral("idx_file_media_suggested"),
                                QStringLiteral("idx_media_kind"),
                                QStringLiteral("idx_files_display_name"),
                                QStringLiteral("idx_files_handle_nocase")}) {
        QVERIFY2(indexes.contains(name), qPrintable(indexes.join(QStringLiteral(", "))));
    }

    QString kindPlan;
    QSqlQuery byKind(database.handle());
    QVERIFY(byKind.exec(QStringLiteral(
        "EXPLAIN QUERY PLAN SELECT id FROM media WHERE kind = 'tv'")));
    while (byKind.next()) {
        kindPlan += byKind.value(3).toString() + QLatin1Char(' ');
    }
    QVERIFY2(kindPlan.contains(QStringLiteral("idx_media_kind")), qPrintable(kindPlan));

    QString plan;
    QSqlQuery explain(database.handle());
    QVERIFY(explain.exec(QStringLiteral(
        "EXPLAIN QUERY PLAN"
        " SELECT COUNT(*) FROM file_media WHERE media_id = 1")));
    while (explain.next()) {
        plan += explain.value(3).toString() + QLatin1Char(' ');
    }

    QVERIFY2(plan.contains(QStringLiteral("idx_file_media_media")),
             qPrintable(plan));
}

void TestDatabase::reopeningMigratesNothing()
{
    {
        Database first;
        QVERIFY(first.open(dbPath()));
        QVERIFY(seedOneFile(first));
        first.close();
    }

    const QStringList before = tablesIn(dbPath());

    Database second;
    QVERIFY(second.open(dbPath()));
    QCOMPARE(second.schemaVersion(), Database::targetSchemaVersion());
    QCOMPARE(tablesIn(dbPath()), before);

    QSqlQuery count(second.handle());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM files")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 1);
}

void TestDatabase::dataSurvivesTheUpgradeToSix()
{
    {
        Database current;
        QVERIFY(current.open(dbPath()));
        QVERIFY(seedOneFile(current));
        current.close();
    }

    {
        RawConnection raw(dbPath());
        QVERIFY(raw.isOpen());

        if (!raw.exec(QStringLiteral(
                "ALTER TABLE files DROP COLUMN probe_attempted"))) {
            QSKIP("this SQLite cannot drop a column, so a version 5 database "
                  "cannot be built to migrate from");
        }

        QVERIFY(raw.exec(QStringLiteral("PRAGMA user_version = 5")));
    }

    QVERIFY(!columnsOf(dbPath(), QStringLiteral("files"))
                 .contains(QStringLiteral("probe_attempted")));

    Database upgraded;
    QVERIFY(upgraded.open(dbPath()));

    QCOMPARE(upgraded.schemaVersion(), Database::targetSchemaVersion());
    QVERIFY(columnsOf(dbPath(), QStringLiteral("files"))
                .contains(QStringLiteral("probe_attempted")));

    QSqlQuery row(upgraded.handle());
    QVERIFY(row.exec(QStringLiteral(
        "SELECT display_name, probe_attempted FROM files")));
    QVERIFY(row.next());
    QCOMPARE(row.value(0).toString(), QStringLiteral("e01.mkv"));
    QVERIFY(row.value(1).isNull());
    QVERIFY(!row.next());
}

void TestDatabase::backslashedSubtitleHandlesAreStraightened()
{
    {
        Database current;
        QVERIFY(current.open(dbPath()));
        current.close();
    }

    {
        RawConnection raw(dbPath());
        QVERIFY(raw.isOpen());

        QVERIFY(addSubtitle(raw, QStringLiteral("E:\\TV\\a.mkv"),
                            QStringLiteral("E:\\TV\\a.en.srt")));
        QVERIFY(addSubtitle(raw, QStringLiteral("E:/TV/b.mkv"),
                            QStringLiteral("E:\\TV\\b.en.srt")));
        QVERIFY(addSubtitle(raw, QStringLiteral("E:/TV/c.mkv"),
                            QStringLiteral("E:/TV/c.en.srt")));
        QVERIFY(addSubtitle(raw, QStringLiteral("E:\\TV\\c.mkv"),
                            QStringLiteral("E:\\TV\\c.en.srt")));
        QVERIFY(addSubtitle(raw, QStringLiteral("E:/TV/d.mkv"),
                            QStringLiteral("E:/TV/d.en.srt")));

        QVERIFY(raw.exec(QStringLiteral("PRAGMA user_version = 6")));
    }

    const QStringList before = subtitleRowsIn(dbPath());
    QCOMPARE(before.size(), 5);

    {
        Database upgraded;
        QVERIFY(upgraded.open(dbPath()));
        QCOMPARE(upgraded.schemaVersion(), Database::targetSchemaVersion());
        upgraded.close();
    }

    const QStringList after = subtitleRowsIn(dbPath());

#ifdef Q_OS_WIN
    QCOMPARE(after,
             QStringList({QStringLiteral("E:/TV/a.mkv | E:/TV/a.en.srt"),
                          QStringLiteral("E:/TV/b.mkv | E:/TV/b.en.srt"),
                          QStringLiteral("E:/TV/c.mkv | E:/TV/c.en.srt"),
                          QStringLiteral("E:/TV/d.mkv | E:/TV/d.en.srt")}));
#else
    QCOMPARE(after, before);
#endif
}

void TestDatabase::aSchemaFromANewerBuildIsRefused()
{
    const int newer = Database::targetSchemaVersion() + 1;

    {
        Database current;
        QVERIFY(current.open(dbPath()));
        current.close();
    }

    {
        RawConnection raw(dbPath());
        QVERIFY(raw.isOpen());
        QVERIFY(raw.exec(QStringLiteral("PRAGMA user_version = %1").arg(newer)));
    }

    Database database;
    QVERIFY(!database.open(dbPath()));
    QVERIFY(!database.isOpen());

    RawConnection raw(dbPath());
    QVERIFY(raw.isOpen());
    QCOMPARE(raw.userVersion(), newer);
}

void TestDatabase::anUnwritableLocationFailsRatherThanThrows()
{
    Database database;

    QVERIFY(!database.open(m_dir->path()));
    QVERIFY(!database.isOpen());
}

void TestDatabase::theUpgradeToTenFoldsWhatIsAlreadyThere()
{
    {
        Database current;
        QVERIFY(current.open(dbPath()));
        current.close();
    }

    {
        RawConnection raw(dbPath());
        QVERIFY(raw.isOpen());

        if (!raw.exec(QStringLiteral("ALTER TABLE files DROP COLUMN name_key"))) {
            QSKIP("this SQLite cannot drop a column, so a version 9 database "
                  "cannot be built to migrate from");
        }
        QVERIFY(raw.exec(QStringLiteral("ALTER TABLE media DROP COLUMN title_key")));
        QVERIFY(raw.exec(QStringLiteral("ALTER TABLE episodes DROP COLUMN title_key")));
        for (const QString &name : {QStringLiteral("idx_files_match_attempted"),
                                    QStringLiteral("idx_files_probe_attempted"),
                                    QStringLiteral("idx_file_media_suggested"),
                                    QStringLiteral("idx_media_kind"),
                                    QStringLiteral("idx_files_display_name"),
                                    QStringLiteral("idx_files_handle_nocase")}) {
            QVERIFY(raw.exec(QStringLiteral("DROP INDEX ") + name));
        }

        QVERIFY(raw.exec(QStringLiteral(
            "INSERT INTO folders (id, handle, display_name, added)"
            " VALUES (1, 'F:/Media', 'Media', 1)")));
        QVERIFY(raw.exec(QStringLiteral(
            "INSERT INTO files (folder_id, handle, display_name, added)"
            " VALUES (1, 'F:/Media/Žene.S01E01.mkv', 'Žene.S01E01.mkv', 1)")));
        QVERIFY(raw.exec(QStringLiteral(
            "INSERT INTO media (id, tmdb_id, kind, title)"
            " VALUES (1, 7, 'tv', 'Đorđe i Zmaj')")));
        QVERIFY(raw.exec(QStringLiteral(
            "INSERT INTO episodes (media_id, season, episode, title)"
            " VALUES (1, 1, 1, 'Škola')")));
        QVERIFY(raw.exec(QStringLiteral("PRAGMA user_version = 9")));
    }

    QVERIFY(!columnsOf(dbPath(), QStringLiteral("files"))
                 .contains(QStringLiteral("name_key")));

    Database upgraded;
    QVERIFY(upgraded.open(dbPath()));
    QCOMPARE(upgraded.schemaVersion(), Database::targetSchemaVersion());

    const auto single = [&upgraded](const QString &sql) {
        QSqlQuery query(upgraded.handle());
        if (!query.exec(sql) || !query.next()) {
            return QString();
        }
        return query.value(0).toString();
    };

    QCOMPARE(single(QStringLiteral("SELECT name_key FROM files")),
             QStringLiteral("zene.s01e01.mkv"));
    QCOMPARE(single(QStringLiteral("SELECT title_key FROM media")),
             QStringLiteral("djordje i zmaj"));
    QCOMPARE(single(QStringLiteral("SELECT title_key FROM episodes")),
             QStringLiteral("skola"));
    QCOMPARE(single(QStringLiteral(
                 "SELECT name FROM sqlite_master WHERE name = 'idx_media_kind'")),
             QStringLiteral("idx_media_kind"));
}

void TestDatabase::theUpgradeToElevenLeavesOldSeasonsWithoutAPoster()
{
    {
        Database current;
        QVERIFY(current.open(dbPath()));
        current.close();
    }

    {
        RawConnection raw(dbPath());
        QVERIFY(raw.isOpen());

        if (!raw.exec(QStringLiteral("ALTER TABLE fetched_seasons DROP COLUMN poster_path"))) {
            QSKIP("this SQLite cannot drop a column, so a version 10 database "
                  "cannot be built to migrate from");
        }
        QVERIFY(raw.exec(QStringLiteral(
            "INSERT INTO media (id, tmdb_id, kind, title) VALUES (1, 1438, 'tv', 'The Wire')")));
        QVERIFY(raw.exec(QStringLiteral(
            "INSERT INTO fetched_seasons (media_id, season, fetched_at) VALUES (1, 1, 1)")));
        QVERIFY(raw.exec(QStringLiteral("PRAGMA user_version = 10")));
    }

    Database upgraded;
    QVERIFY(upgraded.open(dbPath()));
    QCOMPARE(upgraded.schemaVersion(), Database::targetSchemaVersion());
    QVERIFY(columnsOf(dbPath(), QStringLiteral("fetched_seasons"))
                .contains(QStringLiteral("poster_path")));

    QSqlQuery query(upgraded.handle());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT COUNT(*) FROM fetched_seasons WHERE poster_path IS NULL")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestDatabase::theUpgradeToTwelveLeavesOldFilmsUnchecked()
{
    {
        Database current;
        QVERIFY(current.open(dbPath()));
        current.close();
    }

    {
        RawConnection raw(dbPath());
        QVERIFY(raw.isOpen());

        QVERIFY(raw.exec(QStringLiteral("DROP INDEX idx_media_collection")));
        if (!raw.exec(QStringLiteral("ALTER TABLE media DROP COLUMN collection_id"))) {
            QSKIP("this SQLite cannot drop a column, so a version 11 database "
                  "cannot be built to migrate from");
        }
        QVERIFY(raw.exec(QStringLiteral("ALTER TABLE media DROP COLUMN collection_checked")));
        QVERIFY(raw.exec(QStringLiteral("DROP TABLE collection_parts")));
        QVERIFY(raw.exec(QStringLiteral("DROP TABLE collections")));
        QVERIFY(raw.exec(QStringLiteral(
            "INSERT INTO media (id, tmdb_id, kind, title) VALUES (1, 105, 'movie', 'Back to the Future')")));
        QVERIFY(raw.exec(QStringLiteral("PRAGMA user_version = 11")));
    }

    Database upgraded;
    QVERIFY(upgraded.open(dbPath()));
    QCOMPARE(upgraded.schemaVersion(), Database::targetSchemaVersion());

    const QStringList columns = columnsOf(dbPath(), QStringLiteral("media"));
    QVERIFY(columns.contains(QStringLiteral("collection_id")));
    QVERIFY(columns.contains(QStringLiteral("collection_checked")));
    QVERIFY(tablesIn(dbPath()).contains(QStringLiteral("collections")));
    QVERIFY(tablesIn(dbPath()).contains(QStringLiteral("collection_parts")));

    QSqlQuery query(upgraded.handle());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT COUNT(*) FROM media WHERE collection_checked IS NULL AND collection_id IS NULL")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestDatabase::anInnerRollbackKeepsTheOuterWrites()
{
    Database database;
    QVERIFY(database.open(dbPath()));

    QVERIFY(database.transaction());
    QSqlQuery outer(database.handle());
    QVERIFY(outer.exec(QStringLiteral(
        "INSERT INTO folders (handle, display_name, added) VALUES ('F:/A', 'A', 1)")));

    QVERIFY(database.transaction());
    QSqlQuery inner(database.handle());
    QVERIFY(inner.exec(QStringLiteral(
        "INSERT INTO folders (handle, display_name, added) VALUES ('F:/B', 'B', 1)")));
    QVERIFY(database.rollback());

    QVERIFY(database.transaction());
    QSqlQuery kept(database.handle());
    QVERIFY(kept.exec(QStringLiteral(
        "INSERT INTO folders (handle, display_name, added) VALUES ('F:/C', 'C', 1)")));
    QVERIFY(database.commit());

    QVERIFY(database.commit());

    QSqlQuery read(database.handle());
    QVERIFY(read.exec(QStringLiteral("SELECT handle FROM folders ORDER BY handle")));
    QStringList handles;
    while (read.next()) {
        handles.append(read.value(0).toString());
    }
    QCOMPARE(handles, (QStringList{QStringLiteral("F:/A"), QStringLiteral("F:/C")}));

    QVERIFY(!database.commit());
    QVERIFY(!database.rollback());
}

void TestDatabase::anOuterRollbackTakesTheInnerWritesWithIt()
{
    Database database;
    QVERIFY(database.open(dbPath()));

    QVERIFY(database.transaction());
    QVERIFY(database.transaction());
    QSqlQuery inner(database.handle());
    QVERIFY(inner.exec(QStringLiteral(
        "INSERT INTO folders (handle, display_name, added) VALUES ('F:/B', 'B', 1)")));
    QVERIFY(database.commit());
    QVERIFY(database.rollback());

    QSqlQuery read(database.handle());
    QVERIFY(read.exec(QStringLiteral("SELECT COUNT(*) FROM folders")));
    QVERIFY(read.next());
    QCOMPARE(read.value(0).toInt(), 0);

    QVERIFY(database.transaction());
    QVERIFY(database.commit());
}

QTEST_GUILESS_MAIN(TestDatabase)

#include "tst_database.moc"
