#include <QtTest>

#include <QScopedPointer>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

#include "Data/Database.h"
#include "Data/PlaybackState.h"
#include "Data/PlaybackStateRepository.h"

class TestPlaybackStateRepository : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aSeasonIsMarkedWatchedInOneGo();
    void unwatchingTogetherClearsEveryPosition();
    void oneBadFileInTheBatchChangesNothing();
    void anEmptyBatchWritesNothing();
    void oneFileStillWorksOnItsOwn();

    void markingAnUnplayedFileWatchedKeepsItsLength();
    void aSaveBeforeTheLengthIsKnownKeepsTheStoredOne();
    void aFirstSaveWithNoLengthTakesTheFilesOwn();

    void resumesOnlyWhenThereIsSomethingToResume_data();
    void resumesOnlyWhenThereIsSomethingToResume();

    void aReplacedFilmTakesOverTheHistoryOfTheOneItReplaced();
    void aShowDoesNotHandOneEpisodesPositionToAllTheOthers();
    void anEpisodeWithNoNumberLeftKeepsItsPositionToItself();
    void aReplacementThatHasBeenWatchedIsLeftAlone();

private:
    qint64 addFile(const QString &name);
    bool startWatching(qint64 fileId, double position);
    bool setFileLength(qint64 fileId, double seconds);
    bool addTitle(qint64 mediaId, const QString &kind);
    bool linkFile(qint64 fileId, qint64 mediaId);
    bool markMissing(qint64 fileId);
    bool addEpisode(qint64 mediaId, qint64 fileId, int season, int episode);
    PlaybackState stateOf(qint64 fileId) const;

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_database;
    QScopedPointer<PlaybackStateRepository> m_playback;
};

void TestPlaybackStateRepository::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));

    m_database.reset(new Database);
    QVERIFY(m_database->open(m_dir->path() + QStringLiteral("/library.sqlite")));

    QSqlQuery folder(m_database->handle());
    QVERIFY(folder.exec(QStringLiteral(
        "INSERT INTO folders (id, handle, display_name, added)"
        " VALUES (1, 'F:/Media', 'Media', 1)")));

    m_playback.reset(new PlaybackStateRepository(*m_database));
}

void TestPlaybackStateRepository::cleanup()
{
    m_playback.reset();
    m_database.reset();
    m_dir.reset();
}

qint64 TestPlaybackStateRepository::addFile(const QString &name)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO files (folder_id, handle, display_name, added)"
        " VALUES (1, :handle, :name, 1)"));
    query.bindValue(QStringLiteral(":handle"), QStringLiteral("F:/Media/") + name);
    query.bindValue(QStringLiteral(":name"), name);

    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool TestPlaybackStateRepository::addTitle(qint64 mediaId, const QString &kind)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO media (id, tmdb_id, kind, title) VALUES (:id, :id, :kind, 'Title')"));
    query.bindValue(QStringLiteral(":id"), mediaId);
    query.bindValue(QStringLiteral(":kind"), kind);
    return query.exec();
}

bool TestPlaybackStateRepository::linkFile(qint64 fileId, qint64 mediaId)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO file_media (file_id, media_id, confidence, suggested)"
        " VALUES (:file, :media, 1.0, 0)"));
    query.bindValue(QStringLiteral(":file"), fileId);
    query.bindValue(QStringLiteral(":media"), mediaId);
    return query.exec();
}

bool TestPlaybackStateRepository::markMissing(qint64 fileId)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral("UPDATE files SET missing = 1 WHERE id = :file"));
    query.bindValue(QStringLiteral(":file"), fileId);
    return query.exec();
}

bool TestPlaybackStateRepository::addEpisode(qint64 mediaId, qint64 fileId,
                                             int season, int episode)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO episodes (media_id, file_id, season, episode)"
        " VALUES (:media, :file, :season, :episode)"));
    query.bindValue(QStringLiteral(":media"), mediaId);
    query.bindValue(QStringLiteral(":file"), fileId);
    query.bindValue(QStringLiteral(":season"), season);
    query.bindValue(QStringLiteral(":episode"), episode);
    return query.exec();
}

