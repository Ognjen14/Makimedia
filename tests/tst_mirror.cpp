#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "Data/Database.h"
#include "Streaming/LibrarySnapshot.h"
#include "Streaming/Mirror.h"
#include "Streaming/MirrorSync.h"
#include "Streaming/StreamProtocol.h"
#include "Streaming/StreamServer.h"

namespace {

const QString kServerId = QStringLiteral("server-7");

bool run(Database &db, const QString &statement)
{
    QSqlQuery query(db.handle());
    if (!query.exec(statement)) {
        qWarning() << statement.left(80) << query.lastError().text();
        return false;
    }
    return true;
}

double positionIn(const QString &path, qint64 fileId)
{
    Database db;
    if (!db.open(path)) {
        return -1;
    }
    QSqlQuery query(db.handle());
    query.prepare(QStringLiteral(
        "SELECT position_seconds FROM playback_state WHERE file_id = :id"));
    query.bindValue(QStringLiteral(":id"), fileId);
    double position = -1;
    if (query.exec() && query.next()) {
        position = query.value(0).toDouble();
    }
    query.finish();
    return position;
}

}

class TestMirror : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void progressFollowsTheFileNotItsId();
    void thereIsNothingToCarryTheFirstTime();
    void storedLibrariesAreListedAndForgotten();
    void aServerIdCannotLeaveTheFolder();

    void aFirstConnectDownloadsAndTheNextOpensAtOnce();
    void anUpdateKeepsWhereTheDeviceStopped();
    void aNewerLibraryIsRefused();
    void aDamagedDownloadLeavesTheOldCopy();

private:
    void seedSource();
    LibrarySnapshotResult snapshot();
    void serve(const LibrarySnapshotResult &result, int schemaOverride = 0,
               const QString &revisionOverride = QString());
    QString base() const;
    QString sessionBase();

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_source;
    QScopedPointer<StreamServer> m_server;
    QNetworkAccessManager m_network;
};

void TestMirror::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY(m_dir->isValid());
}

void TestMirror::cleanup()
{
    m_server.reset();
    m_source.reset();
    m_dir.reset();
}

void TestMirror::seedSource()
{
    m_source.reset(new Database);
    QVERIFY(m_source->open(m_dir->filePath(QStringLiteral("pc.sqlite"))));
    QVERIFY(run(*m_source, QStringLiteral(
        "INSERT INTO folders (id, handle, display_name, added) VALUES (1, 'D:/Movies', 'Movies', 1)")));
    QVERIFY(run(*m_source, QStringLiteral(
        "INSERT INTO media (id, tmdb_id, kind, title) VALUES (10, 949, 'movie', 'Heat'),"
        " (11, 348, 'movie', 'Alien')")));
    QVERIFY(run(*m_source, QStringLiteral(
        "INSERT INTO files (id, folder_id, handle, display_name, added) VALUES"
        " (1, 1, 'D:/Movies/Heat.mkv', 'Heat.mkv', 1),"
        " (2, 1, 'D:/Movies/Alien.mkv', 'Alien.mkv', 1)")));
    QVERIFY(run(*m_source, QStringLiteral(
        "INSERT INTO file_media (file_id, media_id, confidence, suggested) VALUES"
        " (1, 10, 1, 0), (2, 11, 1, 0)")));
}

LibrarySnapshotResult TestMirror::snapshot()
{
    return LibrarySnapshot::build(m_source->filePath(),
                                  m_dir->filePath(QStringLiteral("snapshot.sqlite")),
                                  kServerId, QStringLiteral("DESKTOP-TEST"));
}

void TestMirror::serve(const LibrarySnapshotResult &result, int schemaOverride,
                       const QString &revisionOverride)
{
    m_server.reset(new StreamServer);
    StreamServerInfo info;
    info.serverId = kServerId;
    info.name = QStringLiteral("DESKTOP-TEST");
    info.schemaVersion = schemaOverride > 0 ? schemaOverride : result.schemaVersion;
    info.snapshotPath = result.path;
    info.revision = revisionOverride.isEmpty() ? result.revision : revisionOverride;
    info.snapshotBytes = result.bytes;
    m_server->setInfo(info);
    m_server->setFiles(result.servedFiles);
    QVERIFY(m_server->start(0));
}

QString TestMirror::base() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_server->port());
}

