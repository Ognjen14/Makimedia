#include <QtTest>

#include <QAbstractItemModel>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

#include "Data/Database.h"
#include "Data/MediaRepository.h"
#include "Library/ShowEpisodeModel.h"

namespace {

MediaRecord showNamed(const QString &title, qint64 tmdbId)
{
    MediaRecord record;
    record.tmdbId = tmdbId;
    record.kind = QStringLiteral("tv");
    record.title = title;
    record.year = 2008;
    return record;
}

EpisodeRecord episodeOf(qint64 mediaId, int season, int episode)
{
    EpisodeRecord record;
    record.mediaId = mediaId;
    record.season = season;
    record.episode = episode;
    record.title = QStringLiteral("S%1E%2").arg(season).arg(episode);
    return record;
}

}

class TestShowEpisodeModel : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aShowReadsBackItsSeasonsAndStartsOnTheFirst();
    void nextUpCarriesOnAcrossASeason();
    void nextUpIsWhatWasLeftHalfWatched();
    void aSeasonIsWatchedWhenWhatIsOnDiskIsWatched();
    void watchingOneEpisodeChangesOneRow();
    void switchingSeasonReplacesTheRows();
    void onlyChangesToThisShowConcernIt();
    void handlesCoverTheShowAndTheSeason();

private:
    qint64 addFile(const QString &displayName);
    bool watch(qint64 fileId);
    bool stopPartWay(qint64 fileId, double seconds, qint64 at);
    qint64 addEpisode(int season, int episode, bool withFile);

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_database;
    QScopedPointer<MediaRepository> m_media;
    qint64 m_showId = 0;
};

void TestShowEpisodeModel::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));

    m_database.reset(new Database);
    QVERIFY(m_database->open(m_dir->path() + QStringLiteral("/library.sqlite")));

    QSqlQuery folder(m_database->handle());
    QVERIFY(folder.exec(QStringLiteral(
        "INSERT INTO folders (handle, display_name, added)"
        " VALUES ('F:/Media', 'Media', 1)")));

    m_media.reset(new MediaRepository(*m_database));
    m_showId = m_media->upsertMedia(showNamed(QStringLiteral("Breaking Bad"), 1396));
    QVERIFY(m_showId > 0);
}

void TestShowEpisodeModel::cleanup()
{
    m_media.reset();
    m_database.reset();
    m_dir.reset();
}

qint64 TestShowEpisodeModel::addFile(const QString &displayName)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO files (folder_id, handle, display_name, added)"
        " VALUES (1, :handle, :name, 1)"));
    query.bindValue(QStringLiteral(":handle"), QStringLiteral("F:/Media/") + displayName);
    query.bindValue(QStringLiteral(":name"), displayName);
    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool TestShowEpisodeModel::watch(qint64 fileId)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO playback_state (file_id, position_seconds, duration_seconds,"
        "                            watched, last_played)"
        " VALUES (:file, 2900, 3000, 1, 1)"
        " ON CONFLICT(file_id) DO UPDATE SET watched = 1"));
    query.bindValue(QStringLiteral(":file"), fileId);
    return query.exec();
}

bool TestShowEpisodeModel::stopPartWay(qint64 fileId, double seconds, qint64 at)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO playback_state (file_id, position_seconds, duration_seconds,"
        "                            watched, last_played)"
        " VALUES (:file, :position, 3000, 0, :at)"
        " ON CONFLICT(file_id) DO UPDATE SET position_seconds = excluded.position_seconds,"
        " last_played = excluded.last_played"));
    query.bindValue(QStringLiteral(":file"), fileId);
    query.bindValue(QStringLiteral(":position"), seconds);
    query.bindValue(QStringLiteral(":at"), at);
    return query.exec();
}

qint64 TestShowEpisodeModel::addEpisode(int season, int episode, bool withFile)
{
    qint64 fileId = -1;
    if (withFile) {
        fileId = addFile(QStringLiteral("s%1e%2.mkv").arg(season).arg(episode));
        if (fileId <= 0 || !m_media->linkFile(fileId, m_showId, 1.0, false)) {
            return -1;
        }
    }
    if (!m_media->upsertEpisode(episodeOf(m_showId, season, episode), fileId)) {
        return -1;
    }
    return withFile ? fileId : 0;
}

void TestShowEpisodeModel::aShowReadsBackItsSeasonsAndStartsOnTheFirst()
{
    QVERIFY(addEpisode(2, 1, true) > 0);
    QVERIFY(addEpisode(1, 1, true) > 0);
    QVERIFY(addEpisode(1, 2, true) > 0);

    ShowEpisodeModel model(*m_media);
    model.setMediaId(m_showId);

    QCOMPARE(model.seasons(), (QVariantList{1, 2}));
    QCOMPARE(model.season(), 1);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.info().value(QStringLiteral("title")).toString(),
             QStringLiteral("Breaking Bad"));
    QCOMPARE(model.info().value(QStringLiteral("filesOnDisk")).toInt(), 3);

    const QModelIndex first = model.index(0, 0);
    QCOMPARE(model.data(first, ShowEpisodeModel::EpisodeRole).toInt(), 1);
    QCOMPARE(model.data(first, ShowEpisodeModel::HasFileRole).toBool(), true);
    QCOMPARE(model.get(1).value(QStringLiteral("episode")).toInt(), 2);
    QVERIFY(model.get(5).isEmpty());
}