PlaybackState TestPlaybackStateRepository::stateOf(qint64 fileId) const
{
    return m_playback->forFile(fileId);
}

bool TestPlaybackStateRepository::startWatching(qint64 fileId, double position)
{
    PlaybackState state;
    state.fileId = fileId;
    state.positionSeconds = position;
    state.durationSeconds = 3600.0;
    return m_playback->save(state);
}

void TestPlaybackStateRepository::aSeasonIsMarkedWatchedInOneGo()
{
    const QList<qint64> season = {addFile(QStringLiteral("e01.mkv")),
                                  addFile(QStringLiteral("e02.mkv")),
                                  addFile(QStringLiteral("e03.mkv"))};
    for (const qint64 id : season) {
        QVERIFY(id > 0);
    }

    QVERIFY(m_playback->setWatched(season, true));

    for (const qint64 id : season) {
        const PlaybackState state = m_playback->forFile(id);
        QVERIFY(state.isValid());
        QVERIFY(state.watched);
    }
}

void TestPlaybackStateRepository::unwatchingTogetherClearsEveryPosition()
{
    const QList<qint64> season = {addFile(QStringLiteral("e01.mkv")),
                                  addFile(QStringLiteral("e02.mkv")),
                                  addFile(QStringLiteral("e03.mkv"))};
    for (const qint64 id : season) {
        QVERIFY(startWatching(id, 600.0));
    }

    QVERIFY(m_playback->setWatched(season, true));
    QCOMPARE(m_playback->forFile(season.first()).positionSeconds, 600.0);

    QVERIFY(m_playback->setWatched(season, false));

    for (const qint64 id : season) {
        const PlaybackState state = m_playback->forFile(id);
        QVERIFY(!state.watched);
        QCOMPARE(state.positionSeconds, 0.0);
        QCOMPARE(state.watchedSeconds, 0.0);
    }
}

void TestPlaybackStateRepository::oneBadFileInTheBatchChangesNothing()
{
    const qint64 first = addFile(QStringLiteral("e01.mkv"));
    const qint64 last = addFile(QStringLiteral("e03.mkv"));
    const qint64 noSuchFile = 9999;

    QVERIFY(!m_playback->setWatched(QList<qint64>{first, noSuchFile, last}, true));

    QVERIFY(!m_playback->forFile(first).isValid());
    QVERIFY(!m_playback->forFile(last).isValid());
}

void TestPlaybackStateRepository::anEmptyBatchWritesNothing()
{
    QVERIFY(m_playback->setWatched(QList<qint64>(), true));

    QSqlQuery count(m_database->handle());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM playback_state")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 0);
}

void TestPlaybackStateRepository::oneFileStillWorksOnItsOwn()
{
    const qint64 id = addFile(QStringLiteral("film.mkv"));
    QVERIFY(startWatching(id, 600.0));

    QVERIFY(m_playback->setWatched(id, true));
    QVERIFY(m_playback->forFile(id).watched);

    QVERIFY(m_playback->setWatched(id, false));
    QVERIFY(!m_playback->forFile(id).watched);
    QCOMPARE(m_playback->forFile(id).positionSeconds, 0.0);
}

bool TestPlaybackStateRepository::setFileLength(qint64 fileId, double seconds)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "UPDATE files SET duration_seconds = :seconds WHERE id = :id"));
    query.bindValue(QStringLiteral(":seconds"), seconds);
    query.bindValue(QStringLiteral(":id"), fileId);
    return query.exec();
}

void TestPlaybackStateRepository::markingAnUnplayedFileWatchedKeepsItsLength()
{
    const qint64 probed = addFile(QStringLiteral("probed.mkv"));
    const qint64 unprobed = addFile(QStringLiteral("unprobed.mkv"));
    QVERIFY(setFileLength(probed, 2700.0));

    QVERIFY(m_playback->setWatched(QList<qint64>{probed, unprobed}, true));

    const PlaybackState state = m_playback->forFile(probed);
    QVERIFY(state.watched);
    QCOMPARE(state.durationSeconds, 2700.0);

    QVERIFY(m_playback->forFile(unprobed).watched);
    QCOMPARE(m_playback->forFile(unprobed).durationSeconds, 0.0);

    QVERIFY(setFileLength(unprobed, 1500.0));
    QVERIFY(m_playback->setWatched(unprobed, true));
    QCOMPARE(m_playback->forFile(unprobed).durationSeconds, 1500.0);
}

