#include <QtTest>

#include <QDateTime>
#include <QScopedPointer>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

#include <algorithm>

#include "Data/Database.h"
#include "Data/FileRepository.h"
#include "TextFold.h"

namespace {

MediaFileInfo scanned(const QString &name, qint64 sizeBytes, qint64 modifiedAt)
{
    MediaFileInfo info;
    info.handle = QStringLiteral("F:/Media/") + name;
    info.parentHandle = QStringLiteral("F:/Media");
    info.displayName = name;
    info.sizeBytes = sizeBytes;
    info.modified = QDateTime::fromSecsSinceEpoch(modifiedAt);
    return info;
}

MediaFileInfo scannedIn(const QString &parent, const QString &name, qint64 sizeBytes)
{
    MediaFileInfo info;
    info.handle = parent.endsWith(QLatin1Char('/'))
        ? parent + name
        : parent + QLatin1Char('/') + name;
    info.parentHandle = parent;
    info.displayName = name;
    info.sizeBytes = sizeBytes;
    info.modified = QDateTime::fromSecsSinceEpoch(1000);
    return info;
}

LibraryFile aFullyPopulatedFile()
{
    LibraryFile file;
    file.id = 7;
    file.folderId = 1;
    file.handle = QStringLiteral("F:/Media/The Wire S01E01.mkv");
    file.parentHandle = QStringLiteral("F:/Media");
    file.displayName = QStringLiteral("The Wire S01E01.mkv");
    file.sizeBytes = 1024;
    file.modified = QDateTime::fromSecsSinceEpoch(1000);
    file.durationSeconds = 3600.0;
    file.container = QStringLiteral("matroska");
    file.videoCodec = QStringLiteral("h264");
    file.audioCodec = QStringLiteral("ac3");
    file.width = 1920;
    file.height = 1080;
    file.hdr = false;
    file.audioTrackCount = 2;
    file.subtitleTrackCount = 3;
    file.matchAttempted = true;
    file.missing = false;

    file.playback.fileId = 7;
    file.playback.positionSeconds = 120.0;
    file.playback.durationSeconds = 3600.0;
    file.playback.watchedSeconds = 115.0;
    file.playback.watched = false;
    file.playback.lastPlayed = QDateTime::fromSecsSinceEpoch(2000);

    file.matchedTitle = QStringLiteral("The Wire");
    file.matchedPosterPath = QStringLiteral("/poster.jpg");
    file.matchedBackdropPath = QStringLiteral("/backdrop.jpg");
    file.episodeStillPath = QStringLiteral("/still.jpg");
    file.matchedKind = QStringLiteral("tv");
    file.matchedYear = 2002;
    file.matchSuggested = false;
    file.episodeTitle = QStringLiteral("The Target");
    file.season = 1;
    file.episode = 1;

    return file;
}

}

class TestFileRepository : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void upsertBatchInsertsThenUpdatesWithoutDuplicating();
    void anUnchangedFileIsSkipped();
    void aChangedFileIsAskedAboutAgain();
    void aChangedFileIsProbedAgain();
    void aFileThatCameBackIsWrittenAgain();

    void aRemovedFileStaysOutOfTheNextScan();
    void aRemovedFileIsNeverMarkedMissing();
    void aFilePutBackIsAddedByTheNextScan();
    void removedFilesAreListedWithTheirFolder();

    void copiesOfAFilmAreItsConfirmedFilesOnDisk();

    void markMissingOutsideFlagsOnlyWhatIsGone();
    void markMissingOutsideSaysNothingTheSecondTime();
    void anEmptyScanMarksNothingMissing();
    void aMissingFileLeavesTheListsButNotItsFolder();

    void neverAskedFollowsTheMatchMarker();
    void neverProbedNeedsNoMarkerAndNoDuration();

    void unmatchedAgreesWithItsCount();
    void handlesWithPosterListsOnlyRealArt();
    void continueWatchingAgreesWithItsCount();
    void progressWithNoTimestampIsInNeitherTheListNorTheCount();
    void aGlanceAtATitleIsNotContinueWatching();
    void aFileThatMovedKeepsItsRow();
    void twoFilesThatLookAlikeAreNotSettled();

    void searchIgnoresCaseAndSkipsWhatIsGone();
    void searchIgnoresAccentsBothWays();
    void searchFindsTheTitleAndTheEpisodeName();
    void searchingWhatIsLeftOverSkipsAnythingMatched();
    void anEpisodeIsItsOwnResultOnlyWhenTheShowIsNotTheMatch();
    void summaryUnderAddsUpOnlyItsOwnPrefix();
    void summariesBelowCountEachChildFolderOnce();
    void summariesBelowWorkFromADriveRoot();
    void filesInReturnsAFolderItsOwnFiles();
    void byHandleAndByIdReadTheSameRow();
    void byIdsReadsTheRowsAskedForAndNothingElse();
    void byIdsReadsPastOneChunk();

    void aBatchSaysWhatItWroteAndKeepsTheIndexCurrent();
    void byHandlesReadsTheRowsNamedAndNothingElse();
    void probeFieldsAndFailuresAreReadByHandle();
    void matchCandidatesLeaveOutWhatIsPinnedLinkedOrAsked();
    void aFileIsWrittenWithItsFoldedName();

    void anEpisodeRowFillsTheSeasonWithoutDuplicatingTheFile();

    void equalityNoticesEveryField();

private:
    bool exec(const QString &statement);
    qint64 mediaWithPoster(const QString &title, const QString &posterPath);
    bool link(qint64 fileId, qint64 mediaId);
    bool play(qint64 fileId, double position, bool watched, qint64 lastPlayed);

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_database;
    QScopedPointer<FileRepository> m_files;
};

void TestFileRepository::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));

    m_database.reset(new Database);
    QVERIFY(m_database->open(m_dir->path() + QStringLiteral("/library.sqlite")));

    QVERIFY(exec(QStringLiteral(
        "INSERT INTO folders (id, handle, display_name, added)"
        " VALUES (1, 'F:/Media', 'Media', 1)")));

    m_files.reset(new FileRepository(*m_database));
}

void TestFileRepository::cleanup()
{
    m_files.reset();
    m_database.reset();
    m_dir.reset();
}

bool TestFileRepository::exec(const QString &statement)
{
    QSqlQuery query(m_database->handle());
    return query.exec(statement);
}

qint64 TestFileRepository::mediaWithPoster(const QString &title,
                                           const QString &posterPath)
{
    static int nextTmdbId = 100;

    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO media (tmdb_id, kind, title, title_key, poster_path)"
        " VALUES (:tmdb, 'tv', :title, :title_key, :poster)"));
    query.bindValue(QStringLiteral(":tmdb"), ++nextTmdbId);
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":title_key"), TextFold::key(title));
    query.bindValue(QStringLiteral(":poster"), posterPath);

    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool TestFileRepository::link(qint64 fileId, qint64 mediaId)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO file_media (file_id, media_id, confidence, suggested)"
        " VALUES (:file, :media, 1.0, 0)"));
    query.bindValue(QStringLiteral(":file"), fileId);
    query.bindValue(QStringLiteral(":media"), mediaId);
    return query.exec();
}

bool TestFileRepository::play(qint64 fileId, double position, bool watched,
                              qint64 lastPlayed)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO playback_state (file_id, position_seconds,"
        "                            duration_seconds, watched, last_played)"
        " VALUES (:file, :position, 3600, :watched, :played)"));
    query.bindValue(QStringLiteral(":file"), fileId);
    query.bindValue(QStringLiteral(":position"), position);
    query.bindValue(QStringLiteral(":watched"), watched ? 1 : 0);
    query.bindValue(QStringLiteral(":played"),
                    lastPlayed > 0 ? QVariant(lastPlayed) : QVariant());
    return query.exec();
}

