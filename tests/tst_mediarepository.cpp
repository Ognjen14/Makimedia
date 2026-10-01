#include <QtTest>

#include <QScopedPointer>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QSet>
#include <QVariant>

#include <algorithm>

#include "Data/Database.h"
#include "Data/MediaRepository.h"

namespace {

MediaRecord showNamed(const QString &title, qint64 tmdbId)
{
    MediaRecord record;
    record.tmdbId = tmdbId;
    record.kind = QStringLiteral("tv");
    record.title = title;
    record.year = 2002;
    return record;
}

EpisodeRecord episodeOf(qint64 mediaId, int season, int episode,
                        const QString &title = QString())
{
    EpisodeRecord record;
    record.mediaId = mediaId;
    record.season = season;
    record.episode = episode;
    record.title = title;
    return record;
}

}

class TestMediaRepository : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void upsertMediaIsIdempotentPerTmdbIdAndKind();
    void mediaWithoutATmdbIdIsRefused();

    void linkingAndUnlinkingAFile();
    void unlinkingAlsoReleasesTheEpisodeRow();
    void aRepinnedFileLeavesOneEpisodeRowBehind();

    void anEpisodeIsUniquePerSeasonAndNumber();
    void tmdbNeverClearsTheLinkToAFile();
    void aFileWithNoEpisodeTitleLeavesTmdbsAlone();
    void aFileNeverOverwritesATitleTmdbAlreadyGave();
    void aFilledInTitleSurvivesUntilTmdbAnswers();

    void nextEpisodeCrossesASeasonBoundary();
    void nextEpisodeSkipsWhatHasNoFile();
    void nextEpisodeStopsAtTheLastOne();

    void creditsBelongToATitleOrToOneEpisode();
    void creditsArrivingAgainReplaceWhatWasThere();
    void aTitleAskedAboutIsNeverAskedAboutAgain();

    void aFetchedSeasonSurvivesAReopen();
    void aSeasonBackdropIsItsFirstStill();
    void recentlyAddedListsTitlesNotFiles();

    void aPinnedShowReportsItsEpisodesAndItsFile();

    void aPinWritesTheTitleTheLinkAndTheOverrideTogether();
    void aFilmPinWritesNoEpisode();
    void aFailedPinLeavesNothingBehind();
    void repinningMovesTheLinkAndTheOverride();

    void aListReadsEveryColumnIntoItsField();

    void recordEqualityIncludesTheListCounts();
    void aTitleReadByIdCountsLikeTheLists();
    void titlesSortByFoldedTitleThenId();
    void clearingFindsOnlyUnpinnedLinksAndReleasesStrayEpisodes();
    void titlesAndEpisodesAreWrittenWithTheirFoldedKeys();
    void artworkPathsForNameOnlyThoseTitles();
    void everyFaceTheLibraryKnowsIsOnTheWarmingList();

    void onlyOwnedUncheckedFilmsWaitForACollectionCheck();
    void aNotedCollectionIsFetchedUntilItIsSaved();
    void savingACollectionReplacesItsFilms();
    void ownedCollectionsComeWithTheirFilms();
    void aCollectionTheUserMadeKeepsItsOrder();
    void aCollectionTakenOffThePageCanComeBack();
    void aFolderRefusedAsAShowIsRememberedUntilItIsPutBack();
    void searchingOfOneKindAnswersWithTitlesThatAreInTheLibrary();
    void searchingFindsAGenreFromItsStartAndNotFromTheMiddle();

private:
    qint64 addFile(const QString &displayName);

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_database;
    QScopedPointer<MediaRepository> m_media;
};

void TestMediaRepository::init()
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
}

void TestMediaRepository::cleanup()
{
    m_media.reset();
    m_database.reset();
    m_dir.reset();
}

qint64 TestMediaRepository::addFile(const QString &displayName)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO files (folder_id, handle, display_name, added)"
        " VALUES (1, :handle, :name, 1)"));
    query.bindValue(QStringLiteral(":handle"),
                    QStringLiteral("F:/Media/") + displayName);
    query.bindValue(QStringLiteral(":name"), displayName);

    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

void TestMediaRepository::upsertMediaIsIdempotentPerTmdbIdAndKind()
{
    const qint64 first =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    QVERIFY(first > 0);

    const qint64 again =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    QCOMPARE(again, first);

    MediaRecord film;
    film.tmdbId = 1438;
    film.kind = QStringLiteral("movie");
    film.title = QStringLiteral("Something Else");

    const qint64 other = m_media->upsertMedia(film);
    QVERIFY(other > 0);
    QVERIFY(other != first);

    QCOMPARE(m_media->mediaById(first).title, QStringLiteral("The Wire"));
    QCOMPARE(m_media->mediaByTmdbId(1438, QStringLiteral("tv")).id, first);
}

void TestMediaRepository::mediaWithoutATmdbIdIsRefused()
{
    MediaRecord nameless;
    nameless.kind = QStringLiteral("tv");
    nameless.title = QStringLiteral("No Id");

    QCOMPARE(m_media->upsertMedia(nameless), -1);

    MediaRecord kindless;
    kindless.tmdbId = 99;

    QCOMPARE(m_media->upsertMedia(kindless), -1);
}

void TestMediaRepository::linkingAndUnlinkingAFile()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));
    QVERIFY(fileId > 0);

    QVERIFY(m_media->linkFile(fileId, mediaId, 1.0, false));

    const FileMediaLink link = m_media->linkForFile(fileId);
    QVERIFY(link.isValid());
    QCOMPARE(link.mediaId, mediaId);
    QCOMPARE(link.suggested, false);
    QCOMPARE(m_media->mediaForFile(fileId).id, mediaId);

    QVERIFY(m_media->unlinkFile(fileId));
    QVERIFY(!m_media->linkForFile(fileId).isValid());
}

void TestMediaRepository::unlinkingAlsoReleasesTheEpisodeRow()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));

    QVERIFY(m_media->linkFile(fileId, mediaId, 1.0, false));
    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1), fileId));
    QCOMPARE(m_media->episodesFor(mediaId).first().fileId, fileId);
    QVERIFY(m_media->episodeForFile(fileId).hasFile());
    QCOMPARE(m_media->episodeForFile(fileId).fileId, fileId);

    QVERIFY(m_media->unlinkFile(fileId));

    QCOMPARE(m_media->episodesFor(mediaId).size(), 1);
    QVERIFY(!m_media->episodesFor(mediaId).first().hasFile());
}

void TestMediaRepository::aRepinnedFileLeavesOneEpisodeRowBehind()
{
    const qint64 wrong =
        m_media->upsertMedia(showNamed(QStringLiteral("Wrong Show"), 111));
    const qint64 right =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));

    QVERIFY(m_media->linkFile(fileId, wrong, 1.0, false));
    QVERIFY(m_media->upsertEpisode(episodeOf(wrong, 1, 1), fileId));

    QVERIFY(m_media->linkFile(fileId, right, 1.0, false));
    QVERIFY(m_media->upsertEpisode(episodeOf(right, 1, 1), fileId));

    QVERIFY(m_media->detachFileFromOtherEpisodes(fileId, right));

    QCOMPARE(m_media->episodesFor(wrong).size(), 1);
    QVERIFY(!m_media->episodesFor(wrong).first().hasFile());

    QCOMPARE(m_media->episodesFor(right).size(), 1);
    QVERIFY(m_media->episodesFor(right).first().hasFile());

    QSqlQuery holders(m_database->handle());
    QVERIFY(holders.exec(QStringLiteral(
        "SELECT COUNT(*) FROM episodes WHERE file_id IS NOT NULL")));
    QVERIFY(holders.next());
    QCOMPARE(holders.value(0).toInt(), 1);
}