QString TestMirror::sessionBase()
{
    QNetworkRequest request(QUrl(base() + QStringLiteral("/v1/session")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    const QJsonObject asking{
        { QStringLiteral("device"), QStringLiteral("test-device") },
        { QStringLiteral("name"), QStringLiteral("Tablet") },
        { QStringLiteral("form"), QStringLiteral("tablet") }
    };

    QScopedPointer<QNetworkReply> reply(
        m_network.post(request, QJsonDocument(asking).toJson(QJsonDocument::Compact)));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    if (!reply->isFinished() && !finished.wait(10000)) {
        return base();
    }

    const QString token = QJsonDocument::fromJson(reply->readAll())
                              .object()
                              .value(QStringLiteral("token"))
                              .toString();
    return token.isEmpty() ? base() : base() + StreamProtocol::tokenPrefix(token);
}

void TestMirror::progressFollowsTheFileNotItsId()
{
    const QString oldPath = m_dir->filePath(QStringLiteral("old.sqlite"));
    const QString newPath = m_dir->filePath(QStringLiteral("new.sqlite"));
    {
        Database old;
        QVERIFY(old.open(oldPath));
        QVERIFY(run(old, QStringLiteral(
            "INSERT INTO folders (id, handle, display_name, added) VALUES (1, 'pc', 'PC', 0)")));
        QVERIFY(run(old, QStringLiteral(
            "INSERT INTO files (id, folder_id, handle, display_name, added) VALUES"
            " (1, 1, 'D:/a.mkv', 'a.mkv', 0), (2, 1, 'D:/b.mkv', 'b.mkv', 0),"
            " (3, 1, 'D:/c.mkv', 'c.mkv', 0)")));
        QVERIFY(run(old, QStringLiteral(
            "INSERT INTO playback_state (file_id, position_seconds, duration_seconds, watched,"
            " last_played) VALUES (1, 100, 600, 0, 5), (2, 200, 600, 1, 6), (3, 300, 600, 0, 7)")));
    }
    {
        Database fresh;
        QVERIFY(fresh.open(newPath));
        QVERIFY(run(fresh, QStringLiteral(
            "INSERT INTO folders (id, handle, display_name, added) VALUES (1, 'pc', 'PC', 0)")));
        QVERIFY(run(fresh, QStringLiteral(
            "INSERT INTO files (id, folder_id, handle, display_name, added) VALUES"
            " (1, 1, 'D:/a.mkv', 'a.mkv', 0), (7, 1, 'D:/b.mkv', 'b.mkv', 0)")));
    }

    int carried = -1;
    QString error;
    QVERIFY2(Mirror::carryPlayback(oldPath, newPath, &carried, &error), qPrintable(error));
    QCOMPARE(carried, 2);
    QCOMPARE(positionIn(newPath, 1), 100.0);
    QCOMPARE(positionIn(newPath, 7), 200.0);
    QCOMPARE(positionIn(newPath, 3), -1.0);
}

void TestMirror::thereIsNothingToCarryTheFirstTime()
{
    const QString newPath = m_dir->filePath(QStringLiteral("new.sqlite"));
    int carried = -1;
    QString error;
    QVERIFY(Mirror::carryPlayback(QString(), newPath, &carried, &error));
    QCOMPARE(carried, 0);
    QVERIFY(Mirror::looksLikeSqlite(newPath));
}

void TestMirror::storedLibrariesAreListedAndForgotten()
{
    const QString root = m_dir->filePath(QStringLiteral("servers"));
    Mirror::Stored stored;
    stored.serverId = kServerId;
    stored.name = QStringLiteral("DESKTOP-TEST");
    stored.revision = QStringLiteral("abc");
    stored.bytes = 42;
    stored.schemaVersion = 17;
    QVERIFY(Mirror::writeInfo(root, stored));

    const Mirror::Stored read = Mirror::readInfo(root, kServerId);
    QCOMPARE(read.name, stored.name);
    QCOMPARE(read.revision, stored.revision);
    QCOMPARE(read.schemaVersion, 17);

    QVERIFY(Mirror::all(root).isEmpty());
    QFile library(Mirror::libraryPath(root, kServerId));
    QVERIFY(library.open(QIODevice::WriteOnly));
    library.write("SQLite format 3");
    library.close();
    const QList<Mirror::Stored> listed = Mirror::all(root);
    QCOMPARE(listed.size(), qsizetype(1));
    QCOMPARE(listed.first().serverId, kServerId);
    QCOMPARE(listed.first().bytes, Q_INT64_C(15));

    QVERIFY(Mirror::forget(root, kServerId));
    QVERIFY(Mirror::all(root).isEmpty());
}

void TestMirror::aServerIdCannotLeaveTheFolder()
{
    QVERIFY(Mirror::directoryFor(QStringLiteral("R"), QStringLiteral("../escape")).isEmpty());
    QVERIFY(Mirror::directoryFor(QStringLiteral("R"), QStringLiteral("a/b")).isEmpty());
    QVERIFY(Mirror::directoryFor(QStringLiteral("R"), QString()).isEmpty());
    QCOMPARE(Mirror::directoryFor(QStringLiteral("R"), QStringLiteral("9f5c-48f8")),
             QStringLiteral("R/9f5c-48f8"));
}

void TestMirror::aFirstConnectDownloadsAndTheNextOpensAtOnce()
{
    seedSource();
    const LibrarySnapshotResult built = snapshot();
    QVERIFY2(built.ok, qPrintable(built.error));
    serve(built);

    MirrorSync sync(&m_network);
    sync.setRoot(m_dir->filePath(QStringLiteral("servers")));
    QSignalSpy downloading(&sync, &MirrorSync::downloading);
    QSignalSpy ready(&sync, &MirrorSync::ready);
    QSignalSpy failed(&sync, &MirrorSync::failed);

    sync.start(sessionBase(), kServerId, QStringLiteral("DESKTOP-TEST"));
    QVERIFY(ready.wait(10000));
    QCOMPARE(failed.count(), 0);
    QCOMPARE(downloading.count(), 1);
    QVERIFY(downloading.first().at(0).toBool());
    QVERIFY(ready.first().at(1).toBool());
    const QString path = ready.first().at(0).toString();
    QVERIFY(Mirror::looksLikeSqlite(path));
    QCOMPARE(Mirror::readInfo(sync.root(), kServerId).revision, built.revision);

    sync.start(sessionBase(), kServerId, QStringLiteral("DESKTOP-TEST"));
    QVERIFY(ready.wait(10000));
    QCOMPARE(downloading.count(), 1);
    QVERIFY(!ready.last().at(1).toBool());
}

void TestMirror::anUpdateKeepsWhereTheDeviceStopped()
{
    seedSource();
    serve(snapshot());

    MirrorSync sync(&m_network);
    sync.setRoot(m_dir->filePath(QStringLiteral("servers")));
    QSignalSpy ready(&sync, &MirrorSync::ready);
    sync.start(sessionBase(), kServerId, QStringLiteral("DESKTOP-TEST"));
    QVERIFY(ready.wait(10000));
    const QString path = ready.first().at(0).toString();

    {
        Database mirror;
        QVERIFY(mirror.open(path));
        QVERIFY(run(mirror, QStringLiteral(
            "INSERT INTO playback_state (file_id, position_seconds, duration_seconds, watched,"
            " last_played) VALUES (2, 1234, 7000, 0, 9)")));
    }

    QVERIFY(run(*m_source, QStringLiteral("UPDATE media SET title = 'Alien (1979)' WHERE id = 11")));
    m_server.reset();
    const LibrarySnapshotResult updated = snapshot();
    QVERIFY(updated.ok);
    serve(updated);

    sync.start(sessionBase(), kServerId, QStringLiteral("DESKTOP-TEST"));
    QVERIFY(ready.wait(10000));
    QVERIFY(ready.last().at(1).toBool());
    QCOMPARE(Mirror::readInfo(sync.root(), kServerId).revision, updated.revision);
    QCOMPARE(positionIn(path, 2), 1234.0);
}

void TestMirror::aNewerLibraryIsRefused()
{
    seedSource();
    serve(snapshot(), Database::targetSchemaVersion() + 1);

    MirrorSync sync(&m_network);
    sync.setRoot(m_dir->filePath(QStringLiteral("servers")));
    QSignalSpy failed(&sync, &MirrorSync::failed);
    sync.start(sessionBase(), kServerId, QStringLiteral("DESKTOP-TEST"));
    QVERIFY(failed.wait(10000));
    QVERIFY(failed.first().at(1).toBool());
    QVERIFY(!QFile::exists(Mirror::libraryPath(sync.root(), kServerId)));
}

void TestMirror::aDamagedDownloadLeavesTheOldCopy()
{
    seedSource();
    const LibrarySnapshotResult built = snapshot();
    serve(built);

    MirrorSync sync(&m_network);
    sync.setRoot(m_dir->filePath(QStringLiteral("servers")));
    QSignalSpy ready(&sync, &MirrorSync::ready);
    QSignalSpy failed(&sync, &MirrorSync::failed);
    sync.start(sessionBase(), kServerId, QStringLiteral("DESKTOP-TEST"));
    QVERIFY(ready.wait(10000));
    const QString path = ready.first().at(0).toString();
    const QByteArray before = [&path]() {
        QFile file(path);
        file.open(QIODevice::ReadOnly);
        return file.readAll();
    }();

    m_server.reset();
    serve(built, 0, QStringLiteral("ffffffffffffffffffffffffffffffffffffffff"));
    sync.start(sessionBase(), kServerId, QStringLiteral("DESKTOP-TEST"));
    QVERIFY(failed.wait(10000));
    QVERIFY(!failed.first().at(1).toBool());

    QFile after(path);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QVERIFY(after.readAll() == before);
    QVERIFY(!QFile::exists(path + QStringLiteral(".download")));
    QCOMPARE(Mirror::readInfo(sync.root(), kServerId).revision, built.revision);
}

QTEST_GUILESS_MAIN(TestMirror)
#include "tst_mirror.moc"