void TestFileRepository::upsertBatchInsertsThenUpdatesWithoutDuplicating()
{
    const QList<MediaFileInfo> first = {
        scanned(QStringLiteral("a.mkv"), 100, 1000),
        scanned(QStringLiteral("b.mkv"), 200, 1000),
        scanned(QStringLiteral("c.mkv"), 300, 1000),
    };

    QCOMPARE(m_files->upsertBatch(1, first), 3);
    QCOMPARE(m_files->count(), 3);

    const QList<MediaFileInfo> second = {
        scanned(QStringLiteral("a.mkv"), 100, 1000),
        scanned(QStringLiteral("b.mkv"), 999, 1000),
        scanned(QStringLiteral("c.mkv"), 300, 1000),
    };

    QCOMPARE(m_files->upsertBatch(1, second), 1);
    QCOMPARE(m_files->count(), 3);

    const LibraryFile grown =
        m_files->byHandle(QStringLiteral("F:/Media/b.mkv"));
    QVERIFY(grown.isValid());
    QCOMPARE(grown.sizeBytes, Q_INT64_C(999));
}

void TestFileRepository::anUnchangedFileIsSkipped()
{
    const QList<MediaFileInfo> batch = {
        scanned(QStringLiteral("a.mkv"), 100, 1000),
        scanned(QStringLiteral("b.mkv"), 200, 2000),
    };

    QCOMPARE(m_files->upsertBatch(1, batch), 2);
    QCOMPARE(m_files->upsertBatch(1, batch), 0);
    QCOMPARE(m_files->upsertBatch(1, QList<MediaFileInfo>()), 0);
    QCOMPARE(m_files->count(), 2);
}

void TestFileRepository::aChangedFileIsAskedAboutAgain()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000)}), 1);

    const LibraryFile before =
        m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    QVERIFY(m_files->markMatchAttempted(before.id));
    QVERIFY(m_files->byId(before.id).matchAttempted);
    QCOMPARE(m_files->neverAsked().size(), 0);

    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 2000)}), 1);

    QVERIFY(!m_files->byId(before.id).matchAttempted);
    QCOMPARE(m_files->neverAsked().size(), 1);
}

void TestFileRepository::aChangedFileIsProbedAgain()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000)}), 1);

    const LibraryFile before =
        m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));

    QVERIFY(m_files->updateProbeInfo(before.id, 3600.0,
                                     QStringLiteral("matroska"),
                                     QStringLiteral("h264"),
                                     QStringLiteral("ac3"),
                                     1920, 1080, true, 2, 3));
    QVERIFY(m_files->markProbeAttempted(before.id));
    QCOMPARE(m_files->neverProbed().size(), 0);
    QCOMPARE(m_files->byId(before.id).durationSeconds, 3600.0);

    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 4000, 1000)}), 1);

    QCOMPARE(m_files->neverProbed().size(), 1);
    QCOMPARE(m_files->neverProbed().first().id, before.id);
    QCOMPARE(m_files->byId(before.id).durationSeconds, 0.0);
}

void TestFileRepository::aFileThatCameBackIsWrittenAgain()
{
    const MediaFileInfo file = scanned(QStringLiteral("a.mkv"), 100, 1000);
    const MediaFileInfo other = scanned(QStringLiteral("b.mkv"), 200, 1000);
    QCOMPARE(m_files->upsertBatch(1, {file, other}), 2);

    QCOMPARE(m_files->markMissingOutside(1, {other}), 1);
    QCOMPARE(m_files->count(), 1);

    QCOMPARE(m_files->upsertBatch(1, {file, other}), 1);
    QCOMPARE(m_files->count(), 2);
    QVERIFY(!m_files->byHandle(file.handle).missing);
}

void TestFileRepository::aRemovedFileStaysOutOfTheNextScan()
{
    const MediaFileInfo sample = scanned(QStringLiteral("sample.mkv"), 100, 1000);
    const MediaFileInfo film = scanned(QStringLiteral("film.mkv"), 200, 1000);
    QCOMPARE(m_files->upsertBatch(1, {sample, film}), 2);

    const LibraryFile removed = m_files->byHandle(sample.handle);
    QCOMPARE(m_files->removeFromLibrary({removed}, {}).size(), 1);
    QVERIFY(!m_files->byHandle(sample.handle).isValid());
    QCOMPARE(m_files->count(), 1);

    ScanIndex index = m_files->scanIndex(1);
    const BatchWrite batch = m_files->upsertBatch(1, {sample, film}, index);
    QCOMPARE(batch.written(), 0);
    QCOMPARE(batch.skipped, 2);
    QVERIFY(!m_files->byHandle(sample.handle).isValid());

    const MediaFileInfo grown = scanned(QStringLiteral("sample.mkv"), 999, 5000);
    QCOMPARE(m_files->upsertBatch(1, {grown}), 0);
    QVERIFY(!m_files->byHandle(sample.handle).isValid());
}

void TestFileRepository::aRemovedFileIsNeverMarkedMissing()
{
    const MediaFileInfo sample = scanned(QStringLiteral("sample.mkv"), 100, 1000);
    const MediaFileInfo film = scanned(QStringLiteral("film.mkv"), 200, 1000);
    QCOMPARE(m_files->upsertBatch(1, {sample, film}), 2);
    QCOMPARE(m_files->removeFromLibrary({m_files->byHandle(sample.handle)}, {}).size(), 1);

    ScanIndex index = m_files->scanIndex(1);
    const QStringList marked =
        m_files->markMissingOutside(1, QSet<QString>{film.handle}, index);
    QVERIFY(marked.isEmpty());
    QCOMPARE(m_files->count(), 1);
}

void TestFileRepository::aFilePutBackIsAddedByTheNextScan()
{
    const MediaFileInfo sample = scanned(QStringLiteral("sample.mkv"), 100, 1000);
    QCOMPARE(m_files->upsertBatch(1, {sample}), 1);
    QCOMPARE(m_files->removeFromLibrary({m_files->byHandle(sample.handle)}, {}).size(), 1);
    QCOMPARE(m_files->upsertBatch(1, {sample}), 0);

    QCOMPARE(m_files->restoreRemoved({sample.handle}), QList<qint64>({1}));
    QVERIFY(m_files->removedFiles().isEmpty());
    QCOMPARE(m_files->restoreRemoved({sample.handle}), QList<qint64>());

    ScanIndex index = m_files->scanIndex(1);
    const BatchWrite batch = m_files->upsertBatch(1, {sample}, index);
    QCOMPARE(batch.inserted, QStringList({sample.handle}));
    QVERIFY(m_files->byHandle(sample.handle).isValid());
}

void TestFileRepository::removedFilesAreListedWithTheirFolder()
{
    const MediaFileInfo a = scanned(QStringLiteral("a.mkv"), 100, 1000);
    const MediaFileInfo b = scanned(QStringLiteral("b.mkv"), 200, 1000);
    QCOMPARE(m_files->upsertBatch(1, {a, b}), 2);

    const QHash<QString, QString> folders = {
        { QStringLiteral("F:/Media"), QStringLiteral("F:\\Media") }
    };
    const QList<RemovedFileRecord> removed = m_files->removeFromLibrary(
        {m_files->byHandle(a.handle), m_files->byHandle(b.handle)}, folders);
    QCOMPARE(removed.size(), 2);

    const QList<RemovedFileRecord> listed = m_files->removedFiles();
    QCOMPARE(listed.size(), 2);
    QCOMPARE(listed.at(0).displayName, QStringLiteral("a.mkv"));
    QCOMPARE(listed.at(0).folder, QStringLiteral("F:\\Media"));
    QCOMPARE(listed.at(0).folderId, Q_INT64_C(1));
    QCOMPARE(listed.at(1).displayName, QStringLiteral("b.mkv"));
}