void TestMediaRepository::anEpisodeIsUniquePerSeasonAndNumber()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));

    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1), fileId));
    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1), fileId));
    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 2), -1));

    QCOMPARE(m_media->episodesFor(mediaId).size(), 2);
}

void TestMediaRepository::tmdbNeverClearsTheLinkToAFile()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));

    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1), fileId));

    EpisodeRecord fromTmdb = episodeOf(mediaId, 1, 1, QStringLiteral("The Target"));
    fromTmdb.overview = QStringLiteral("McNulty watches a corner.");
    QVERIFY(m_media->overwriteEpisodeFromTmdb(fromTmdb));

    const QList<EpisodeRecord> stored = m_media->episodesFor(mediaId);
    QCOMPARE(stored.size(), 1);
    QVERIFY(stored.first().hasFile());
    QCOMPARE(stored.first().fileId, fileId);
    QCOMPARE(stored.first().title, QStringLiteral("The Target"));

    const EpisodeRecord found = m_media->episodeForFile(fileId);
    QCOMPARE(found.season, 1);
    QCOMPARE(found.episode, 1);
    QCOMPARE(found.title, QStringLiteral("The Target"));
    QVERIFY(found.hasFile());
    QCOMPARE(found.fileId, fileId);
}

void TestMediaRepository::aFileNeverOverwritesATitleTmdbAlreadyGave()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));

    QVERIFY(m_media->overwriteEpisodeFromTmdb(
        episodeOf(mediaId, 1, 1, QStringLiteral("The Target"))));

    QVERIFY(m_media->upsertEpisode(
        episodeOf(mediaId, 1, 1, QStringLiteral("Pilot Episode HDTV")), fileId));

    const EpisodeRecord stored = m_media->episodeForFile(fileId);
    QCOMPARE(stored.title, QStringLiteral("The Target"));
    QCOMPARE(stored.fileId, fileId);

    QVERIFY(m_media->overwriteEpisodeFromTmdb(
        episodeOf(mediaId, 1, 1, QStringLiteral("The Detail"))));

    QCOMPARE(m_media->episodeForFile(fileId).title, QStringLiteral("The Detail"));
}

void TestMediaRepository::aFilledInTitleSurvivesUntilTmdbAnswers()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));

    QVERIFY(m_media->upsertEpisode(
        episodeOf(mediaId, 1, 1, QStringLiteral("Guessed From The Name")),
        fileId));

    QCOMPARE(m_media->episodeForFile(fileId).title,
             QStringLiteral("Guessed From The Name"));

    QVERIFY(m_media->overwriteEpisodeFromTmdb(
        episodeOf(mediaId, 1, 1, QStringLiteral("The Target"))));

    QCOMPARE(m_media->episodeForFile(fileId).title,
             QStringLiteral("The Target"));
}

void TestMediaRepository::aFileWithNoEpisodeTitleLeavesTmdbsAlone()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));

    QVERIFY(m_media->overwriteEpisodeFromTmdb(
        episodeOf(mediaId, 1, 1, QStringLiteral("The Target"))));

    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1), fileId));

    const QList<EpisodeRecord> stored = m_media->episodesFor(mediaId);
    QCOMPARE(stored.size(), 1);
    QCOMPARE(stored.first().title, QStringLiteral("The Target"));
    QCOMPARE(stored.first().fileId, fileId);
}

void TestMediaRepository::nextEpisodeCrossesASeasonBoundary()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));

    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1),
                                   addFile(QStringLiteral("s01e01.mkv"))));
    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 2, 1),
                                   addFile(QStringLiteral("s02e01.mkv"))));

    const EpisodeRecord next = m_media->nextEpisodeWithFile(mediaId, 1, 1);
    QCOMPARE(next.season, 2);
    QCOMPARE(next.episode, 1);
    QVERIFY(next.hasFile());
    QCOMPARE(next.fileName, QStringLiteral("s02e01.mkv"));
}

void TestMediaRepository::nextEpisodeSkipsWhatHasNoFile()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));

    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1),
                                   addFile(QStringLiteral("s01e01.mkv"))));
    QVERIFY(m_media->overwriteEpisodeFromTmdb(episodeOf(mediaId, 1, 2)));
    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 3),
                                   addFile(QStringLiteral("s01e03.mkv"))));

    const EpisodeRecord next = m_media->nextEpisodeWithFile(mediaId, 1, 1);
    QCOMPARE(next.episode, 3);
}

void TestMediaRepository::nextEpisodeStopsAtTheLastOne()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));

    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1),
                                   addFile(QStringLiteral("s01e01.mkv"))));

    QVERIFY(!m_media->nextEpisodeWithFile(mediaId, 1, 1).hasFile());
}

void TestMediaRepository::creditsBelongToATitleOrToOneEpisode()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));

    CreditRecord star;
    star.kind = CreditRecord::Cast;
    star.name = QStringLiteral("Dominic West");
    star.role = QStringLiteral("Jimmy McNulty");

    CreditRecord creator;
    creator.kind = CreditRecord::Creator;
    creator.name = QStringLiteral("David Simon");

    QVERIFY(m_media->replaceCredits(mediaId, 0, {star, creator}));

    QVERIFY(m_media->upsertEpisode(
        episodeOf(mediaId, 1, 1, QStringLiteral("The Target")), -1));

    const qint64 episodeId = m_media->episodeRowId(mediaId, 1, 1);
    QVERIFY(episodeId > 0);

    CreditRecord director;
    director.kind = CreditRecord::Director;
    director.name = QStringLiteral("Clark Johnson");
    director.role = QStringLiteral("Director");
    QVERIFY(m_media->replaceCredits(mediaId, episodeId, {director}));

    const QList<CreditRecord> title = m_media->creditsFor(mediaId);
    QCOMPARE(title.size(), 2);
    QCOMPARE(title.at(0).name, QStringLiteral("Dominic West"));
    QCOMPARE(title.at(0).role, QStringLiteral("Jimmy McNulty"));

    const QList<CreditRecord> own = m_media->creditsFor(mediaId, episodeId);
    QCOMPARE(own.size(), 1);
    QCOMPARE(own.at(0).name, QStringLiteral("Clark Johnson"));
    QCOMPARE(own.at(0).kind, int(CreditRecord::Director));
}

void TestMediaRepository::creditsArrivingAgainReplaceWhatWasThere()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("Deadwood"), 1406));

    CreditRecord first;
    first.kind = CreditRecord::Cast;
    first.name = QStringLiteral("Timothy Olyphant");

    QVERIFY(m_media->replaceCredits(mediaId, 0, {first}));
    QVERIFY(m_media->replaceCredits(mediaId, 0, {first}));

    QCOMPARE(m_media->creditsFor(mediaId).size(), 1);

    CreditRecord second;
    second.kind = CreditRecord::Cast;
    second.name = QStringLiteral("Ian McShane");

    QVERIFY(m_media->replaceCredits(mediaId, 0, {second}));

    const QList<CreditRecord> credits = m_media->creditsFor(mediaId);
    QCOMPARE(credits.size(), 1);
    QCOMPARE(credits.at(0).name, QStringLiteral("Ian McShane"));
}

