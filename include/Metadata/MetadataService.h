#pragma once

#include "Data/FileRepository.h"
#include "Data/MediaRepository.h"
#include "Library/LibraryChange.h"
#include "Metadata/FileNameParser.h"
#include "Metadata/MakimediaTmdbConfig.h"
#include "Metadata/PosterCache.h"
#include "Metadata/TmdbAsk.h"
#include "Metadata/TmdbImageUrl.h"
#include "Metadata/TmdbRequestBuilder.h"
#include "Library/UniverseCatalog.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QQueue>

#include <optional>
#include <vector>
#include <QScopedPointer>
#include <QTimer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class AppSettings;
class Database;
class QNetworkAccessManager;

namespace Makimedia::Tmdb {
class TmdbClient;
class PosterUrlResolver;
}

class MetadataService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ available CONSTANT FINAL)
    Q_PROPERTY(int pending READ pending NOTIFY pendingChanged FINAL)
    Q_PROPERTY(int matchedCount READ matchedCount NOTIFY matchesChanged FINAL)
    Q_PROPERTY(int suggestedCount READ suggestedCount NOTIFY matchesChanged FINAL)
    Q_PROPERTY(int artworkRevision READ artworkRevision NOTIFY artworkChanged FINAL)

    Q_PROPERTY(bool creditsSweeping READ creditsSweeping
               NOTIFY backgroundWorkChanged FINAL)
    Q_PROPERTY(int creditsRemaining READ creditsRemaining
               NOTIFY backgroundWorkChanged FINAL)
    Q_PROPERTY(bool artworkWarming READ artworkWarming
               NOTIFY backgroundWorkChanged FINAL)
    Q_PROPERTY(int artworkRemaining READ artworkRemaining
               NOTIFY backgroundWorkChanged FINAL)

public:
    MetadataService(const AppSettings &settings,
                    Database &database,
                    QObject *parent = nullptr);
    ~MetadataService() override;

    bool available() const;
    int pending() const;
    int matchedCount() const;
    int suggestedCount() const;
    int artworkRevision() const;
    bool creditsSweeping() const;
    int creditsRemaining() const;
    bool artworkWarming() const;
    int artworkRemaining() const;

    Q_INVOKABLE QVariantMap metadataForFile(const QString &fileHandle) const;
    Q_INVOKABLE QVariantMap nextEpisodeFor(const QString &fileHandle) const;
    Q_INVOKABLE QString posterUrl(const QString &posterPath, int pixelWidth) const;
    Q_INVOKABLE QString stillUrl(const QString &stillPath, int pixelWidth) const;
    Q_INVOKABLE QString backdropUrl(const QString &backdropPath, int pixelWidth) const;
    Q_INVOKABLE QString profileUrl(const QString &profilePath, int pixelWidth) const;
    Q_INVOKABLE void clearPosterCache();

    Q_INVOKABLE void previewMatch(const QString &fileHandle, const QString &fileName);
    Q_INVOKABLE void matchLibrary();
    Q_INVOKABLE void matchUnmatched();
    Q_INVOKABLE void matchUnmatchedAgain();
    void matchScanned(const QStringList &fileHandles);
    Q_INVOKABLE void ensureSeasons(qint64 mediaId);
    Q_INVOKABLE void ensureCollectionFilms();
    Q_INVOKABLE void searchCandidates(const QString &query, bool tv);
    Q_INVOKABLE bool pinMatch(const QString &fileHandle,
                              const QVariantMap &candidate);
    Q_INVOKABLE void unpinMatch(const QString &fileHandle);
    Q_INVOKABLE int pinMatchForShow(const QStringList &fileHandles,
                                    const QVariantMap &candidate);

    Q_INVOKABLE void clearMatch(const QString &fileHandle);

    Q_INVOKABLE void clearAllMatches();
    Q_INVOKABLE void cancelAll();

    void setHeldForStreaming(bool held);
    void forgetLibraryState();
    void readLibraryAfterSwitch();
    void setArtworkServer(const QString &baseUrl);

    void warmArtworkKey(const QString &key);

    Q_INVOKABLE void prefetchArtwork();

    void setHeldForSetup(bool held);
    void setUniverses(const QList<UniverseCatalog::Universe> &universes);
    void readForSetup();
    void matchForSetup(bool oneTitleAtATime);
    void collectionsForSetup(bool oneAtATime);
    void downloadArtworkForSetup(bool television);