void TestPlaybackStateRepository::aSaveBeforeTheLengthIsKnownKeepsTheStoredOne()
{
    const qint64 id = addFile(QStringLiteral("film.mkv"));
    QVERIFY(startWatching(id, 600.0));

    PlaybackState early;
    early.fileId = id;
    early.positionSeconds = 610.0;
    early.durationSeconds = 0.0;
    QVERIFY(m_playback->save(early));

    const PlaybackState state = m_playback->forFile(id);
    QCOMPARE(state.positionSeconds, 610.0);
    QCOMPARE(state.durationSeconds, 3600.0);
    QVERIFY(state.progress() > 0.0);

    PlaybackState later = early;
    later.positionSeconds = 620.0;
    later.durationSeconds = 3500.0;
    QVERIFY(m_playback->save(later));
    QCOMPARE(m_playback->forFile(id).durationSeconds, 3500.0);
}

void TestPlaybackStateRepository::aFirstSaveWithNoLengthTakesTheFilesOwn()
{
    const qint64 probed = addFile(QStringLiteral("probed.mkv"));
    const qint64 unprobed = addFile(QStringLiteral("unprobed.mkv"));
    QVERIFY(setFileLength(probed, 1800.0));

    for (const qint64 id : {probed, unprobed}) {
        PlaybackState state;
        state.fileId = id;
        state.positionSeconds = 60.0;
        QVERIFY(m_playback->save(state));
    }

    QCOMPARE(m_playback->forFile(probed).durationSeconds, 1800.0);
    QCOMPARE(m_playback->forFile(probed).positionSeconds, 60.0);
    QCOMPARE(m_playback->forFile(unprobed).durationSeconds, 0.0);
    QCOMPARE(m_playback->forFile(unprobed).positionSeconds, 60.0);
}

void TestPlaybackStateRepository::resumesOnlyWhenThereIsSomethingToResume_data()
{
    QTest::addColumn<double>("position");
    QTest::addColumn<double>("duration");
    QTest::addColumn<bool>("watched");
    QTest::addColumn<double>("expected");

    QTest::newRow("a short peek") << 23.0 << 3600.0 << false << 0.0;
    QTest::newRow("exactly thirty seconds") << 30.0 << 3600.0 << false << 0.0;
    QTest::newRow("past thirty seconds") << 45.0 << 3600.0 << false << 45.0;
    QTest::newRow("a minute into a two hour film") << 60.0 << 7200.0 << false << 60.0;
    QTest::newRow("forty seconds into a long film") << 40.0 << 5869.94 << false << 40.0;
    QTest::newRow("a short clip past halfway") << 40.0 << 64.0 << false << 40.0;
    QTest::newRow("a short clip at twenty seconds") << 20.0 << 64.0 << false << 0.0;
    QTest::newRow("nearly finished") << 3500.0 << 3600.0 << false << 0.0;
    QTest::newRow("marked watched") << 600.0 << 3600.0 << true << 0.0;
    QTest::newRow("length unknown, past thirty") << 45.0 << 0.0 << false << 45.0;
    QTest::newRow("length unknown, a peek") << 20.0 << 0.0 << false << 0.0;
}

void TestPlaybackStateRepository::resumesOnlyWhenThereIsSomethingToResume()
{
    QFETCH(double, position);
    QFETCH(double, duration);
    QFETCH(bool, watched);
    QFETCH(double, expected);

    QCOMPARE(PlaybackStateRepository::resumeSeconds(position, duration, watched), expected);
}