void TestFileRepository::copiesOfAFilmAreItsConfirmedFilesOnDisk()
{
    const MediaFileInfo remux = scanned(QStringLiteral("Heat 1995 Remux.mkv"), 900, 1000);
    const MediaFileInfo rip = scanned(QStringLiteral("Heat.1995.YIFY.mp4"), 100, 1000);
    const MediaFileInfo guess = scanned(QStringLiteral("heat-trailer.mp4"), 10, 1000);
    const MediaFileInfo gone = scanned(QStringLiteral("Heat old.avi"), 50, 1000);
    const MediaFileInfo other = scanned(QStringLiteral("Alien 1979.mkv"), 800, 1000);
    QCOMPARE(m_files->upsertBatch(1, {remux, rip, guess, gone, other}), 5);

    QVERIFY(exec(QStringLiteral(
        "INSERT INTO media (id, tmdb_id, kind, title, title_key)"
        " VALUES (50, 949, 'movie', 'Heat', 'heat'),"
        "        (51, 348, 'movie', 'Alien', 'alien')")));

    const auto idOf = [this](const MediaFileInfo &info) {
        return m_files->byHandle(info.handle).id;
    };
    const auto linkTo = [this](qint64 fileId, qint64 mediaId, bool suggested) {
        QSqlQuery query(m_database->handle());
        query.prepare(QStringLiteral(
            "INSERT INTO file_media (file_id, media_id, confidence, suggested)"
            " VALUES (:file, :media, 1.0, :suggested)"));
        query.bindValue(QStringLiteral(":file"), fileId);
        query.bindValue(QStringLiteral(":media"), mediaId);
        query.bindValue(QStringLiteral(":suggested"), suggested ? 1 : 0);
        return query.exec();
    };

    QVERIFY(linkTo(idOf(remux), 50, false));
    QVERIFY(linkTo(idOf(rip), 50, false));
    QVERIFY(linkTo(idOf(guess), 50, true));
    QVERIFY(linkTo(idOf(gone), 50, false));
    QVERIFY(linkTo(idOf(other), 51, false));

    QVERIFY(m_files->updateProbeInfo(idOf(remux), 6000.0, QStringLiteral("matroska"),
                                     QStringLiteral("hevc"), QStringLiteral("dts"),
                                     3840, 1600, true, 1, 1));
    QVERIFY(m_files->updateProbeInfo(idOf(rip), 6000.0, QStringLiteral("mp4"),
                                     QStringLiteral("h264"), QStringLiteral("aac"),
                                     1920, 800, false, 1, 0));
    QCOMPARE(m_files->markMissingOutside(1, {remux, rip, guess, other}), 1);

    const QList<LibraryFile> fromRip = m_files->copiesOfFilm(rip.handle);
    QCOMPARE(fromRip.size(), 2);
    QCOMPARE(fromRip.at(0).handle, remux.handle);
    QCOMPARE(fromRip.at(1).handle, rip.handle);

    QCOMPARE(m_files->copiesOfFilm(remux.handle).size(), 2);
    QCOMPARE(m_files->copiesOfFilm(other.handle).size(), 1);
    QVERIFY(m_files->copiesOfFilm(guess.handle).isEmpty());
}

void TestFileRepository::markMissingOutsideFlagsOnlyWhatIsGone()
{
    const QList<MediaFileInfo> batch = {
        scanned(QStringLiteral("a.mkv"), 100, 1000),
        scanned(QStringLiteral("b.mkv"), 200, 1000),
        scanned(QStringLiteral("c.mkv"), 300, 1000),
    };
    QCOMPARE(m_files->upsertBatch(1, batch), 3);

    const QList<MediaFileInfo> stillThere = {
        scanned(QStringLiteral("a.mkv"), 100, 1000),
        scanned(QStringLiteral("c.mkv"), 300, 1000),
    };

    QCOMPARE(m_files->markMissingOutside(1, stillThere), 1);

    QVERIFY(!m_files->byHandle(QStringLiteral("F:/Media/a.mkv")).missing);
    QVERIFY(m_files->byHandle(QStringLiteral("F:/Media/b.mkv")).missing);
    QVERIFY(!m_files->byHandle(QStringLiteral("F:/Media/c.mkv")).missing);
}

void TestFileRepository::markMissingOutsideSaysNothingTheSecondTime()
{
    const QList<MediaFileInfo> stillThere = {scanned(QStringLiteral("a.mkv"), 100, 1000)};
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                     scanned(QStringLiteral("b.mkv"), 200, 1000)}), 2);

    QCOMPARE(m_files->markMissingOutside(1, stillThere), 1);
    QCOMPARE(m_files->markMissingOutside(1, stillThere), 0);
}

void TestFileRepository::anEmptyScanMarksNothingMissing()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                     scanned(QStringLiteral("b.mkv"), 200, 1000)}), 2);

    QCOMPARE(m_files->markMissingOutside(1, QList<MediaFileInfo>()), 0);

    QCOMPARE(m_files->all().size(), 2);
    QCOMPARE(m_files->count(), 2);
}

void TestFileRepository::aMissingFileLeavesTheListsButNotItsFolder()
{
    const QList<MediaFileInfo> batch = {
        scanned(QStringLiteral("a.mkv"), 100, 1000),
        scanned(QStringLiteral("b.mkv"), 200, 1000),
    };
    QCOMPARE(m_files->upsertBatch(1, batch), 2);
    QCOMPARE(m_files->markMissingOutside(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000)}), 1);

    QCOMPARE(m_files->all().size(), 1);
    QCOMPARE(m_files->count(), 1);
    QCOMPARE(m_files->unmatchedCount(), 1);
    QCOMPARE(m_files->withParentHandle(QStringLiteral("F:/Media")).size(), 1);
    QCOMPARE(m_files->neverAsked().size(), 1);

    QCOMPARE(m_files->inFolder(1).size(), 2);
}

void TestFileRepository::neverAskedFollowsTheMatchMarker()
{
    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                                      scanned(QStringLiteral("b.mkv"), 200, 1000)}),
             2);

    QCOMPARE(m_files->neverAsked().size(), 2);

    const LibraryFile first = m_files->neverAsked().first();
    QVERIFY(m_files->markMatchAttempted(first.id));
    QCOMPARE(m_files->neverAsked().size(), 1);

    QVERIFY(m_files->clearMatchAttempts());
    QCOMPARE(m_files->neverAsked().size(), 2);
}

void TestFileRepository::neverProbedNeedsNoMarkerAndNoDuration()
{
    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                                      scanned(QStringLiteral("b.mkv"), 200, 1000)}),
             2);

    QCOMPARE(m_files->neverProbed().size(), 2);

    const LibraryFile a = m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    const LibraryFile b = m_files->byHandle(QStringLiteral("F:/Media/b.mkv"));

    QVERIFY(m_files->updateProbeInfo(a.id, 3600.0, QStringLiteral("matroska"),
                                     QStringLiteral("h264"),
                                     QStringLiteral("ac3"),
                                     1920, 1080, true, 2, 3));
    QCOMPARE(m_files->neverProbed().size(), 1);

    const LibraryFile probed = m_files->byId(a.id);
    QCOMPARE(probed.durationSeconds, 3600.0);
    QCOMPARE(probed.container, QStringLiteral("matroska"));
    QCOMPARE(probed.width, 1920);
    QVERIFY(probed.hdr);
    QCOMPARE(probed.audioTrackCount, 2);
    QCOMPARE(probed.subtitleTrackCount, 3);

    QVERIFY(m_files->markProbeAttempted(b.id));
    QCOMPARE(m_files->neverProbed().size(), 0);

    QVERIFY(m_files->clearProbeAttempts());
    QCOMPARE(m_files->neverProbed().size(), 1);
    QCOMPARE(m_files->neverProbed().first().id, b.id);
}