void TestMediaRepository::aTitleAskedAboutIsNeverAskedAboutAgain()
{
    const qint64 first =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    const qint64 second =
        m_media->upsertMedia(showNamed(QStringLiteral("Deadwood"), 1406));

    QCOMPARE(m_media->titlesWithoutCreditsCount(), 2);
    QCOMPARE(m_media->titlesWithoutCredits(10), QList<qint64>({first, second}));

    QVERIFY(m_media->markCreditsFetched(first));
    QCOMPARE(m_media->titlesWithoutCredits(10), QList<qint64>({second}));

    QVERIFY(m_media->markCreditsFetched(second));
    QVERIFY(m_media->titlesWithoutCredits(10).isEmpty());
    QCOMPARE(m_media->titlesWithoutCreditsCount(), 0);

    QVERIFY(m_media->markCreditsFetched(first));
    QVERIFY(m_media->titlesWithoutCredits(10).isEmpty());

    QVERIFY(m_media->markSeasonFetched(first, 1, QString()));
    QCOMPARE(m_media->seasonsWithoutCredits(10),
             (QList<QPair<qint64, int>>({{first, 1}})));

    QVERIFY(m_media->markSeasonCreditsFetched(first, 1));
    QVERIFY(m_media->seasonsWithoutCredits(10).isEmpty());
}

void TestMediaRepository::aFetchedSeasonSurvivesAReopen()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));

    QVERIFY(!m_media->seasonFetched(mediaId, 1));
    QVERIFY(m_media->markSeasonFetched(mediaId, 1, QStringLiteral("/season1.jpg")));
    QVERIFY(m_media->markSeasonFetched(mediaId, 2, QString()));
    QVERIFY(m_media->seasonFetched(mediaId, 1));
    QVERIFY(m_media->seasonFetched(mediaId, 2));
    QVERIFY(!m_media->seasonFetched(mediaId, 3));
    QCOMPARE(m_media->seasonPosters(mediaId).value(1), QStringLiteral("/season1.jpg"));
    QVERIFY(!m_media->seasonPosters(mediaId).contains(2));

    const QString path = m_database->filePath();
    m_media.reset();
    m_database->close();

    m_database.reset(new Database);
    QVERIFY(m_database->open(path));
    m_media.reset(new MediaRepository(*m_database));

    QVERIFY(m_media->seasonFetched(mediaId, 1));
    QCOMPARE(m_media->seasonPosters(mediaId).value(1), QStringLiteral("/season1.jpg"));

    QVERIFY(m_media->clearFetchedSeasons());
    QVERIFY(!m_media->seasonFetched(mediaId, 1));
}

void TestMediaRepository::aSeasonBackdropIsItsFirstStill()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("Dark"), 70523));

    EpisodeRecord first = episodeOf(mediaId, 1, 1);
    first.stillPath = QStringLiteral("/s1e1.jpg");
    EpisodeRecord second = episodeOf(mediaId, 1, 2);
    second.stillPath = QStringLiteral("/s1e2.jpg");
    EpisodeRecord otherSeason = episodeOf(mediaId, 2, 1);
    otherSeason.stillPath = QStringLiteral("/s2e1.jpg");

    QVERIFY(m_media->upsertEpisode(first, 0));
    QVERIFY(m_media->upsertEpisode(second, addFile(QStringLiteral("Dark.S01E02.mkv"))));
    QVERIFY(m_media->upsertEpisode(otherSeason, 0));

    QCOMPARE(m_media->artworkPaths().seasonBackdrops,
             QStringList({QStringLiteral("/s1e1.jpg")}));
    QCOMPARE(m_media->artworkPathsFor({mediaId}).seasonBackdrops,
             QStringList({QStringLiteral("/s1e1.jpg")}));
}

void TestMediaRepository::recentlyAddedListsTitlesNotFiles()
{
    constexpr qint64 kWeek = 7 * 24 * 60 * 60;

    const auto addedAt = [this](const QString &name, qint64 added) {
        const qint64 fileId = addFile(name);
        QSqlQuery query(m_database->handle());
        query.prepare(QStringLiteral("UPDATE files SET added = :added WHERE id = :id"));
        query.bindValue(QStringLiteral(":added"), added);
        query.bindValue(QStringLiteral(":id"), fileId);
        return query.exec() ? fileId : -1;
    };

    const qint64 show = m_media->upsertMedia(showNamed(QStringLiteral("Tvrdjava"), 1));
    MediaRecord film = showNamed(QStringLiteral("Heat"), 949);
    film.kind = QStringLiteral("movie");
    const qint64 movie = m_media->upsertMedia(film);
    const qint64 unsure = m_media->upsertMedia(showNamed(QStringLiteral("Right to Reply"), 2));

    const qint64 old = 1000000;
    const qint64 now = old + 10 * kWeek;

    QVERIFY(m_media->linkFile(addedAt(QStringLiteral("t1.mkv"), old), show, 1.0, false));
    QVERIFY(m_media->linkFile(addedAt(QStringLiteral("t2.mkv"), now - 60), show, 1.0, false));
    QVERIFY(m_media->linkFile(addedAt(QStringLiteral("t3.mkv"), now), show, 1.0, false));
    QVERIFY(m_media->linkFile(addedAt(QStringLiteral("heat.mkv"), now - 3600), movie, 1.0, false));
    QVERIFY(m_media->linkFile(addedAt(QStringLiteral("reply.mkv"), now + 60), unsure, 0.7, true));
    QVERIFY(addedAt(QStringLiteral("video_01.mkv"), now + 120) > 0);

    const QList<MediaRecord> recent = m_media->recentlyAddedTitles(20, kWeek);

    QCOMPARE(recent.size(), 2);
    QCOMPARE(recent.at(0).id, show);
    QCOMPARE(recent.at(0).lastAdded, now);
    QCOMPARE(recent.at(0).newFileCount, 2);
    QCOMPARE(recent.at(0).fileCount, 3);
    QCOMPARE(recent.at(1).id, movie);
    QCOMPARE(recent.at(1).newFileCount, 1);

    QCOMPARE(m_media->recentlyAddedTitles(1, kWeek).size(), 1);
}

void TestMediaRepository::aPinnedShowReportsItsEpisodesAndItsFile()
{
    const qint64 mediaId =
        m_media->upsertMedia(showNamed(QStringLiteral("Better Call Saul"), 60059));
    const qint64 fileId = addFile(QStringLiteral("01.mkv"));

    QVERIFY(m_media->linkFile(fileId, mediaId, 1.0, false));
    QVERIFY(m_media->upsertEpisode(episodeOf(mediaId, 1, 1), fileId));

    for (int episode = 1; episode <= 10; ++episode) {
        QVERIFY(m_media->overwriteEpisodeFromTmdb(
            episodeOf(mediaId, 1, episode,
                      QStringLiteral("Episode %1").arg(episode))));
    }

    const QList<EpisodeRecord> episodes = m_media->episodesFor(mediaId);
    QCOMPARE(episodes.size(), 10);

    int withFiles = 0;
    for (const EpisodeRecord &episode : episodes) {
        if (episode.hasFile()) {
            ++withFiles;
            QCOMPARE(episode.season, 1);
            QCOMPARE(episode.episode, 1);
            QCOMPARE(episode.fileName, QStringLiteral("01.mkv"));
        }
        QVERIFY(!episode.title.isEmpty());
    }
    QCOMPARE(withFiles, 1);

    QCOMPARE(m_media->allOfKind(QStringLiteral("tv")).size(), 1);
    QCOMPARE(m_media->matchedCount(), 1);
}