void TestPlaybackStateRepository::aReplacedFilmTakesOverTheHistoryOfTheOneItReplaced()
{
    QVERIFY(addTitle(1, QStringLiteral("movie")));

    const qint64 gone = addFile(QStringLiteral("film-hushrips.mkv"));
    const qint64 fresh = addFile(QStringLiteral("film-yts.mp4"));
    QVERIFY(gone > 0 && fresh > 0);
    QVERIFY(linkFile(gone, 1));
    QVERIFY(linkFile(fresh, 1));
    QVERIFY(startWatching(gone, 3155.6));
    QVERIFY(markMissing(gone));

    QCOMPARE(m_playback->mediaWithMissingHistory(), QList<qint64>({1}));
    QCOMPARE(m_playback->adoptFromMissing({1}), QList<qint64>({fresh}));

    QCOMPARE(stateOf(fresh).positionSeconds, 3155.6);
    QCOMPARE(stateOf(gone).positionSeconds, 3155.6);

    QVERIFY(m_playback->adoptFromMissing({1}).isEmpty());
}

void TestPlaybackStateRepository::aShowDoesNotHandOneEpisodesPositionToAllTheOthers()
{
    QVERIFY(addTitle(2, QStringLiteral("tv")));

    const qint64 goneFirst = addFile(QStringLiteral("show-s01e01-old.mkv"));
    const qint64 freshFirst = addFile(QStringLiteral("show-s01e01-new.mkv"));
    const qint64 second = addFile(QStringLiteral("show-s01e02.mkv"));
    const qint64 third = addFile(QStringLiteral("show-s01e03.mkv"));
    QVERIFY(goneFirst > 0 && freshFirst > 0 && second > 0 && third > 0);

    for (const qint64 fileId : {goneFirst, freshFirst, second, third}) {
        QVERIFY(linkFile(fileId, 2));
    }
    QVERIFY(addEpisode(2, freshFirst, 1, 1));
    QVERIFY(addEpisode(2, second, 1, 2));
    QVERIFY(addEpisode(2, third, 1, 3));

    QVERIFY(startWatching(goneFirst, 928.0));
    QVERIFY(markMissing(goneFirst));

    QCOMPARE(m_playback->adoptFromMissing({2}), QList<qint64>({freshFirst}));

    QCOMPARE(stateOf(freshFirst).positionSeconds, 928.0);
    QCOMPARE(stateOf(second).positionSeconds, 0.0);
    QCOMPARE(stateOf(third).positionSeconds, 0.0);
}

void TestPlaybackStateRepository::anEpisodeWithNoNumberLeftKeepsItsPositionToItself()
{
    QVERIFY(addTitle(4, QStringLiteral("tv")));

    const qint64 gone = addFile(QStringLiteral("unreadable-name.mkv"));
    const qint64 first = addFile(QStringLiteral("other-s01e01.mkv"));
    const qint64 second = addFile(QStringLiteral("other-s01e02.mkv"));
    QVERIFY(gone > 0 && first > 0 && second > 0);

    for (const qint64 fileId : {gone, first, second}) {
        QVERIFY(linkFile(fileId, 4));
    }
    QVERIFY(addEpisode(4, first, 1, 1));
    QVERIFY(addEpisode(4, second, 1, 2));

    QVERIFY(startWatching(gone, 600.0));
    QVERIFY(markMissing(gone));

    QVERIFY(m_playback->adoptFromMissing({4}).isEmpty());

    QCOMPARE(stateOf(first).positionSeconds, 0.0);
    QCOMPARE(stateOf(second).positionSeconds, 0.0);
}

void TestPlaybackStateRepository::aReplacementThatHasBeenWatchedIsLeftAlone()
{
    QVERIFY(addTitle(3, QStringLiteral("movie")));

    const qint64 gone = addFile(QStringLiteral("other-old.mkv"));
    const qint64 fresh = addFile(QStringLiteral("other-new.mkv"));
    QVERIFY(gone > 0 && fresh > 0);
    QVERIFY(linkFile(gone, 3));
    QVERIFY(linkFile(fresh, 3));

    QVERIFY(startWatching(gone, 600.0));
    QVERIFY(markMissing(gone));
    QVERIFY(startWatching(fresh, 1800.0));

    QVERIFY(m_playback->adoptFromMissing({3}).isEmpty());
    QCOMPARE(stateOf(fresh).positionSeconds, 1800.0);
}

QTEST_GUILESS_MAIN(TestPlaybackStateRepository)

#include "tst_playbackstaterepository.moc"