void TestFileRepository::unmatchedAgreesWithItsCount()
{
    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                                      scanned(QStringLiteral("b.mkv"), 200, 1000),
                                      scanned(QStringLiteral("c.mkv"), 300, 1000)}),
             3);

    QCOMPARE(m_files->unmatched().size(), 3);
    QCOMPARE(m_files->unmatchedCount(), 3);

    const qint64 mediaId =
        mediaWithPoster(QStringLiteral("The Wire"), QStringLiteral("/p.jpg"));
    QVERIFY(mediaId > 0);
    QVERIFY(link(m_files->byHandle(QStringLiteral("F:/Media/b.mkv")).id, mediaId));

    QCOMPARE(m_files->unmatched().size(), 2);
    QCOMPARE(m_files->unmatchedCount(), 2);

    for (const LibraryFile &file : m_files->unmatched()) {
        QVERIFY(!file.isMatched());
    }
}

void TestFileRepository::handlesWithPosterListsOnlyRealArt()
{
    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                                      scanned(QStringLiteral("b.mkv"), 200, 1000),
                                      scanned(QStringLiteral("c.mkv"), 300, 1000)}),
             3);

    const qint64 withArt =
        mediaWithPoster(QStringLiteral("The Wire"), QStringLiteral("/p.jpg"));
    const qint64 withoutArt =
        mediaWithPoster(QStringLiteral("Treme"), QString());

    QVERIFY(link(m_files->byHandle(QStringLiteral("F:/Media/a.mkv")).id, withArt));
    QVERIFY(link(m_files->byHandle(QStringLiteral("F:/Media/b.mkv")).id, withoutArt));

    QCOMPARE(m_files->handlesWithPoster(),
             QStringList({QStringLiteral("F:/Media/a.mkv")}));
}

void TestFileRepository::continueWatchingAgreesWithItsCount()
{
    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                                      scanned(QStringLiteral("b.mkv"), 200, 1000),
                                      scanned(QStringLiteral("c.mkv"), 300, 1000)}),
             3);

    const LibraryFile a = m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    const LibraryFile b = m_files->byHandle(QStringLiteral("F:/Media/b.mkv"));
    const LibraryFile c = m_files->byHandle(QStringLiteral("F:/Media/c.mkv"));

    QVERIFY(play(a.id, 120.0, false, 5000));
    QVERIFY(play(b.id, 3500.0, true, 6000));
    QVERIFY(play(c.id, 60.0, false, 7000));

    const QList<LibraryFile> resumable = m_files->continueWatching(10);
    QCOMPARE(resumable.size(), 2);
    QCOMPARE(resumable.first().handle, c.handle);
    QCOMPARE(m_files->continueWatchingCount(), 2);

    QCOMPARE(m_files->continueWatching(1).size(), 1);

    const LibraryFile resumed = m_files->byId(a.id);
    QVERIFY(resumed.playback.isValid());
    QCOMPARE(resumed.playback.positionSeconds, 120.0);
    QVERIFY(resumed.playback.isPartial());
    QVERIFY(!m_files->byId(c.id).playback.watched);
}

void TestFileRepository::progressWithNoTimestampIsInNeitherTheListNorTheCount()
{
    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                                      scanned(QStringLiteral("b.mkv"), 200, 1000)}),
             2);

    const LibraryFile a = m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    const LibraryFile b = m_files->byHandle(QStringLiteral("F:/Media/b.mkv"));

    QVERIFY(play(a.id, 120.0, false, 5000));
    QVERIFY(play(b.id, 120.0, false, 0));

    QCOMPARE(m_files->continueWatching(10).size(), 1);
    QCOMPARE(m_files->continueWatchingCount(), 1);
}

void TestFileRepository::aGlanceAtATitleIsNotContinueWatching()
{
    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                                      scanned(QStringLiteral("b.mkv"), 200, 1000),
                                      scanned(QStringLiteral("c.mkv"), 300, 1000),
                                      scanned(QStringLiteral("d.mkv"), 400, 1000)}),
             4);

    const LibraryFile a = m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    const LibraryFile b = m_files->byHandle(QStringLiteral("F:/Media/b.mkv"));
    const LibraryFile c = m_files->byHandle(QStringLiteral("F:/Media/c.mkv"));
    const LibraryFile d = m_files->byHandle(QStringLiteral("F:/Media/d.mkv"));

    QVERIFY(play(a.id, 5.0, false, 5000));
    QVERIFY(play(b.id, 30.0, false, 6000));
    QVERIFY(play(c.id, 31.0, false, 7000));
    QVERIFY(play(d.id, 3500.0, false, 8000));

    const QList<LibraryFile> resumable = m_files->continueWatching(10);
    QCOMPARE(resumable.size(), 1);
    QCOMPARE(resumable.first().handle, c.handle);
    QCOMPARE(m_files->continueWatchingCount(), 1);
}

void TestFileRepository::aFileThatMovedKeepsItsRow()
{
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO folders (id, handle, display_name, added)"
        " VALUES (2, 'G:/Films', 'Films', 1)")));

    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("film.mkv"), 5000, 1234)}), 1);
    const LibraryFile before = m_files->byHandle(QStringLiteral("F:/Media/film.mkv"));
    QVERIFY(before.id > 0);
    QVERIFY(play(before.id, 900.0, false, 5000));

    MediaFileInfo moved = scanned(QStringLiteral("film.mkv"), 5000, 1234);
    moved.handle = QStringLiteral("G:/Films/film.mkv");
    moved.parentHandle = QStringLiteral("G:/Films");
    QCOMPARE(m_files->upsertBatch(2, {moved}), 1);

    QVERIFY(exec(QStringLiteral("UPDATE files SET missing = 1 WHERE id = %1")
                     .arg(before.id)));

    const QList<QPair<QString, QString>> settled = m_files->settleMoves(
        {QStringLiteral("F:/Media/film.mkv")}, {QStringLiteral("G:/Films/film.mkv")});

    QCOMPARE(settled.size(), 1);
    QCOMPARE(settled.first().first, QStringLiteral("F:/Media/film.mkv"));
    QCOMPARE(settled.first().second, QStringLiteral("G:/Films/film.mkv"));

    const LibraryFile after = m_files->byHandle(QStringLiteral("G:/Films/film.mkv"));
    QCOMPARE(after.id, before.id);
    QCOMPARE(after.folderId, qint64(2));
    QCOMPARE(after.parentHandle, QStringLiteral("G:/Films"));
    QVERIFY(!after.missing);
    QCOMPARE(after.playback.positionSeconds, 900.0);

    QVERIFY(!m_files->byHandle(QStringLiteral("F:/Media/film.mkv")).isValid());
}

void TestFileRepository::twoFilesThatLookAlikeAreNotSettled()
{
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO folders (id, handle, display_name, added)"
        " VALUES (2, 'G:/Films', 'Films', 1)")));

    QCOMPARE(m_files->upsertBatch(1, {scanned(QStringLiteral("film.mkv"), 5000, 1234),
                                      scannedIn(QStringLiteral("F:/Media/two"),
                                                QStringLiteral("film.mkv"), 5000)}),
             2);
    QVERIFY(exec(QStringLiteral("UPDATE files SET modified = 1234, missing = 1")));

    MediaFileInfo moved = scanned(QStringLiteral("film.mkv"), 5000, 1234);
    moved.handle = QStringLiteral("G:/Films/film.mkv");
    moved.parentHandle = QStringLiteral("G:/Films");
    QCOMPARE(m_files->upsertBatch(2, {moved}), 1);

    QVERIFY(m_files->settleMoves({QStringLiteral("F:/Media/film.mkv"),
                                  QStringLiteral("F:/Media/two/film.mkv")},
                                 {QStringLiteral("G:/Films/film.mkv")})
                .isEmpty());
}