void TestMediaRepository::aListReadsEveryColumnIntoItsField()
{
    MediaRecord show = showNamed(QStringLiteral("The Wire"), 1438);
    show.originalTitle = QStringLiteral("The Wire (US)");
    show.overview = QStringLiteral("Baltimore, told from both sides.");
    show.rating = 9.25;
    show.runtimeMinutes = 58;
    show.genres = QStringLiteral("Crime, Drama");
    show.certification = QStringLiteral("TV-MA");
    show.posterPath = QStringLiteral("/poster.jpg");
    show.backdropPath = QStringLiteral("/backdrop.jpg");

    const qint64 showId = m_media->upsertMedia(show);
    QVERIFY(showId > 0);

    const qint64 first = addFile(QStringLiteral("s01e01.mkv"));
    const qint64 second = addFile(QStringLiteral("s02e01.mkv"));
    QVERIFY(m_media->linkFile(first, showId, 1.0, false));
    QVERIFY(m_media->linkFile(second, showId, 1.0, false));
    QVERIFY(m_media->upsertEpisode(episodeOf(showId, 1, 1, QStringLiteral("The Target")), first));
    QVERIFY(m_media->upsertEpisode(episodeOf(showId, 2, 1, QStringLiteral("Ebb Tide")), second));

    QSqlQuery played(m_database->handle());
    played.prepare(QStringLiteral(
        "INSERT INTO playback_state (file_id, position_seconds, duration_seconds,"
        "                            watched, last_played)"
        " VALUES (:file, :position, 3000, :watched, 1)"));
    played.bindValue(QStringLiteral(":file"), first);
    played.bindValue(QStringLiteral(":position"), 0.0);
    played.bindValue(QStringLiteral(":watched"), 1);
    QVERIFY(played.exec());
    played.bindValue(QStringLiteral(":file"), second);
    played.bindValue(QStringLiteral(":position"), 1500.0);
    played.bindValue(QStringLiteral(":watched"), 0);
    QVERIFY(played.exec());

    const QList<MediaRecord> shows = m_media->allOfKind(QStringLiteral("tv"));
    QCOMPARE(shows.size(), 1);
    const MediaRecord listed = shows.first();
    QCOMPARE(listed.id, showId);
    QCOMPARE(listed.tmdbId, Q_INT64_C(1438));
    QCOMPARE(listed.kind, QStringLiteral("tv"));
    QCOMPARE(listed.title, show.title);
    QCOMPARE(listed.originalTitle, show.originalTitle);
    QCOMPARE(listed.year, 2002);
    QCOMPARE(listed.overview, show.overview);
    QCOMPARE(listed.rating, 9.25);
    QCOMPARE(listed.runtimeMinutes, 58);
    QCOMPARE(listed.genres, show.genres);
    QCOMPARE(listed.certification, show.certification);
    QCOMPARE(listed.posterPath, show.posterPath);
    QCOMPARE(listed.backdropPath, show.backdropPath);
    QCOMPARE(listed.fileCount, 2);
    QCOMPARE(listed.seasonCount, 2);
    QCOMPARE(listed.firstFileHandle, QStringLiteral("F:/Media/s01e01.mkv"));
    QCOMPARE(listed.watchedCount, 1);
    QCOMPARE(listed.partialProgress, 0.5);

    const QList<EpisodeRecord> episodes = m_media->episodesFor(showId);
    QCOMPARE(episodes.size(), 2);
    QCOMPARE(episodes.at(0).season, 1);
    QCOMPARE(episodes.at(0).episode, 1);
    QCOMPARE(episodes.at(0).title, QStringLiteral("The Target"));
    QCOMPARE(episodes.at(0).fileId, first);
    QCOMPARE(episodes.at(0).fileHandle, QStringLiteral("F:/Media/s01e01.mkv"));
    QCOMPARE(episodes.at(0).fileName, QStringLiteral("s01e01.mkv"));
    QVERIFY(!episodes.at(0).missing);
    QVERIFY(episodes.at(0).watched);
    QCOMPARE(episodes.at(0).durationSeconds, 3000.0);
    QCOMPARE(episodes.at(1).season, 2);
    QCOMPARE(episodes.at(1).title, QStringLiteral("Ebb Tide"));
    QCOMPARE(episodes.at(1).positionSeconds, 1500.0);
    QVERIFY(!episodes.at(1).watched);

    MediaRecord film;
    film.tmdbId = 949;
    film.kind = QStringLiteral("movie");
    film.title = QStringLiteral("Heat");
    film.year = 1995;
    film.posterPath = QStringLiteral("/heat.jpg");
    const qint64 filmId = m_media->upsertMedia(film);
    const qint64 heat = addFile(QStringLiteral("heat.mkv"));
    QVERIFY(m_media->linkFile(heat, filmId, 0.6, true));

    const QList<MediaRecord> suggested = m_media->withSuggestions();
    QCOMPARE(suggested.size(), 1);
    QCOMPARE(suggested.first().id, filmId);
    QCOMPARE(suggested.first().title, QStringLiteral("Heat"));
    QCOMPARE(suggested.first().year, 1995);
    QCOMPARE(suggested.first().posterPath, QStringLiteral("/heat.jpg"));
    QCOMPARE(suggested.first().fileCount, 1);
    QCOMPARE(suggested.first().suggestedFileCount, 1);
    QCOMPARE(suggested.first().firstFileHandle, QStringLiteral("F:/Media/heat.mkv"));
}

void TestMediaRepository::recordEqualityIncludesTheListCounts()
{
    MediaRecord left = showNamed(QStringLiteral("The Wire"), 1438);
    MediaRecord right = left;
    QVERIFY(left == right);

    right.fileCount = 1;
    QVERIFY(left != right);

    right = left;
    right.watchedCount = 2;
    QVERIFY(left != right);

    right = left;
    right.partialProgress = 0.5;
    QVERIFY(left != right);

    right = left;
    right.seasonCount = 3;
    QVERIFY(left != right);
}

void TestMediaRepository::aTitleReadByIdCountsLikeTheLists()
{
    const qint64 showId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    MediaRecord film = showNamed(QStringLiteral("Heat"), 949);
    film.kind = QStringLiteral("movie");
    const qint64 filmId = m_media->upsertMedia(film);
    QVERIFY(showId > 0);
    QVERIFY(filmId > 0);

    const qint64 first = addFile(QStringLiteral("s01e01.mkv"));
    const qint64 second = addFile(QStringLiteral("s01e02.mkv"));
    const qint64 heat = addFile(QStringLiteral("heat.mkv"));
    QVERIFY(m_media->linkFile(first, showId, 1.0, false));
    QVERIFY(m_media->linkFile(second, showId, 0.6, true));
    QVERIFY(m_media->linkFile(heat, filmId, 1.0, false));
    QVERIFY(m_media->upsertEpisode(episodeOf(showId, 1, 1), first));
    QVERIFY(m_media->upsertEpisode(episodeOf(showId, 1, 2), second));

    const QList<MediaRecord> shows = m_media->allOfKind(QStringLiteral("tv"));
    QCOMPARE(shows.size(), 1);

    const QList<MediaRecord> byId = m_media->listedByIds({showId, 9999});
    QCOMPARE(byId.size(), 1);
    QVERIFY(byId.first() == shows.first());
    QCOMPARE(byId.first().fileCount, 2);
    QCOMPARE(byId.first().suggestedFileCount, 1);
    QCOMPARE(byId.first().seasonCount, 1);

    const QList<MediaRecord> suggested = m_media->withSuggestions();
    QCOMPARE(suggested.size(), 1);
    QVERIFY(suggested.first() == byId.first());

    QList<qint64> files = m_media->fileIdsFor(showId);
    std::sort(files.begin(), files.end());
    QCOMPARE(files, (QList<qint64>{first, second}));
    QVERIFY(m_media->fileIdsFor(9999).isEmpty());

    QVERIFY(m_media->listedByIds({}).isEmpty());
}