signals:
    void setupReadProgress(int done, int total);
    void setupReadFinished(int files, int titles);
    void setupMatchProgress(int done, int total);
    void setupMatchFinished(int unreachable);
    void setupCollectionsProgress(int done, int total);
    void setupCollectionsFinished(int failed);
    void setupArtworkProgress(int done, int total);
    void setupArtworkFinished(int failed);

    void pendingChanged();
    void matchesChanged();
    void matchesChangedFor(const QStringList &fileHandles, const QVariantList &mediaIds);
    void libraryChanged(const LibraryChange &change);
    void artworkChanged();
    void backgroundWorkChanged();
    void collectionsChanged();
    void artworkLanded(const QStringList &paths, bool everything);
    void previewReady(const QVariantMap &result);
    void candidatesReady(const QVariantList &candidates);

private:
    struct PinResult
    {
        qint64 mediaId = 0;
        int season = 0;
    };

    PinResult applyPin(const LibraryFile &file, const QVariantMap &candidate,
                       bool fetchArtwork, int episodeHint = 0);

    struct WaitingMatch
    {
        QVariantMap base;
        ParsedFileName parsed;
        QString fileHandle;
        bool write = false;
        bool usedYear = false;
        qint64 fileId = 0;
    };

    void queueMatches(const QList<LibraryFile> &files, const char *what,
                      bool forSetup = false);
    void drainMatches();
    void runMatch(const QString &fileHandle, const QString &fileName,
                  const QString &folderHandle, bool write, qint64 fileId = 0);
    void runParsedMatch(const ParsedFileName &parsed, const QString &fileHandle,
                        const QString &fileName, bool write, qint64 fileId);
    void readSetupSlice();
    void checkSetupMatchDone();
    void startNextSetupTitle();
    void startSetupImages();
    void pumpSetupImages();
    void settleSetupImage(const QString &key, bool saved);
    void finishSetupImages();
    void searchAndScore(const ParsedFileName &parsed, const QString &fileHandle,
                        const QVariantMap &base, bool write, bool useYear);

    void recordNoMatch(qint64 fileId, const QString &fileHandle, const QString &why);

    void announcePending();
    void flushPending();
    void noteRelink(const LibraryFile &file, const FileMediaLink &before,
                    qint64 mediaAfter, bool suggestedAfter);
    void noteTitleChanged(qint64 mediaId);

    void startCreditsSweep();
    void sweepCredits();
    void noteBackgroundWork(int creditsLeft, int artworkLeft);

    void storeTitleCredits(qint64 mediaId,
                           const Makimedia::Tmdb::TmdbTitleDetailsDto &details);
    void storeEpisodeCredits(qint64 mediaId, qint64 episodeId,
                             const Makimedia::Tmdb::TmdbEpisodeDto &episode);
    void flushChanges();

    enum class ImageUse { Shown, Warmed };

    QString imageUrl(Makimedia::Tmdb::ImageKind kind,
                     const QString &path,
                     int pixelWidth,
                     ImageUse use) const;
    static QString imageKey(Makimedia::Tmdb::ImageKind kind,
                            const QString &path,
                            int width);
    static QString pathOfImageKey(const QString &key);

    void drainMissedArtwork();
    void fetchArtworkKey(const QString &key);

    void drainPrefetch();

    void deliverSearch(const QString &key,
                       const std::vector<Makimedia::Tmdb::TmdbTitleResultDto> &results);
    bool askAgainAnotherWay(const WaitingMatch &waiting);
    bool askAgainWithTheOtherName(const WaitingMatch &waiting);
    void completeMatch(const WaitingMatch &waiting,
                       const std::vector<Makimedia::Tmdb::TmdbTitleResultDto> &results);
    bool fetchDetails(qint64 tmdbId, const QString &kind);
    bool fetchSeason(qint64 tmdbId, qint64 mediaId, int seasonNumber,
                     bool evenIfFetched = false);
    void checkCollections();
    void noteFilmCollection(qint64 mediaId, qint64 tmdbId,
                            const std::optional<Makimedia::Tmdb::TmdbCollectionRefDto> &collection);
    bool fetchCollection(qint64 collectionId);
    bool fetchFilmDetails(qint64 tmdbId);
    QList<qint64> filmsWantingDetails(int *candidateCount) const;
    void startSetupCollectionFilms();
    void settleSetupCollectionWork(const QString &key, bool fetched);
    void finishSetupCollections();
    void finish(const QVariantMap &result);
    void applyResult(qint64 fileId,
                     const QString &fileHandle,
                     const ParsedFileName &parsed,
                     const QVariantMap &result);

    Database &m_database;
    FileRepository m_fileRepository;
    MediaRepository m_mediaRepository;
    QScopedPointer<QNetworkAccessManager> m_network;
    QScopedPointer<Makimedia::Tmdb::MakimediaTmdbConfig> m_config;
    QScopedPointer<Makimedia::Tmdb::TmdbRequestBuilder> m_requestBuilder;
    QScopedPointer<Makimedia::Tmdb::TmdbClient> m_client;
    QScopedPointer<Makimedia::Tmdb::PosterUrlResolver> m_posters;
    QScopedPointer<PosterCache> m_posterCache;
    TmdbAsk::Ledger<> m_seasonAsks;

    TmdbAsk::Ledger<> m_detailsAsks;
    TmdbAsk::Ledger<> m_collectionAsks;
    QTimer m_collectionCheckStart;
    QTimer m_collectionsChangedSoon;
    TmdbAsk::Ledger<> m_filmAsks;
    bool m_setupCollectionsRunning = false;
    bool m_setupCollectionsOneAtATime = false;
    int m_setupCollectionsStage = 0;
    int m_setupCollectionsDone = 0;
    int m_setupCollectionsTotal = 0;
    int m_setupCollectionsFailed = 0;
    QSet<QString> m_setupCollectionsWaiting;
    QList<UniverseCatalog::Universe> m_universes;
    TmdbAsk::Ledger<std::vector<Makimedia::Tmdb::TmdbTitleResultDto>, WaitingMatch>
        m_searchAsks;

    struct PrefetchItem
    {
        enum Kind { Poster, Backdrop, Still, Profile };
        Kind kind = Poster;
        QString path;
    };
    QList<PrefetchItem> m_prefetchQueue;
    QSet<qint64> m_warmTitles;
    bool m_warmedAll = false;
    bool m_warmAgain = false;
    mutable QSet<QString> m_prefetchedKeys;
    mutable QSet<QString> m_fromServer;
    int m_landedFromServer = 0;
    int m_landedFromTmdb = 0;
    int m_missedOnServer = 0;
    QTimer m_prefetchTimer;
    QTimer m_prefetchStart;
    QTimer m_artworkRefresh;
    QTimer m_creditsSweep;
    bool m_creditsSweepDone = false;
    int m_creditsSwept = 0;
    int m_creditsLeft = 0;
    int m_artworkLeft = 0;
    int m_warmDownloads = 0;

    static constexpr int kMissedArtworkCap = 2000;
    QQueue<QString> m_missedArtwork;
    QTimer m_missedDrain;
    int m_pending = 0;
    QQueue<LibraryFile> m_matchQueue;
    QSet<qint64> m_matching;
    QTimer m_matchDrain;
    int m_announcedPending = 0;
    QTimer m_pendingFlush;
    int m_artworkRevision = 0;
    int m_matchedCount = 0;
    int m_suggestedCount = 0;
    QSet<qint64> m_changedFiles;
    QSet<qint64> m_changedMedia;
    QSet<QString> m_changedHandles;
    QSet<QString> m_landedPaths;
    int m_filesMatched = 0;
    int m_filesUnmatched = 0;
    QTimer m_changeFlush;

    bool m_heldForSetup = false;
    bool m_heldForStreaming = false;
    QString m_artworkServer;
    QSet<QString> m_notOnArtworkServer;
    bool refusesWhileStreaming(const char *what) const;
    bool m_matchAskedWhileHeld = false;
    QList<LibraryFile> m_setupToRead;
    int m_setupReadIndex = 0;
    QHash<qint64, ParsedFileName> m_setupParsed;
    QSet<QString> m_setupTitles;
    QTimer m_setupReadTimer;
    QSet<qint64> m_setupMatching;
    int m_setupMatchTotal = 0;
    int m_setupUnreachable = 0;
    bool m_setupMatchRunning = false;
    bool m_setupOneTitleAtATime = false;
    QList<QList<LibraryFile>> m_setupTitleQueue;
    QSet<qint64> m_setupTitleFiles;
    int m_setupTitlesStarted = 0;
    int m_setupTitlesTotal = 0;

    struct SetupImage
    {
        Makimedia::Tmdb::ImageKind kind;
        QString path;
        int width = 0;
        QString key;
    };
    QList<SetupImage> m_setupImages;
    QSet<QString> m_setupImagesInFlight;
    int m_setupImagesTotal = 0;
    int m_setupImagesSettled = 0;
    int m_setupImagesFailed = 0;
    int m_setupImagesParallel = 1;
    bool m_setupImagesRunning = false;
    bool m_setupImagesPumping = false;
    bool m_setupImagesTelevision = false;
    QTimer m_setupImagesWait;
};