void TestFileRepository::searchIgnoresCaseAndSkipsWhatIsGone()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("The Wire S01E01.mkv"), 100, 1000),
                     scanned(QStringLiteral("the wire s01e02.mkv"), 200, 1000),
                     scanned(QStringLiteral("Treme S01E01.mkv"), 300, 1000)}),
             3);

    QCOMPARE(m_files->search(QStringLiteral("WIRE"), 10).size(), 2);
    QCOMPARE(m_files->search(QStringLiteral("  wire  "), 10).size(), 2);
    QCOMPARE(m_files->search(QStringLiteral("wire"), 1).size(), 1);
    QCOMPARE(m_files->search(QString(), 10).size(), 0);
    QCOMPARE(m_files->search(QStringLiteral("   "), 10).size(), 0);
    QCOMPARE(m_files->search(QStringLiteral("nothing here"), 10).size(), 0);

    QCOMPARE(m_files->markMissingOutside(
                 1, {scanned(QStringLiteral("The Wire S01E01.mkv"), 100, 1000)}),
             2);
    QCOMPARE(m_files->search(QStringLiteral("wire"), 10).size(), 1);
}

void TestFileRepository::searchIgnoresAccentsBothWays()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("Žene.S01E01.mkv"), 100, 1000),
                     scanned(QStringLiteral("Đorđe i Zmaj.mkv"), 200, 1000),
                     scanned(QStringLiteral("Zemlja.mkv"), 300, 1000)}),
             3);

    for (const QString &needle : {QStringLiteral("žene"), QStringLiteral("ŽENE"),
                                  QStringLiteral("zene")}) {
        const QList<LibraryFile> hits = m_files->search(needle, 10);
        QCOMPARE(hits.size(), 1);
        QCOMPARE(hits.first().displayName, QStringLiteral("Žene.S01E01.mkv"));
    }

    QCOMPARE(m_files->search(QStringLiteral("djordje"), 10).size(), 1);
    QCOMPARE(m_files->search(QStringLiteral("DJORDJE"), 10).size(), 1);
    QCOMPARE(m_files->search(QStringLiteral("đorđe"), 10).size(), 1);

    const QList<LibraryFile> allZ = m_files->search(QStringLiteral("z"), 10);
    QCOMPARE(allZ.size(), 3);
    QCOMPARE(allZ.at(0).displayName, QStringLiteral("Đorđe i Zmaj.mkv"));
    QCOMPARE(allZ.at(1).displayName, QStringLiteral("Zemlja.mkv"));
    QCOMPARE(allZ.at(2).displayName, QStringLiteral("Žene.S01E01.mkv"));

    QCOMPARE(m_files->search(QStringLiteral("z"), 0).size(), 0);
}

void TestFileRepository::searchFindsTheTitleAndTheEpisodeName()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("E02.mp4"), 200, 1000),
                     scanned(QStringLiteral("E01 Rites of Passage.mp4"), 100, 1000),
                     scanned(QStringLiteral("Treme S01E01.mkv"), 300, 1000)}),
             3);

    const qint64 vikings =
        mediaWithPoster(QStringLiteral("Vikings"), QStringLiteral("/vikings.jpg"));
    QVERIFY(vikings > 0);

    const LibraryFile first =
        m_files->byHandle(QStringLiteral("F:/Media/E01 Rites of Passage.mp4"));
    const LibraryFile second = m_files->byHandle(QStringLiteral("F:/Media/E02.mp4"));
    QVERIFY(link(first.id, vikings));
    QVERIFY(link(second.id, vikings));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO episodes (media_id, file_id, season, episode, title, title_key)"
        " VALUES"
        " (%1, %2, 1, 1, 'Rites of Passage', 'rites of passage'),"
        " (%1, %3, 1, 2, 'Wrath of the Northmen', 'wrath of the northmen')")
        .arg(vikings).arg(first.id).arg(second.id)));

    const QList<LibraryFile> byShow = m_files->search(QStringLiteral("vikings"), 10);
    QCOMPARE(byShow.size(), 2);
    QCOMPARE(byShow.at(0).id, first.id);
    QCOMPARE(byShow.at(1).id, second.id);

    const QList<LibraryFile> byEpisode =
        m_files->search(QStringLiteral("northmen"), 10);
    QCOMPARE(byEpisode.size(), 1);
    QCOMPARE(byEpisode.first().id, second.id);

    QCOMPARE(m_files->search(QStringLiteral("rites"), 10).size(), 1);
    QCOMPARE(m_files->search(QStringLiteral("treme"), 10).size(), 1);
    QCOMPARE(m_files->search(QStringLiteral("e0"), 10).size(), 3);
}

void TestFileRepository::searchingWhatIsLeftOverSkipsAnythingMatched()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("Vikings.S01E01.mkv"), 100, 1000),
                     scanned(QStringLiteral("Vikings.sample.mkv"), 200, 1000),
                     scanned(QStringLiteral("Treme S01E01.mkv"), 300, 1000)}),
             3);

    const qint64 vikings =
        mediaWithPoster(QStringLiteral("Vikings"), QStringLiteral("/vikings.jpg"));
    QVERIFY(vikings > 0);

    const LibraryFile episode =
        m_files->byHandle(QStringLiteral("F:/Media/Vikings.S01E01.mkv"));
    QVERIFY(link(episode.id, vikings));

    const QList<LibraryFile> loose =
        m_files->searchUnmatched(QStringLiteral("vikings"), 10);
    QCOMPARE(loose.size(), 1);
    QCOMPARE(loose.first().displayName, QStringLiteral("Vikings.sample.mkv"));

    QCOMPARE(m_files->search(QStringLiteral("vikings"), 10).size(), 2);

    QCOMPARE(m_files->searchUnmatched(QString(), 10).size(), 0);
    QCOMPARE(m_files->searchUnmatched(QStringLiteral("vikings"), 0).size(), 0);

    QCOMPARE(m_files->markMissingOutside(
                 1, {scanned(QStringLiteral("Vikings.S01E01.mkv"), 100, 1000)}),
             2);
    QCOMPARE(m_files->searchUnmatched(QStringLiteral("vikings"), 10).size(), 0);
}

void TestFileRepository::anEpisodeIsItsOwnResultOnlyWhenTheShowIsNotTheMatch()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("Vikings.S01E01.mkv"), 100, 1000),
                     scanned(QStringLiteral("Vikings.S02E01.mkv"), 200, 1000)}),
             2);

    const qint64 vikings =
        mediaWithPoster(QStringLiteral("Vikings"), QStringLiteral("/vikings.jpg"));
    QVERIFY(vikings > 0);

    const LibraryFile first =
        m_files->byHandle(QStringLiteral("F:/Media/Vikings.S01E01.mkv"));
    const LibraryFile second =
        m_files->byHandle(QStringLiteral("F:/Media/Vikings.S02E01.mkv"));
    QVERIFY(link(first.id, vikings));
    QVERIFY(link(second.id, vikings));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO episodes (media_id, file_id, season, episode, title, title_key)"
        " VALUES"
        " (%1, %2, 1, 1, 'Rites of Passage', 'rites of passage'),"
        " (%1, %3, 2, 1, 'Brother''s War', 'brothers war')")
        .arg(vikings).arg(first.id).arg(second.id)));

    QCOMPARE(m_files->searchEpisodes(QStringLiteral("vikings"), 10).size(), 0);

    const QList<LibraryFile> byEpisode =
        m_files->searchEpisodes(QStringLiteral("rites"), 10);
    QCOMPARE(byEpisode.size(), 1);
    QCOMPARE(byEpisode.first().id, first.id);

    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("Rites of Spring.mkv"), 300, 1000)}),
             1);
    QCOMPARE(m_files->searchEpisodes(QStringLiteral("rites"), 10).size(), 1);

    QCOMPARE(m_files->searchEpisodes(QString(), 10).size(), 0);
    QCOMPARE(m_files->searchEpisodes(QStringLiteral("rites"), 0).size(), 0);
}