void TestMediaRepository::titlesSortByFoldedTitleThenId()
{
    MediaRecord elite;
    elite.id = 2;
    elite.title = QStringLiteral("Élite");

    MediaRecord dune;
    dune.id = 1;
    dune.title = QStringLiteral("Dune");

    MediaRecord remake;
    remake.id = 3;
    remake.title = QStringLiteral("Dune");

    QVERIFY(MediaRepository::listsBefore(dune, elite));
    QVERIFY(!MediaRepository::listsBefore(elite, dune));
    QVERIFY(MediaRepository::listsBefore(dune, remake));
    QVERIFY(!MediaRepository::listsBefore(remake, dune));
    QVERIFY(!MediaRepository::listsBefore(dune, dune));
}

void TestMediaRepository::clearingFindsOnlyUnpinnedLinksAndReleasesStrayEpisodes()
{
    const qint64 showId =
        m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    QVERIFY(showId > 0);
    QCOMPARE(m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438)), showId);

    const qint64 automatic = addFile(QStringLiteral("s01e01.mkv"));
    const qint64 pinned = addFile(QStringLiteral("s01e02.mkv"));
    const qint64 stray = addFile(QStringLiteral("s01e03.mkv"));

    QVERIFY(m_media->linkFile(automatic, showId, 0.9, true));
    QVERIFY(m_media->linkFile(pinned, showId, 1.0, false));
    QVERIFY(m_media->upsertEpisode(episodeOf(showId, 1, 1), automatic));
    QVERIFY(m_media->upsertEpisode(episodeOf(showId, 1, 3), stray));

    MatchOverride pin;
    pin.fileHandle = QStringLiteral("F:/Media/s01e02.mkv");
    pin.tmdbId = 1438;
    pin.kind = QStringLiteral("tv");
    QVERIFY(m_media->setOverride(pin));

    const QList<LinkedFile> links = m_media->unpinnedLinks();
    QCOMPARE(links.size(), 1);
    QCOMPARE(links.first().link.fileId, automatic);
    QCOMPARE(links.first().link.mediaId, showId);
    QVERIFY(links.first().link.suggested);
    QCOMPARE(links.first().fileHandle, QStringLiteral("F:/Media/s01e01.mkv"));

    QVERIFY(m_media->detachUnlinkedEpisodes());

    const QList<EpisodeRecord> episodes = m_media->episodesFor(showId);
    QCOMPARE(episodes.size(), 2);
    QCOMPARE(episodes.at(0).episode, 1);
    QCOMPARE(episodes.at(0).fileId, automatic);
    QCOMPARE(episodes.at(1).episode, 3);
    QVERIFY(!episodes.at(1).hasFile());
}

void TestMediaRepository::titlesAndEpisodesAreWrittenWithTheirFoldedKeys()
{
    const auto single = [this](const QString &sql) {
        QSqlQuery query(m_database->handle());
        if (!query.exec(sql) || !query.next()) {
            return QString();
        }
        return query.value(0).toString();
    };

    const qint64 showId =
        m_media->upsertMedia(showNamed(QStringLiteral("Đorđe i Zmaj"), 7));
    QVERIFY(showId > 0);
    QCOMPARE(single(QStringLiteral("SELECT title_key FROM media")),
             QStringLiteral("djordje i zmaj"));

    const qint64 file = addFile(QStringLiteral("s01e01.mkv"));
    EpisodeRecord guessed = episodeOf(showId, 1, 1, QStringLiteral("Škola"));
    QVERIFY(m_media->upsertEpisode(guessed, file));
    QCOMPARE(single(QStringLiteral("SELECT title_key FROM episodes")),
             QStringLiteral("skola"));

    guessed.title = QStringLiteral("Something else");
    QVERIFY(m_media->upsertEpisode(guessed, file));
    QCOMPARE(single(QStringLiteral("SELECT title_key FROM episodes")),
             QStringLiteral("skola"));

    QVERIFY(m_media->overwriteEpisodeFromTmdb(
        episodeOf(showId, 1, 1, QStringLiteral("Ćao, Škola"))));
    QCOMPARE(single(QStringLiteral("SELECT title_key FROM episodes")),
             QStringLiteral("cao, skola"));
}

void TestMediaRepository::artworkPathsForNameOnlyThoseTitles()
{
    MediaRecord wire = showNamed(QStringLiteral("The Wire"), 1438);
    wire.posterPath = QStringLiteral("/wire.jpg");
    wire.backdropPath = QStringLiteral("/wire-back.jpg");
    MediaRecord heat = showNamed(QStringLiteral("Heat"), 949);
    heat.kind = QStringLiteral("movie");
    heat.posterPath = QStringLiteral("/heat.jpg");

    const qint64 wireId = m_media->upsertMedia(wire);
    const qint64 heatId = m_media->upsertMedia(heat);
    QVERIFY(wireId > 0);
    QVERIFY(heatId > 0);

    const qint64 episodeFile = addFile(QStringLiteral("s01e01.mkv"));
    const qint64 filmFile = addFile(QStringLiteral("heat.mkv"));
    QVERIFY(m_media->linkFile(episodeFile, wireId, 1.0, false));
    QVERIFY(m_media->linkFile(filmFile, heatId, 1.0, false));

    EpisodeRecord episode = episodeOf(wireId, 1, 1, QStringLiteral("The Target"));
    QVERIFY(m_media->upsertEpisode(episode, episodeFile));
    episode.stillPath = QStringLiteral("/target.jpg");
    QVERIFY(m_media->overwriteEpisodeFromTmdb(episode));

    const ArtworkPaths wireOnly = m_media->artworkPathsFor({wireId});
    QCOMPARE(wireOnly.posters, QStringList{QStringLiteral("/wire.jpg")});
    QCOMPARE(wireOnly.backdrops, QStringList{QStringLiteral("/wire-back.jpg")});
    QCOMPARE(wireOnly.stills, QStringList{QStringLiteral("/target.jpg")});

    const ArtworkPaths heatOnly = m_media->artworkPathsFor({heatId});
    QCOMPARE(heatOnly.posters, QStringList{QStringLiteral("/heat.jpg")});
    QVERIFY(heatOnly.backdrops.isEmpty());
    QVERIFY(heatOnly.stills.isEmpty());

    const ArtworkPaths none = m_media->artworkPathsFor({});
    QVERIFY(none.posters.isEmpty());
    QVERIFY(none.stills.isEmpty());
}

void TestMediaRepository::everyFaceTheLibraryKnowsIsOnTheWarmingList()
{
    MediaRecord wire = showNamed(QStringLiteral("The Wire"), 1438);
    const qint64 wireId = m_media->upsertMedia(wire);

    MediaRecord ghost = showNamed(QStringLiteral("Rome"), 1183);
    const qint64 ghostId = m_media->upsertMedia(ghost);

    QVERIFY(m_media->linkFile(addFile(QStringLiteral("s01e01.mkv")), wireId,
                              1.0, false));

    CreditRecord star;
    star.kind = CreditRecord::Cast;
    star.name = QStringLiteral("Dominic West");
    star.role = QStringLiteral("Jimmy McNulty");
    star.profilePath = QStringLiteral("/west.jpg");

    CreditRecord unphotographed;
    unphotographed.kind = CreditRecord::Writer;
    unphotographed.name = QStringLiteral("David Simon");
    unphotographed.role = QStringLiteral("Writer");

    QVERIFY(m_media->replaceCredits(wireId, 0, {star, unphotographed}));

    CreditRecord elsewhere;
    elsewhere.kind = CreditRecord::Cast;
    elsewhere.name = QStringLiteral("Kevin McKidd");
    elsewhere.profilePath = QStringLiteral("/mckidd.jpg");
    QVERIFY(m_media->replaceCredits(ghostId, 0, {elsewhere}));

    const ArtworkPaths all = m_media->artworkPaths();
    QCOMPARE(all.profiles, QStringList{QStringLiteral("/west.jpg")});

    QCOMPARE(m_media->artworkPathsFor({wireId}).profiles,
             QStringList{QStringLiteral("/west.jpg")});
    QVERIFY(m_media->artworkPathsFor({}).profiles.isEmpty());
}