void TestShowEpisodeModel::nextUpCarriesOnAcrossASeason()
{
    const qint64 first = addEpisode(1, 1, true);
    const qint64 second = addEpisode(1, 2, true);
    QVERIFY(addEpisode(2, 1, true) > 0);
    QVERIFY(watch(first));
    QVERIFY(watch(second));

    ShowEpisodeModel model(*m_media);
    model.setMediaId(m_showId);

    QVERIFY(model.hasNextUp());
    QCOMPARE(model.nextUp().value(QStringLiteral("season")).toInt(), 2);
    QCOMPARE(model.nextUp().value(QStringLiteral("episode")).toInt(), 1);
}

void TestShowEpisodeModel::nextUpIsWhatWasLeftHalfWatched()
{
    QVERIFY(addEpisode(1, 1, true) > 0);
    const qint64 second = addEpisode(1, 2, true);
    QVERIFY(second > 0);
    const qint64 sixth = addEpisode(6, 1, true);
    QVERIFY(sixth > 0);
    QVERIFY(stopPartWay(sixth, 1340, 5000));

    ShowEpisodeModel model(*m_media);
    model.setMediaId(m_showId);

    QVERIFY(model.hasNextUp());
    QCOMPARE(model.nextUp().value(QStringLiteral("season")).toInt(), 6);
    QCOMPARE(model.nextUp().value(QStringLiteral("episode")).toInt(), 1);
    QVERIFY(model.nextUp().value(QStringLiteral("resumeSeconds")).toDouble() > 0);

    QVERIFY(stopPartWay(second, 900, 9000));
    model.setMediaId(0);
    model.setMediaId(m_showId);

    QCOMPARE(model.nextUp().value(QStringLiteral("season")).toInt(), 1);
    QCOMPARE(model.nextUp().value(QStringLiteral("episode")).toInt(), 2);

    QVERIFY(watch(second));
    QVERIFY(watch(sixth));
    model.setMediaId(0);
    model.setMediaId(m_showId);

    QCOMPARE(model.nextUp().value(QStringLiteral("season")).toInt(), 1);
    QCOMPARE(model.nextUp().value(QStringLiteral("episode")).toInt(), 1);
}

void TestShowEpisodeModel::aSeasonIsWatchedWhenWhatIsOnDiskIsWatched()
{
    const qint64 onDisk = addEpisode(2, 1, true);
    QVERIFY(addEpisode(2, 2, false) == 0);
    QVERIFY(addEpisode(1, 1, true) > 0);
    QVERIFY(watch(onDisk));

    ShowEpisodeModel model(*m_media);
    model.setMediaId(m_showId);
    QVERIFY(!model.seasonWatched());
    QVERIFY(model.seasonHasFiles());

    model.setSeason(2);
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.seasonWatched());
    QVERIFY(model.seasonHasFiles());
}

void TestShowEpisodeModel::watchingOneEpisodeChangesOneRow()
{
    QVERIFY(addEpisode(1, 1, true) > 0);
    const qint64 second = addEpisode(1, 2, true);
    QVERIFY(addEpisode(1, 3, true) > 0);

    ShowEpisodeModel model(*m_media);
    model.setMediaId(m_showId);

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    QVERIFY(watch(second));
    model.reload();

    QCOMPARE(reset.count(), 0);
    QCOMPARE(changed.count(), 1);
    const QModelIndex topLeft = changed.first().at(0).value<QModelIndex>();
    const QModelIndex bottomRight = changed.first().at(1).value<QModelIndex>();
    QCOMPARE(topLeft.row(), 1);
    QCOMPARE(bottomRight.row(), 1);
    QVERIFY(model.data(model.index(1, 0), ShowEpisodeModel::WatchedRole).toBool());
}

void TestShowEpisodeModel::switchingSeasonReplacesTheRows()
{
    QVERIFY(addEpisode(1, 1, true) > 0);
    QVERIFY(addEpisode(1, 2, true) > 0);
    QVERIFY(addEpisode(2, 1, true) > 0);

    ShowEpisodeModel model(*m_media);
    model.setMediaId(m_showId);

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    model.setSeason(2);

    QCOMPARE(reset.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ShowEpisodeModel::SeasonRole).toInt(), 2);
}

void TestShowEpisodeModel::onlyChangesToThisShowConcernIt()
{
    const qint64 fileId = addEpisode(1, 1, true);
    QVERIFY(fileId > 0);
    const qint64 elsewhere = addFile(QStringLiteral("a film.mkv"));
    QVERIFY(elsewhere > 0);

    ShowEpisodeModel model(*m_media);
    QVERIFY(!model.concerns({m_showId}, {fileId}));

    model.setMediaId(m_showId);
    QVERIFY(model.concerns({m_showId}, {}));
    QVERIFY(model.concerns({}, {fileId}));
    QVERIFY(!model.concerns({m_showId + 100}, {elsewhere}));
    QVERIFY(!model.concerns({}, {}));
}

void TestShowEpisodeModel::handlesCoverTheShowAndTheSeason()
{
    QVERIFY(addEpisode(1, 1, true) > 0);
    QVERIFY(addEpisode(1, 2, false) == 0);
    QVERIFY(addEpisode(2, 1, true) > 0);

    ShowEpisodeModel model(*m_media);
    model.setMediaId(m_showId);

    QCOMPARE(model.fileHandles(),
             (QStringList{QStringLiteral("F:/Media/s1e1.mkv"),
                          QStringLiteral("F:/Media/s2e1.mkv")}));
    QCOMPARE(model.seasonFileHandles(),
             QStringList{QStringLiteral("F:/Media/s1e1.mkv")});
}

QTEST_GUILESS_MAIN(TestShowEpisodeModel)

#include "tst_showepisodemodel.moc"