void TestFileRepository::summaryUnderAddsUpOnlyItsOwnPrefix()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("Shows/a.mkv"), 100, 1000),
                     scanned(QStringLiteral("Shows/b.mkv"), 200, 1000),
                     scanned(QStringLiteral("Films/c.mkv"), 400, 1000)}),
             3);

    const FolderSummary shows =
        m_files->summaryUnder(QStringLiteral("F:/Media/Shows/"));
    QCOMPARE(shows.fileCount, 2);
    QCOMPARE(shows.totalBytes, Q_INT64_C(300));

    const FolderSummary everything =
        m_files->summaryUnder(QStringLiteral("F:/Media/"));
    QCOMPARE(everything.fileCount, 3);
    QCOMPARE(everything.totalBytes, Q_INT64_C(700));

    const FolderSummary nowhere =
        m_files->summaryUnder(QStringLiteral("F:/Elsewhere/"));
    QCOMPARE(nowhere.fileCount, 0);
    QCOMPARE(nowhere.totalBytes, Q_INT64_C(0));

    QCOMPARE(m_files->markMissingOutside(
                 1, {scanned(QStringLiteral("Shows/a.mkv"), 100, 1000)}), 2);
    QCOMPARE(m_files->summaryUnder(QStringLiteral("F:/Media/Shows/")).fileCount, 1);
}

void TestFileRepository::summariesBelowCountEachChildFolderOnce()
{
    const QList<MediaFileInfo> files = {
        scannedIn(QStringLiteral("F:/Media/Shows"), QStringLiteral("a.mkv"), 100),
        scannedIn(QStringLiteral("F:/Media/Shows/Season 1"), QStringLiteral("b.mkv"), 200),
        scannedIn(QStringLiteral("F:/Media/Shows HD"), QStringLiteral("c.mkv"), 400),
        scannedIn(QStringLiteral("F:/Media/Films"), QStringLiteral("d.mkv"), 800),
        scannedIn(QStringLiteral("F:/Media"), QStringLiteral("top.mkv"), 1600),
        scannedIn(QStringLiteral("F:/Elsewhere"), QStringLiteral("e.mkv"), 3200),
    };
    QCOMPARE(m_files->upsertBatch(1, files), 6);

    QHash<QString, FolderSummary> below =
        m_files->summariesBelow(QStringLiteral("F:/Media"));
    QCOMPARE(below.size(), 3);
    QCOMPARE(below.value(QStringLiteral("F:/Media/Shows")).fileCount, 2);
    QCOMPARE(below.value(QStringLiteral("F:/Media/Shows")).totalBytes, Q_INT64_C(300));
    QCOMPARE(below.value(QStringLiteral("F:/Media/Shows HD")).fileCount, 1);
    QCOMPARE(below.value(QStringLiteral("F:/Media/Shows HD")).totalBytes, Q_INT64_C(400));
    QCOMPARE(below.value(QStringLiteral("F:/Media/Films")).fileCount, 1);
    QVERIFY(!below.contains(QStringLiteral("F:/Media")));
    QVERIFY(!below.contains(QStringLiteral("F:/Elsewhere")));

    const QHash<QString, FolderSummary> withSlash =
        m_files->summariesBelow(QStringLiteral("F:/Media/"));
    QCOMPARE(withSlash.size(), 3);
    QCOMPARE(withSlash.value(QStringLiteral("F:/Media/Shows")).fileCount, 2);

    const QHash<QString, FolderSummary> shows =
        m_files->summariesBelow(QStringLiteral("F:/Media/Shows"));
    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.value(QStringLiteral("F:/Media/Shows/Season 1")).totalBytes,
             Q_INT64_C(200));
    QVERIFY(!shows.contains(QStringLiteral("F:/Media/Shows HD")));

    QVERIFY(m_files->summariesBelow(QString()).isEmpty());
    QVERIFY(m_files->summariesBelow(QStringLiteral("F:/Nowhere")).isEmpty());

    QCOMPARE(m_files->markMissingOutside(1, files.mid(1)), 1);
    below = m_files->summariesBelow(QStringLiteral("F:/Media"));
    QCOMPARE(below.value(QStringLiteral("F:/Media/Shows")).fileCount, 1);
    QCOMPARE(below.value(QStringLiteral("F:/Media/Shows")).totalBytes, Q_INT64_C(200));
}

void TestFileRepository::summariesBelowWorkFromADriveRoot()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scannedIn(QStringLiteral("F:/"), QStringLiteral("a.mkv"), 100),
                     scannedIn(QStringLiteral("F:/Media"), QStringLiteral("b.mkv"), 200),
                     scannedIn(QStringLiteral("F:/Media/Deep"), QStringLiteral("c.mkv"), 400),
                     scannedIn(QStringLiteral("G:/Other"), QStringLiteral("d.mkv"), 800)}),
             4);

    const QHash<QString, FolderSummary> below =
        m_files->summariesBelow(QStringLiteral("F:/"));
    QCOMPARE(below.size(), 1);
    QCOMPARE(below.value(QStringLiteral("F:/Media")).fileCount, 2);
    QCOMPARE(below.value(QStringLiteral("F:/Media")).totalBytes, Q_INT64_C(600));
}

void TestFileRepository::filesInReturnsAFolderItsOwnFiles()
{
    const QList<MediaFileInfo> files = {
        scannedIn(QStringLiteral("F:/Media/Shows"), QStringLiteral("a.mkv"), 100),
        scannedIn(QStringLiteral("F:/Media/Shows"), QStringLiteral("b.mkv"), 200),
        scannedIn(QStringLiteral("F:/Media/Shows/Season 1"), QStringLiteral("c.mkv"), 400),
        scannedIn(QStringLiteral("F:/Media/Shows HD"), QStringLiteral("d.mkv"), 800),
    };
    QCOMPARE(m_files->upsertBatch(1, files), 4);

    QHash<QString, LibraryFile> inShows =
        m_files->filesIn(QStringLiteral("F:/Media/Shows"));
    QCOMPARE(inShows.size(), 2);
    QCOMPARE(inShows.value(QStringLiteral("F:/Media/Shows/a.mkv")).sizeBytes,
             Q_INT64_C(100));
    QCOMPARE(inShows.value(QStringLiteral("F:/Media/Shows/b.mkv")).displayName,
             QStringLiteral("b.mkv"));
    QVERIFY(inShows.value(QStringLiteral("F:/Media/Shows/a.mkv"))
            == m_files->byHandle(QStringLiteral("F:/Media/Shows/a.mkv")));

    QCOMPARE(m_files->markMissingOutside(
                 1, {files.at(0), files.at(2), files.at(3)}), 1);
    inShows = m_files->filesIn(QStringLiteral("F:/Media/Shows"));
    QCOMPARE(inShows.size(), 2);
    QVERIFY(inShows.value(QStringLiteral("F:/Media/Shows/b.mkv")).missing);

    QVERIFY(m_files->filesIn(QStringLiteral("F:/Nowhere")).isEmpty());
}

void TestFileRepository::byHandleAndByIdReadTheSameRow()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000)}), 1);

    const LibraryFile byHandle =
        m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    QVERIFY(byHandle.isValid());
    QCOMPARE(byHandle.folderId, Q_INT64_C(1));
    QCOMPARE(byHandle.displayName, QStringLiteral("a.mkv"));
    QCOMPARE(byHandle.sizeBytes, Q_INT64_C(100));
    QCOMPARE(byHandle.modified, QDateTime::fromSecsSinceEpoch(1000));

    const LibraryFile byId = m_files->byId(byHandle.id);
    QVERIFY(byId == byHandle);

    QVERIFY(!m_files->byHandle(QStringLiteral("F:/Media/nothing.mkv")).isValid());
    QVERIFY(!m_files->byId(9999).isValid());
}