namespace {

PinRecord pinOf(qint64 fileId, const QString &fileHandle,
                const MediaRecord &media, int season = 0, int episode = 0)
{
    PinRecord request;
    request.fileId = fileId;
    request.fileHandle = fileHandle;
    request.media = media;
    request.season = season;
    request.episode = episode;
    return request;
}

}

void TestMediaRepository::aPinWritesTheTitleTheLinkAndTheOverrideTogether()
{
    const qint64 fileId = addFile(QStringLiteral("e02.mkv"));
    QVERIFY(fileId > 0);

    PinRecord request = pinOf(fileId, QStringLiteral("F:/Media/e02.mkv"),
                              showNamed(QStringLiteral("The Wire"), 1438), 1, 2);
    request.episodeTitle = QStringLiteral("The Detail");

    const qint64 mediaId = m_media->pin(request);
    QVERIFY(mediaId > 0);

    const FileMediaLink link = m_media->linkForFile(fileId);
    QCOMPARE(link.mediaId, mediaId);
    QCOMPARE(link.confidence, 1.0);
    QVERIFY(!link.suggested);

    const MatchOverride pinned =
        m_media->overrideFor(QStringLiteral("F:/Media/e02.mkv"));
    QVERIFY(pinned.isValid());
    QCOMPARE(pinned.tmdbId, Q_INT64_C(1438));
    QCOMPARE(pinned.kind, QStringLiteral("tv"));
    QCOMPARE(pinned.season, 1);
    QCOMPARE(pinned.episode, 2);

    const EpisodeRecord episode = m_media->episodeForFile(fileId);
    QCOMPARE(episode.mediaId, mediaId);
    QCOMPARE(episode.season, 1);
    QCOMPARE(episode.episode, 2);
    QCOMPARE(episode.title, QStringLiteral("The Detail"));
}

void TestMediaRepository::aFilmPinWritesNoEpisode()
{
    const qint64 fileId = addFile(QStringLiteral("heat.mkv"));

    MediaRecord film;
    film.tmdbId = 949;
    film.kind = QStringLiteral("movie");
    film.title = QStringLiteral("Heat");
    film.year = 1995;

    const qint64 mediaId =
        m_media->pin(pinOf(fileId, QStringLiteral("F:/Media/heat.mkv"), film, 1, 2));
    QVERIFY(mediaId > 0);

    QCOMPARE(m_media->linkForFile(fileId).mediaId, mediaId);
    QVERIFY(!m_media->episodeForFile(fileId).hasFile());
    QVERIFY(m_media->episodesFor(mediaId).isEmpty());
    QCOMPARE(m_media->overrideFor(QStringLiteral("F:/Media/heat.mkv")).kind,
             QStringLiteral("movie"));
}

void TestMediaRepository::aFailedPinLeavesNothingBehind()
{
    const qint64 noSuchFile = 9999;
    const QString goneHandle = QStringLiteral("F:/Media/gone.mkv");

    QCOMPARE(m_media->pin(pinOf(noSuchFile, goneHandle,
                                showNamed(QStringLiteral("The Wire"), 1438), 1, 1)),
             Q_INT64_C(-1));

    QVERIFY(!m_media->overrideFor(goneHandle).isValid());
    QVERIFY(!m_media->mediaByTmdbId(1438, QStringLiteral("tv")).isValid());
    QVERIFY(!m_media->linkForFile(noSuchFile).isValid());
    QCOMPARE(m_media->matchedCount(), 0);

    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));
    MediaRecord nameless;
    nameless.kind = QStringLiteral("tv");

    QCOMPARE(m_media->pin(pinOf(fileId, QStringLiteral("F:/Media/e01.mkv"),
                                nameless, 1, 1)),
             Q_INT64_C(-1));

    QVERIFY(!m_media->overrideFor(QStringLiteral("F:/Media/e01.mkv")).isValid());
    QVERIFY(!m_media->linkForFile(fileId).isValid());
    QVERIFY(!m_media->episodeForFile(fileId).hasFile());
}

void TestMediaRepository::repinningMovesTheLinkAndTheOverride()
{
    const qint64 fileId = addFile(QStringLiteral("e01.mkv"));
    const QString handle = QStringLiteral("F:/Media/e01.mkv");

    const qint64 wrong = m_media->pin(
        pinOf(fileId, handle, showNamed(QStringLiteral("Wrong Show"), 111), 1, 1));
    const qint64 right = m_media->pin(
        pinOf(fileId, handle, showNamed(QStringLiteral("The Wire"), 1438), 1, 1));

    QVERIFY(wrong > 0);
    QVERIFY(right > 0);
    QVERIFY(wrong != right);

    QCOMPARE(m_media->linkForFile(fileId).mediaId, right);
    QCOMPARE(m_media->overrideFor(handle).tmdbId, Q_INT64_C(1438));
    QCOMPARE(m_media->episodeForFile(fileId).mediaId, right);

    const QList<EpisodeRecord> leftBehind = m_media->episodesFor(wrong);
    QCOMPARE(leftBehind.size(), 1);
    QVERIFY(!leftBehind.first().hasFile());
}

void TestMediaRepository::onlyOwnedUncheckedFilmsWaitForACollectionCheck()
{
    MediaRecord alien = showNamed(QStringLiteral("Alien"), 348);
    alien.kind = QStringLiteral("movie");
    MediaRecord aliens = showNamed(QStringLiteral("Aliens"), 679);
    aliens.kind = QStringLiteral("movie");
    MediaRecord leftover = showNamed(QStringLiteral("Leftover"), 5);
    leftover.kind = QStringLiteral("movie");
    MediaRecord guessed = showNamed(QStringLiteral("Guessed"), 6);
    guessed.kind = QStringLiteral("movie");

    const qint64 alienId = m_media->upsertMedia(alien);
    const qint64 aliensId = m_media->upsertMedia(aliens);
    const qint64 guessedId = m_media->upsertMedia(guessed);
    const qint64 wireId = m_media->upsertMedia(showNamed(QStringLiteral("The Wire"), 1438));
    QVERIFY(m_media->upsertMedia(leftover) > 0);

    QVERIFY(m_media->linkFile(addFile(QStringLiteral("alien.mkv")), alienId, 1.0, false));
    QVERIFY(m_media->linkFile(addFile(QStringLiteral("aliens.mkv")), aliensId, 1.0, false));
    QVERIFY(m_media->linkFile(addFile(QStringLiteral("guessed.mkv")), guessedId, 0.5, true));
    QVERIFY(m_media->linkFile(addFile(QStringLiteral("wire.mkv")), wireId, 1.0, false));

    QCOMPARE(m_media->filmsWithoutCollectionCheck(), QList<qint64>({348, 679}));

    QVERIFY(m_media->setMediaCollection(alienId, 8091));
    QVERIFY(m_media->setMediaCollection(aliensId, 0));

    QVERIFY(m_media->filmsWithoutCollectionCheck().isEmpty());
    QCOMPARE(m_media->collectionIdFor(alienId), Q_INT64_C(8091));
    QCOMPARE(m_media->collectionIdFor(aliensId), Q_INT64_C(0));
}

void TestMediaRepository::aNotedCollectionIsFetchedUntilItIsSaved()
{
    MediaRecord alien = showNamed(QStringLiteral("Alien"), 348);
    alien.kind = QStringLiteral("movie");
    const qint64 alienId = m_media->upsertMedia(alien);
    QVERIFY(m_media->linkFile(addFile(QStringLiteral("alien.mkv")), alienId, 1.0, false));
    QVERIFY(m_media->setMediaCollection(alienId, 8091));

    CollectionRecord reference;
    reference.tmdbId = 8091;
    reference.name = QStringLiteral("Alien Collection");
    reference.posterPath = QStringLiteral("/alien.jpg");
    QVERIFY(m_media->noteCollection(reference));
    QVERIFY(m_media->noteCollection(reference));

    QCOMPARE(m_media->collectionById(8091).name, QStringLiteral("Alien Collection"));
    QCOMPARE(m_media->collectionById(8091).fetchedAt, Q_INT64_C(0));
    QCOMPARE(m_media->collectionsToFetch(1000), QList<qint64>({8091}));

    CollectionRecord full = reference;
    full.fetchedAt = 2000;
    QVERIFY(m_media->saveCollection(full));

    QVERIFY(m_media->collectionsToFetch(1000).isEmpty());
    QCOMPARE(m_media->collectionsToFetch(3000), QList<qint64>({8091}));

    QVERIFY(m_media->noteCollection(reference));
    QCOMPARE(m_media->collectionById(8091).fetchedAt, Q_INT64_C(2000));
}

void TestMediaRepository::savingACollectionReplacesItsFilms()
{
    CollectionRecord collection;
    collection.tmdbId = 264;
    collection.name = QStringLiteral("Back to the Future Collection");
    collection.overview = QStringLiteral("Time travel.");
    collection.backdropPath = QStringLiteral("/bttf-bd.jpg");

    const auto partOf = [](qint64 id, int position, const QString &title, const QString &date) {
        CollectionPartRecord part;
        part.tmdbId = id;
        part.position = position;
        part.title = title;
        part.releaseDate = date;
        part.posterPath = QStringLiteral("/p%1.jpg").arg(id);
        return part;
    };

    collection.parts = {partOf(105, 0, QStringLiteral("Back to the Future"), QStringLiteral("1985-07-03")),
                        partOf(999, 1, QStringLiteral("Wrong"), QString())};
    QVERIFY(m_media->saveCollection(collection));

    collection.parts = {partOf(105, 0, QStringLiteral("Back to the Future"), QStringLiteral("1985-07-03")),
                        partOf(165, 1, QStringLiteral("Part II"), QStringLiteral("1989-11-22")),
                        partOf(196, 2, QStringLiteral("Part III"), QStringLiteral("1990-05-25"))};
    QVERIFY(m_media->saveCollection(collection));

    const CollectionRecord read = m_media->collectionById(264);
    QVERIFY(read.isValid());
    QCOMPARE(read.overview, QStringLiteral("Time travel."));
    QCOMPARE(read.backdropPath, QStringLiteral("/bttf-bd.jpg"));
    QVERIFY(read.fetchedAt > 0);
    QCOMPARE(read.parts.size(), 3);
    QCOMPARE(read.parts.at(0).tmdbId, Q_INT64_C(105));
    QCOMPARE(read.parts.at(1).title, QStringLiteral("Part II"));
    QCOMPARE(read.parts.at(2).releaseDate, QStringLiteral("1990-05-25"));
    QCOMPARE(read.parts.at(2).posterPath, QStringLiteral("/p196.jpg"));

    QVERIFY(!m_media->collectionById(1).isValid());
}

void TestMediaRepository::ownedCollectionsComeWithTheirFilms()
{
    MediaRecord first = showNamed(QStringLiteral("Back to the Future"), 105);
    first.kind = QStringLiteral("movie");
    MediaRecord alien = showNamed(QStringLiteral("Alien"), 348);
    alien.kind = QStringLiteral("movie");
    const qint64 firstId = m_media->upsertMedia(first);
    const qint64 alienId = m_media->upsertMedia(alien);
    QVERIFY(m_media->linkFile(addFile(QStringLiteral("bttf.mkv")), firstId, 1.0, false));
    QVERIFY(m_media->setMediaCollection(firstId, 264));
    QVERIFY(m_media->setMediaCollection(alienId, 8091));

    CollectionRecord bttf;
    bttf.tmdbId = 264;
    bttf.name = QStringLiteral("Back to the Future Collection");
    bttf.posterPath = QStringLiteral("/bttf.jpg");
    CollectionPartRecord one;
    one.tmdbId = 105;
    one.position = 0;
    one.posterPath = QStringLiteral("/p105.jpg");
    CollectionPartRecord two;
    two.tmdbId = 165;
    two.position = 1;
    bttf.parts = {one, two};
    QVERIFY(m_media->saveCollection(bttf));

    CollectionRecord aliens;
    aliens.tmdbId = 8091;
    aliens.name = QStringLiteral("Alien Collection");
    aliens.posterPath = QStringLiteral("/alien.jpg");
    QVERIFY(m_media->saveCollection(aliens));

    const QList<CollectionRecord> owned = m_media->ownedCollections();
    QCOMPARE(owned.size(), 1);
    QCOMPARE(owned.first().tmdbId, Q_INT64_C(264));
    QCOMPARE(owned.first().parts.size(), 2);
    QCOMPARE(owned.first().parts.at(1).tmdbId, Q_INT64_C(165));

    const QHash<qint64, qint64> byMedia = m_media->collectionsByMedia();
    QCOMPARE(byMedia.size(), 1);
    QCOMPARE(byMedia.value(firstId), Q_INT64_C(264));

    QStringList posters = m_media->artworkPaths().collectionPosters;
    posters.sort();
    QCOMPARE(posters, QStringList({QStringLiteral("/bttf.jpg"), QStringLiteral("/p105.jpg")}));
}

void TestMediaRepository::aCollectionTheUserMadeKeepsItsOrder()
{
    MediaRecord first = showNamed(QStringLiteral("Alien"), 348);
    first.kind = QStringLiteral("movie");
    MediaRecord second = showNamed(QStringLiteral("Aliens"), 679);
    second.kind = QStringLiteral("movie");
    const qint64 alienId = m_media->upsertMedia(first);
    const qint64 aliensId = m_media->upsertMedia(second);

    CustomCollectionRecord collection;
    collection.name = QStringLiteral("  Friday night  ");
    collection.description = QStringLiteral("Loud ones.");
    collection.coverMode = QStringLiteral("poster");
    collection.mediaIds = {aliensId, alienId};

    const qint64 id = m_media->createCustomCollection(collection);
    QVERIFY(id > 0);

    const QList<CustomCollectionRecord> made = m_media->customCollections();
    QCOMPARE(made.size(), 1);
    QCOMPARE(made.first().id, id);
    QCOMPARE(made.first().name, QStringLiteral("Friday night"));
    QCOMPARE(made.first().description, QStringLiteral("Loud ones."));
    QCOMPARE(made.first().coverMode, QStringLiteral("poster"));
    QVERIFY(made.first().createdAt > 0);
    QCOMPARE(made.first().mediaIds, QList<qint64>({aliensId, alienId}));

    CustomCollectionRecord nameless;
    nameless.mediaIds = {alienId};
    QCOMPARE(m_media->createCustomCollection(nameless), Q_INT64_C(0));

    CustomCollectionRecord empty;
    empty.name = QStringLiteral("Nothing in it");
    QCOMPARE(m_media->createCustomCollection(empty), Q_INT64_C(0));
    QCOMPARE(m_media->customCollections().size(), 1);
}