void TestFileRepository::byIdsReadsTheRowsAskedForAndNothingElse()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                     scanned(QStringLiteral("b.mkv"), 200, 1000),
                     scanned(QStringLiteral("c.mkv"), 300, 1000)}), 3);

    const LibraryFile a = m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    const LibraryFile c = m_files->byHandle(QStringLiteral("F:/Media/c.mkv"));
    QVERIFY(a.isValid());
    QVERIFY(c.isValid());

    QList<LibraryFile> read = m_files->byIds({c.id, a.id, 9999});
    QCOMPARE(read.size(), 2);
    std::sort(read.begin(), read.end(),
              [](const LibraryFile &left, const LibraryFile &right) {
        return left.id < right.id;
    });
    QVERIFY(read.at(0) == a);
    QVERIFY(read.at(1) == c);

    QVERIFY(m_files->byIds({}).isEmpty());
}

void TestFileRepository::byIdsReadsPastOneChunk()
{
    QList<MediaFileInfo> batch;
    for (int i = 0; i < 1200; ++i) {
        batch.append(scanned(QStringLiteral("file%1.mkv").arg(i), 100, 1000));
    }
    QCOMPARE(m_files->upsertBatch(1, batch), 1200);

    QList<qint64> ids;
    const QList<LibraryFile> all = m_files->all();
    for (const LibraryFile &file : all) {
        ids.append(file.id);
    }
    QCOMPARE(ids.size(), 1200);

    QCOMPARE(m_files->byIds(ids).size(), 1200);
}

void TestFileRepository::aBatchSaysWhatItWroteAndKeepsTheIndexCurrent()
{
    const QString a = QStringLiteral("F:/Media/a.mkv");
    const QString b = QStringLiteral("F:/Media/b.mkv");
    const QString c = QStringLiteral("F:/Media/c.mkv");

    ScanIndex index = m_files->scanIndex(1);
    QVERIFY(index.isEmpty());

    BatchWrite first = m_files->upsertBatch(
        1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
            scanned(QStringLiteral("b.mkv"), 200, 1000)}, index);
    QCOMPARE(first.inserted, (QStringList{a, b}));
    QVERIFY(first.changed.isEmpty());
    QCOMPARE(first.written(), 2);
    QCOMPARE(index.size(), 2);

    BatchWrite second = m_files->upsertBatch(
        1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
            scanned(QStringLiteral("b.mkv"), 250, 1000),
            scanned(QStringLiteral("c.mkv"), 300, 1000)}, index);
    QCOMPARE(second.skipped, 1);
    QCOMPARE(second.changed, QStringList{b});
    QCOMPARE(second.inserted, QStringList{c});
    QCOMPARE(second.handles(), (QStringList{c, b}));

    const QStringList gone = m_files->markMissingOutside(1, QSet<QString>{b, c}, index);
    QCOMPARE(gone, QStringList{a});
    QVERIFY(index.value(a).missing);
    QVERIFY(m_files->byHandle(a).missing);

    BatchWrite third = m_files->upsertBatch(
        1, {scanned(QStringLiteral("a.mkv"), 100, 1000)}, index);
    QCOMPARE(third.revived, QStringList{a});
    QVERIFY(!index.value(a).missing);
    QVERIFY(!m_files->byHandle(a).missing);

    QCOMPARE(m_files->markMissingOutside(1, QSet<QString>{a, b, c}, index).size(), 0);
    QVERIFY(m_files->markMissingOutside(1, QSet<QString>(), index).isEmpty());
}

void TestFileRepository::byHandlesReadsTheRowsNamedAndNothingElse()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                     scanned(QStringLiteral("b.mkv"), 200, 1000),
                     scanned(QStringLiteral("c.mkv"), 300, 1000)}), 3);

    QList<LibraryFile> read = m_files->byHandles({QStringLiteral("F:/Media/c.mkv"),
                                                  QStringLiteral("F:/Media/a.mkv"),
                                                  QStringLiteral("F:/Media/nope.mkv")});
    QCOMPARE(read.size(), 2);
    std::sort(read.begin(), read.end(),
              [](const LibraryFile &left, const LibraryFile &right) {
        return left.handle < right.handle;
    });
    QVERIFY(read.at(0) == m_files->byHandle(QStringLiteral("F:/Media/a.mkv")));
    QVERIFY(read.at(1) == m_files->byHandle(QStringLiteral("F:/Media/c.mkv")));

    QVERIFY(m_files->byHandles({}).isEmpty());
}

void TestFileRepository::probeFieldsAndFailuresAreReadByHandle()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000),
                     scanned(QStringLiteral("b.mkv"), 200, 1000)}), 2);

    const LibraryFile a = m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    QVERIFY(m_files->updateProbeInfo(a.id, 3600.0, QStringLiteral("matroska"),
                                     QStringLiteral("hevc"), QStringLiteral("eac3"),
                                     3840, 2160, true, 2, 5));

    const LibraryFile probed = m_files->probeFieldsFor(QStringLiteral("F:/Media/a.mkv"));
    QVERIFY(probed.isValid());
    QCOMPARE(probed.id, a.id);
    QCOMPARE(probed.durationSeconds, 3600.0);
    QCOMPARE(probed.container, QStringLiteral("matroska"));
    QCOMPARE(probed.videoCodec, QStringLiteral("hevc"));
    QCOMPARE(probed.audioCodec, QStringLiteral("eac3"));
    QCOMPARE(probed.width, 3840);
    QCOMPARE(probed.height, 2160);
    QVERIFY(probed.hdr);
    QCOMPARE(probed.audioTrackCount, 2);
    QCOMPARE(probed.subtitleTrackCount, 5);
    QVERIFY(!m_files->probeFieldsFor(QStringLiteral("F:/Media/nope.mkv")).isValid());

    QVERIFY(m_files->failedProbeHandles({1}).isEmpty());
    QVERIFY(m_files->markProbeAttemptedFor(QStringLiteral("F:/Media/a.mkv")));
    QVERIFY(m_files->markProbeAttemptedFor(QStringLiteral("F:/Media/b.mkv")));
    QCOMPARE(m_files->failedProbeHandles({1}),
             QStringList{QStringLiteral("F:/Media/b.mkv")});
    QVERIFY(m_files->failedProbeHandles({}).isEmpty());
    QVERIFY(m_files->failedProbeHandles({2}).isEmpty());
}

void TestFileRepository::matchCandidatesLeaveOutWhatIsPinnedLinkedOrAsked()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("plain.mkv"), 100, 1000),
                     scanned(QStringLiteral("linked.mkv"), 200, 1000),
                     scanned(QStringLiteral("asked.mkv"), 300, 1000),
                     scanned(QStringLiteral("pinned.mkv"), 400, 1000),
                     scanned(QStringLiteral("gone.mkv"), 500, 1000)}), 5);

    const LibraryFile plain = m_files->byHandle(QStringLiteral("F:/Media/plain.mkv"));
    const LibraryFile linked = m_files->byHandle(QStringLiteral("F:/Media/linked.mkv"));
    const LibraryFile asked = m_files->byHandle(QStringLiteral("F:/Media/asked.mkv"));
    const LibraryFile gone = m_files->byHandle(QStringLiteral("F:/Media/gone.mkv"));

    const qint64 mediaId = mediaWithPoster(QStringLiteral("Heat"), QStringLiteral("/h.jpg"));
    QVERIFY(link(linked.id, mediaId));
    QVERIFY(m_files->markMatchAttempted(asked.id));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO match_overrides (file_handle, tmdb_id, kind, pinned_at)"
        " VALUES ('F:/Media/pinned.mkv', 949, 'movie', 1)")));
    QVERIFY(exec(QStringLiteral(
        "UPDATE files SET missing = 1 WHERE handle = 'F:/Media/gone.mkv'")));

    const auto handlesOf = [](const QList<LibraryFile> &files) {
        QStringList handles;
        for (const LibraryFile &file : files) {
            handles.append(file.handle);
        }
        return handles;
    };

    const QList<LibraryFile> unasked =
        m_files->matchCandidates(FileRepository::MatchScope::Unasked);
    QCOMPARE(handlesOf(unasked), QStringList{plain.handle});
    QCOMPARE(unasked.first().id, plain.id);
    QCOMPARE(unasked.first().displayName, QStringLiteral("plain.mkv"));
    QCOMPARE(unasked.first().parentHandle, QStringLiteral("F:/Media"));

    QCOMPARE(handlesOf(m_files->matchCandidates(FileRepository::MatchScope::Unpinned)),
             (QStringList{plain.handle, linked.handle, asked.handle}));

    QCOMPARE(handlesOf(m_files->matchCandidates(
                 FileRepository::MatchScope::Unasked,
                 {linked.handle, plain.handle, QStringLiteral("F:/Media/nope.mkv")})),
             QStringList{plain.handle});
    QCOMPARE(handlesOf(m_files->matchCandidates(
                 FileRepository::MatchScope::Unpinned,
                 {QStringLiteral("F:/Media/pinned.mkv"), asked.handle})),
             QStringList{asked.handle});

    const LibraryFile row = m_files->plainById(asked.id);
    QCOMPARE(row.handle, asked.handle);
    QCOMPARE(row.displayName, QStringLiteral("asked.mkv"));
    QVERIFY(row.matchAttempted);
    QVERIFY(!row.missing);
    QVERIFY(m_files->plainById(gone.id).missing);
    QVERIFY(!m_files->plainById(9999).isValid());
}

void TestFileRepository::aFileIsWrittenWithItsFoldedName()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("Žene.S01E01.mkv"), 100, 1000)}), 1);

    QSqlQuery key(m_database->handle());
    QVERIFY(key.exec(QStringLiteral("SELECT name_key FROM files")));
    QVERIFY(key.next());
    QCOMPARE(key.value(0).toString(), QStringLiteral("zene.s01e01.mkv"));
    key.finish();

    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("Žene.S01E01.mkv"), 150, 1000)}), 1);
    QVERIFY(key.exec(QStringLiteral("SELECT COUNT(*) FROM files WHERE name_key = 'zene.s01e01.mkv'")));
    QVERIFY(key.next());
    QCOMPARE(key.value(0).toInt(), 1);
}

void TestFileRepository::anEpisodeRowFillsTheSeasonWithoutDuplicatingTheFile()
{
    QCOMPARE(m_files->upsertBatch(
                 1, {scanned(QStringLiteral("a.mkv"), 100, 1000)}), 1);

    const LibraryFile file = m_files->byHandle(QStringLiteral("F:/Media/a.mkv"));
    const qint64 mediaId =
        mediaWithPoster(QStringLiteral("The Wire"), QStringLiteral("/p.jpg"));
    QVERIFY(link(file.id, mediaId));

    QSqlQuery episode(m_database->handle());
    episode.prepare(QStringLiteral(
        "INSERT INTO episodes (media_id, file_id, season, episode, title,"
        "                      still_path)"
        " VALUES (:media, :file, 1, 1, 'The Target', '/still.jpg')"));
    episode.bindValue(QStringLiteral(":media"), mediaId);
    episode.bindValue(QStringLiteral(":file"), file.id);
    QVERIFY(episode.exec());

    const QList<LibraryFile> all = m_files->all();
    QCOMPARE(all.size(), 1);

    const LibraryFile joined = all.first();
    QVERIFY(joined.isMatched());
    QVERIFY(joined.isEpisode());
    QCOMPARE(joined.matchedTitle, QStringLiteral("The Wire"));
    QCOMPARE(joined.matchedKind, QStringLiteral("tv"));
    QCOMPARE(joined.season, 1);
    QCOMPARE(joined.episode, 1);
    QCOMPARE(joined.episodeTitle, QStringLiteral("The Target"));
    QCOMPARE(joined.episodeStillPath, QStringLiteral("/still.jpg"));
    QVERIFY(!joined.matchSuggested);
}

void TestFileRepository::equalityNoticesEveryField()
{
    const LibraryFile base = aFullyPopulatedFile();

    LibraryFile same = aFullyPopulatedFile();
    QVERIFY(base == same);
    QVERIFY(!(base != same));

    LibraryFile other = base;
    other.id = 8;
    QVERIFY(base != other);

    other = base;
    other.folderId = 2;
    QVERIFY(base != other);

    other = base;
    other.handle = QStringLiteral("F:/Media/other.mkv");
    QVERIFY(base != other);

    other = base;
    other.parentHandle = QStringLiteral("F:/Other");
    QVERIFY(base != other);

    other = base;
    other.displayName = QStringLiteral("other.mkv");
    QVERIFY(base != other);

    other = base;
    other.sizeBytes = 2048;
    QVERIFY(base != other);

    other = base;
    other.modified = QDateTime::fromSecsSinceEpoch(1001);
    QVERIFY(base != other);

    other = base;
    other.durationSeconds = 3601.0;
    QVERIFY(base != other);

    other = base;
    other.container = QStringLiteral("mp4");
    QVERIFY(base != other);

    other = base;
    other.videoCodec = QStringLiteral("hevc");
    QVERIFY(base != other);

    other = base;
    other.audioCodec = QStringLiteral("eac3");
    QVERIFY(base != other);

    other = base;
    other.width = 1280;
    QVERIFY(base != other);

    other = base;
    other.height = 720;
    QVERIFY(base != other);

    other = base;
    other.hdr = true;
    QVERIFY(base != other);

    other = base;
    other.audioTrackCount = 1;
    QVERIFY(base != other);

    other = base;
    other.subtitleTrackCount = 0;
    QVERIFY(base != other);

    other = base;
    other.matchAttempted = false;
    QVERIFY(base != other);

    other = base;
    other.missing = true;
    QVERIFY(base != other);

    other = base;
    other.playback.positionSeconds = 121.0;
    QVERIFY(base != other);

    other = base;
    other.playback.durationSeconds = 3599.0;
    QVERIFY(base != other);

    other = base;
    other.playback.watchedSeconds = 116.0;
    QVERIFY(base != other);

    other = base;
    other.playback.watched = true;
    QVERIFY(base != other);

    other = base;
    other.playback.lastPlayed = QDateTime::fromSecsSinceEpoch(2001);
    QVERIFY(base != other);

    other = base;
    other.playback.fileId = 8;
    QVERIFY(base != other);

    other = base;
    other.matchedTitle = QStringLiteral("Treme");
    QVERIFY(base != other);

    other = base;
    other.matchedPosterPath = QStringLiteral("/other.jpg");
    QVERIFY(base != other);

    other = base;
    other.matchedBackdropPath = QStringLiteral("/other.jpg");
    QVERIFY(base != other);

    other = base;
    other.episodeStillPath = QStringLiteral("/other.jpg");
    QVERIFY(base != other);

    other = base;
    other.matchedKind = QStringLiteral("movie");
    QVERIFY(base != other);

    other = base;
    other.matchedYear = 2003;
    QVERIFY(base != other);

    other = base;
    other.matchSuggested = true;
    QVERIFY(base != other);

    other = base;
    other.episodeTitle = QStringLiteral("The Detail");
    QVERIFY(base != other);

    other = base;
    other.season = 2;
    QVERIFY(base != other);

    other = base;
    other.episode = 2;
    QVERIFY(base != other);
}

QTEST_GUILESS_MAIN(TestFileRepository)

#include "tst_filerepository.moc"