void TestMediaRepository::aCollectionTakenOffThePageCanComeBack()
{
    QVERIFY(m_media->hiddenCollections().isEmpty());

    QVERIFY(m_media->hideCollection(264, QStringLiteral("Back to the Future Collection")));
    QVERIFY(m_media->hideCollection(-1, QStringLiteral("Marvel Cinematic Universe")));
    QVERIFY(m_media->hideCollection(264, QStringLiteral("Back to the Future Collection")));

    QCOMPARE(m_media->hiddenCollectionIds(), QSet<qint64>({264, -1}));
    QCOMPARE(m_media->hiddenCollections().size(), 2);

    QVERIFY(m_media->restoreCollection(264));
    QCOMPARE(m_media->hiddenCollectionIds(), QSet<qint64>({-1}));

    const QString path = m_database->filePath();
    m_media.reset();
    m_database->close();

    m_database.reset(new Database);
    QVERIFY(m_database->open(path));
    m_media.reset(new MediaRepository(*m_database));

    QCOMPARE(m_media->hiddenCollectionIds(), QSet<qint64>({-1}));
    QCOMPARE(m_media->hiddenCollections().first().name,
             QStringLiteral("Marvel Cinematic Universe"));
}

void TestMediaRepository::searchingOfOneKindAnswersWithTitlesThatAreInTheLibrary()
{
    const auto film = [this](const QString &title, qint64 tmdbId) {
        MediaRecord record;
        record.tmdbId = tmdbId;
        record.kind = QStringLiteral("movie");
        record.title = title;
        record.year = 2000;
        return m_media->upsertMedia(record);
    };

    const qint64 wars = film(QStringLiteral("Star Wars: Episode V"), 1891);
    const qint64 stars = film(QStringLiteral("The Fault in Our Stars"), 222935);
    const qint64 unseen = film(QStringLiteral("Starship Troopers"), 563);
    const qint64 vikings =
        m_media->upsertMedia(showNamed(QStringLiteral("Vikings"), 44217));
    QVERIFY(wars > 0 && stars > 0 && unseen > 0 && vikings > 0);

    QVERIFY(m_media->linkFile(addFile(QStringLiteral("empire.mkv")), wars, 1.0, false));
    QVERIFY(m_media->linkFile(addFile(QStringLiteral("fault.mkv")), stars, 1.0, false));
    QVERIFY(m_media->linkFile(addFile(QStringLiteral("s01e01.mkv")), vikings, 1.0, false));

    const QList<MediaRecord> films =
        m_media->searchOfKind(QStringLiteral("movie"), QStringLiteral("star"), 10);
    QCOMPARE(films.size(), 2);
    QCOMPARE(films.at(0).title, QStringLiteral("Star Wars: Episode V"));
    QCOMPARE(films.at(1).title, QStringLiteral("The Fault in Our Stars"));

    QVERIFY(films.at(0).id != unseen && films.at(1).id != unseen);

    const QList<MediaRecord> shows =
        m_media->searchOfKind(QStringLiteral("tv"), QStringLiteral("vik"), 10);
    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.first().title, QStringLiteral("Vikings"));

    QCOMPARE(m_media->searchOfKind(QStringLiteral("movie"),
                                   QStringLiteral("vik"), 10).size(), 0);
    QCOMPARE(m_media->searchOfKind(QStringLiteral("movie"),
                                   QStringLiteral("star"), 1).size(), 1);
    QCOMPARE(m_media->searchOfKind(QStringLiteral("movie"), QString(), 10).size(), 0);
}

void TestMediaRepository::searchingFindsAGenreFromItsStartAndNotFromTheMiddle()
{
    const auto film = [this](const QString &title, qint64 tmdbId,
                             const QString &genres) {
        MediaRecord record;
        record.tmdbId = tmdbId;
        record.kind = QStringLiteral("movie");
        record.title = title;
        record.year = 2000;
        record.genres = genres;
        const qint64 id = m_media->upsertMedia(record);
        if (id > 0) {
            m_media->linkFile(addFile(title + QStringLiteral(".mkv")), id, 1.0, false);
        }
        return id;
    };

    QVERIFY(film(QStringLiteral("1917"), 530915,
                 QStringLiteral("War, Drama, Action")) > 0);
    QVERIFY(film(QStringLiteral("The Shining"), 694,
                 QStringLiteral("Horror, Thriller")) > 0);

    const QList<MediaRecord> war =
        m_media->searchOfKind(QStringLiteral("movie"), QStringLiteral("war"), 10);
    QCOMPARE(war.size(), 1);
    QCOMPARE(war.first().title, QStringLiteral("1917"));

    QCOMPARE(m_media->searchOfKind(QStringLiteral("movie"),
                                   QStringLiteral("thri"), 10).size(), 1);

    QCOMPARE(m_media->searchOfKind(QStringLiteral("movie"),
                                   QStringLiteral("rror"), 10).size(), 0);
    QCOMPARE(m_media->searchOfKind(QStringLiteral("movie"),
                                   QStringLiteral("ho"), 10).size(), 0);

    QVERIFY(film(QStringLiteral("War Horse"), 59981,
                 QStringLiteral("Drama, History")) > 0);
    const QList<MediaRecord> both =
        m_media->searchOfKind(QStringLiteral("movie"), QStringLiteral("war"), 10);
    QCOMPARE(both.size(), 2);
    QCOMPARE(both.first().title, QStringLiteral("War Horse"));
}

void TestMediaRepository::aFolderRefusedAsAShowIsRememberedUntilItIsPutBack()
{
    QVERIFY(m_media->discardedShows().isEmpty());

    const QStringList rushHour = {
        QStringLiteral("F:/Downloads/Rush Hour Trilogy/Star.Wars.Episode.5.mkv"),
        QStringLiteral("F:/Downloads/Rush Hour Trilogy/Rush.Hour.2.mkv")
    };
    QVERIFY(m_media->discardShow(rushHour, QStringLiteral("Rush Hour Trilogy"),
                                 QStringLiteral("F:/Downloads/Rush Hour Trilogy")));
    QVERIFY(m_media->discardShow({QStringLiteral("F:/Media/Holiday/clip.mkv")},
                                 QStringLiteral("Holiday"),
                                 QStringLiteral("F:/Media/Holiday")));

    QCOMPARE(m_media->discardedShowFiles().size(), 3);
    QCOMPARE(m_media->discardedShows().size(), 2);

    QVERIFY(m_media->discardShow(rushHour, QStringLiteral("Rush Hour Trilogy"),
                                 QStringLiteral("F:/Downloads/Rush Hour Trilogy")));
    QCOMPARE(m_media->discardedShows().size(), 2);

    const QString path = m_database->filePath();
    m_media.reset();
    m_database->close();

    m_database.reset(new Database);
    QVERIFY(m_database->open(path));
    m_media.reset(new MediaRepository(*m_database));

    QCOMPARE(m_media->discardedShowFiles().size(), 3);

    QVERIFY(m_media->restoreShow(QStringLiteral("Rush Hour Trilogy")));
    QCOMPARE(m_media->discardedShows().size(), 1);
    QCOMPARE(m_media->discardedShows().first().title, QStringLiteral("Holiday"));
    QCOMPARE(m_media->discardedShows().first().fileCount, 1);
    QCOMPARE(m_media->discardedShowFiles(),
             QSet<QString>({QStringLiteral("F:/Media/Holiday/clip.mkv")}));
}

QTEST_GUILESS_MAIN(TestMediaRepository)

#include "tst_mediarepository.moc"
