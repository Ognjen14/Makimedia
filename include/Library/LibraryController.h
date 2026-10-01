#pragma once

#include "Data/FileRepository.h"
#include "Data/FolderRepository.h"
#include "Data/MediaRepository.h"
#include "Data/PlaybackStateRepository.h"
#include "Data/SubtitleRepository.h"
#include "Library/BrowseListModel.h"
#include "Library/BrowseOptions.h"
#include "Library/CollectionListModel.h"
#include "Library/CollectionPageModel.h"
#include "Library/GenreRowModel.h"
#include "Library/LibraryChange.h"
#include "Library/MediaListModel.h"
#include "Library/RemovedFileModel.h"
#include "Library/ShowEpisodeModel.h"
#include "Library/UnmatchedShowModel.h"
#include "Platform/MediaFileInfo.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QQueue>
#include <QString>
#include <QStringList>

#include <functional>

class Database;
class FileListModel;
class FolderListModel;
class PlatformServices;
class ProbeService;
class ThumbnailService;
struct ProbeInfo;

class LibraryController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool ready READ ready CONSTANT FINAL)

    Q_PROPERTY(QStringList videoFormats READ videoFormats CONSTANT FINAL)
    Q_PROPERTY(QStringList subtitleFormats READ subtitleFormats CONSTANT FINAL)
    Q_PROPERTY(QStringList subtitleFolderNames READ subtitleFolderNames
               CONSTANT FINAL)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged FINAL)
    Q_PROPERTY(bool scanQuiet READ scanQuiet NOTIFY scanningChanged FINAL)
    Q_PROPERTY(QString scanningFolderName READ scanningFolderName NOTIFY scanningChanged FINAL)
    Q_PROPERTY(int scanFilesSeen READ scanFilesSeen NOTIFY scanProgressChanged FINAL)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY fileCountChanged FINAL)
    Q_PROPERTY(int unmatchedCount READ unmatchedCount NOTIFY fileCountChanged FINAL)
    Q_PROPERTY(int continueCount READ continueCount NOTIFY fileCountChanged FINAL)
    Q_PROPERTY(QString currentParentHandle READ currentParentHandle NOTIFY viewChanged FINAL)
    Q_PROPERTY(FolderListModel *folders READ folders CONSTANT FINAL)
    Q_PROPERTY(FileListModel *files READ files CONSTANT FINAL)
    Q_PROPERTY(FileListModel *continueFiles READ continueFiles CONSTANT FINAL)
    Q_PROPERTY(FileListModel *searchResults READ searchResults CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *searchMovies READ searchMovies CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *searchShows READ searchShows CONSTANT FINAL)
    Q_PROPERTY(FileListModel *searchEpisodes READ searchEpisodes CONSTANT FINAL)
    Q_PROPERTY(CollectionListModel *searchCollections READ searchCollections CONSTANT FINAL)
    Q_PROPERTY(int searchCount READ searchCount NOTIFY searchChanged FINAL)
    Q_PROPERTY(bool searchMoviesCapped READ searchMoviesCapped NOTIFY searchChanged FINAL)
    Q_PROPERTY(bool searchShowsCapped READ searchShowsCapped NOTIFY searchChanged FINAL)
    Q_PROPERTY(bool searchEpisodesCapped READ searchEpisodesCapped NOTIFY searchChanged FINAL)
    Q_PROPERTY(bool searchFilesCapped READ searchFilesCapped NOTIFY searchChanged FINAL)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchChanged FINAL)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY browseOptionsChanged FINAL)
    Q_PROPERTY(int filterMode READ filterMode WRITE setFilterMode NOTIFY browseOptionsChanged FINAL)
    Q_PROPERTY(bool hasCustomOptions READ hasCustomOptions NOTIFY browseOptionsChanged FINAL)
    Q_PROPERTY(BrowseListModel *browseEntries READ browseEntries CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *movies READ movies CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *shows READ shows CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *suggestions READ suggestions CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *recentTitles READ recentTitles CONSTANT FINAL)
    Q_PROPERTY(GenreRowModel *genreRows READ genreRows CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *movieGrid READ movieGrid CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *showGrid READ showGrid CONSTANT FINAL)
    Q_PROPERTY(int movieSort READ movieSort WRITE setMovieSort NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(int movieFilter READ movieFilter WRITE setMovieFilter NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(int showSort READ showSort WRITE setShowSort NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(int showFilter READ showFilter WRITE setShowFilter NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(QString movieGenre READ movieGenre WRITE setMovieGenre NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(QString showGenre READ showGenre WRITE setShowGenre NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(int movieGridSize READ movieGridSize WRITE setMovieGridSize NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(int showGridSize READ showGridSize WRITE setShowGridSize NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(CollectionListModel *collections READ collections CONSTANT FINAL)
    Q_PROPERTY(CollectionListModel *collectionGrid READ collectionGrid CONSTANT FINAL)
    Q_PROPERTY(CollectionPageModel *collectionPage READ collectionPage CONSTANT FINAL)
    Q_PROPERTY(MediaListModel *pickerTitles READ pickerTitles CONSTANT FINAL)
    Q_PROPERTY(QString pickerQuery READ pickerQuery WRITE setPickerQuery NOTIFY pickerChanged FINAL)
    Q_PROPERTY(int pickerKind READ pickerKind WRITE setPickerKind NOTIFY pickerChanged FINAL)
    Q_PROPERTY(QVariantList hiddenCollections READ hiddenCollections NOTIFY hiddenCollectionsChanged FINAL)
    Q_PROPERTY(QVariantList hiddenCollectionFilms READ hiddenCollectionFilms NOTIFY hiddenCollectionsChanged FINAL)
    Q_PROPERTY(int collectionFilter READ collectionFilter WRITE setCollectionFilter NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(int collectionGridSize READ collectionGridSize WRITE setCollectionGridSize NOTIFY gridOptionsChanged FINAL)
    Q_PROPERTY(QStringList movieGenres READ movieGenres NOTIFY gridGenresChanged FINAL)
    Q_PROPERTY(QStringList showGenres READ showGenres NOTIFY gridGenresChanged FINAL)
    Q_PROPERTY(UnmatchedShowModel *unmatchedShows READ unmatchedShows CONSTANT FINAL)
    Q_PROPERTY(RemovedFileModel *removedFiles READ removedFiles CONSTANT FINAL)
    Q_PROPERTY(QVariantList discardedShows READ discardedShows NOTIFY discardedShowsChanged FINAL)
    Q_PROPERTY(bool isRemote READ isRemote NOTIFY remoteChanged FINAL)
    Q_PROPERTY(ShowEpisodeModel *showEpisodes READ showEpisodes CONSTANT FINAL)
    Q_PROPERTY(QString browsePath READ browsePath NOTIFY browseChanged FINAL)
    Q_PROPERTY(QString browseName READ browseName NOTIFY browseChanged FINAL)
    Q_PROPERTY(bool canBrowseUp READ canBrowseUp NOTIFY browseChanged FINAL)
    Q_PROPERTY(int browseFileCount READ browseFileCount NOTIFY browseChanged FINAL)
    Q_PROPERTY(int browseFolderCount READ browseFolderCount NOTIFY browseChanged FINAL)

public:
    enum SortMode {
        SortRecentlyAdded = BrowseOptions::RecentlyAdded,
        SortTitle = BrowseOptions::Title,
        SortLastPlayed = BrowseOptions::LastPlayed,
        SortFileSize = BrowseOptions::FileSize
    };
    Q_ENUM(SortMode)

    enum FilterMode {
        FilterEverything = BrowseOptions::Everything,
        FilterUnwatched = BrowseOptions::Unwatched,
        FilterInProgress = BrowseOptions::InProgress,
        FilterWatched = BrowseOptions::Watched,
        FilterUnmatched = BrowseOptions::Unmatched,
        FilterSuggested = BrowseOptions::Suggested
    };
    Q_ENUM(FilterMode)

    LibraryController(Database &database,
                      PlatformServices &platform,
                      QObject *parent = nullptr);
    ~LibraryController() override;

    bool ready() const;
    QStringList videoFormats() const;
    QStringList subtitleFormats() const;
    QStringList subtitleFolderNames() const;
    bool scanning() const;
    bool scanQuiet() const;
    QString scanningFolderName() const;
    int scanFilesSeen() const;
    int fileCount() const;
    int unmatchedCount() const;
    int continueCount() const;
    QString currentParentHandle() const;

    FolderListModel *folders() const;
    FileListModel *files() const;
    FileListModel *continueFiles() const;
    FileListModel *searchResults() const;
    MediaListModel *searchMovies() const;
    MediaListModel *searchShows() const;
    FileListModel *searchEpisodes() const;
    CollectionListModel *searchCollections() const;
    int searchCount() const;
    bool searchMoviesCapped() const;
    bool searchShowsCapped() const;
    bool searchEpisodesCapped() const;
    bool searchFilesCapped() const;
    QString searchQuery() const;
    int sortMode() const;
    void setSortMode(int mode);
    int filterMode() const;
    void setFilterMode(int mode);
    bool hasCustomOptions() const;
    Q_INVOKABLE void resetOptions();

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void clearSearch();

    BrowseListModel *browseEntries() const;
    MediaListModel *movies() const;
    MediaListModel *shows() const;
    MediaListModel *suggestions() const;
    MediaListModel *recentTitles() const;
    GenreRowModel *genreRows() const;
    MediaListModel *movieGrid() const;
    MediaListModel *showGrid() const;
    int movieSort() const;
    void setMovieSort(int sort);
    int movieFilter() const;
    void setMovieFilter(int filter);
    int showSort() const;
    void setShowSort(int sort);
    int showFilter() const;
    void setShowFilter(int filter);
    QString movieGenre() const;
    void setMovieGenre(const QString &genre);
    QString showGenre() const;
    void setShowGenre(const QString &genre);
    int movieGridSize() const;
    void setMovieGridSize(int size);
    int showGridSize() const;
    void setShowGridSize(int size);
    QStringList movieGenres() const;
    QStringList showGenres() const;
    CollectionListModel *collections() const;
    CollectionListModel *collectionGrid() const;
    CollectionPageModel *collectionPage() const;
    void refreshCollectionPage();
    MediaListModel *pickerTitles() const;
    QString pickerQuery() const;
    void setPickerQuery(const QString &query);
    int pickerKind() const;
    void setPickerKind(int kind);
    void refreshPickerTitles();
    Q_INVOKABLE qint64 createCollection(const QString &name,
                                        const QString &description,
                                        const QString &coverMode,
                                        const QVariantList &mediaIds);
    Q_INVOKABLE QVariantMap collectionOfMedia(qint64 mediaId) const;
    Q_INVOKABLE void hideCollection(qint64 collectionId, const QString &name);
    Q_INVOKABLE void restoreCollection(qint64 collectionId);
    Q_INVOKABLE void hideCollectionFilm(qint64 collectionId,
                                        qint64 tmdbId,
                                        const QString &title);
    Q_INVOKABLE void restoreCollectionFilm(const QString &scope, qint64 tmdbId);
    Q_INVOKABLE void addFilmToCollection(qint64 collectionId,
                                         qint64 tmdbId,
                                         const QString &title);
    QVariantList hiddenCollections() const;
    QVariantList hiddenCollectionFilms() const;
    int collectionFilter() const;
    void setCollectionFilter(int filter);
    int collectionGridSize() const;
    void setCollectionGridSize(int size);
    void refreshCollections();
    QString scopeOfCollection(qint64 collectionId) const;
    QString nameOfCollection(qint64 collectionId) const;
    QList<UniverseCatalog::Universe> universes() const;
    UnmatchedShowModel *unmatchedShows() const;
    void reloadUnmatchedShows();

    Q_INVOKABLE void discardShow(const QStringList &fileHandles,
                                 const QString &title, const QString &folder);
    Q_INVOKABLE void restoreShow(const QString &title);
    QVariantList discardedShows() const;

    RemovedFileModel *removedFiles() const;

    bool heldStill() const;
    void setHeldStill(bool held);
    bool isRemote() const;
    void setRemote(bool remote);
    void reloadAfterSwitch();
    Q_INVOKABLE void removeFromLibrary(const QString &handle);
    Q_INVOKABLE int hideableUnmatchedCount();
    Q_INVOKABLE void hideAllUnmatched();
    Q_INVOKABLE void restoreRemoved(const QString &handle);
    Q_INVOKABLE void restoreAllRemoved();

    Q_INVOKABLE void reloadMedia();

    Q_INVOKABLE void noteUi(const QString &what) const;
    Q_INVOKABLE QVariantMap surpriseMe(qint64 notThisOne) const;
    ShowEpisodeModel *showEpisodes() const;
    QString browsePath() const;
    QString browseName() const;
    bool canBrowseUp() const;
    int browseFileCount() const;
    int browseFolderCount() const;

    Q_INVOKABLE void browseRoot(qint64 folderId);
    Q_INVOKABLE void browseInto(const QString &handle);
    Q_INVOKABLE void browseRootList();
    Q_INVOKABLE void browseUp();
    Q_INVOKABLE void browseRefresh();

    Q_INVOKABLE void addFolder();
    Q_INVOKABLE void removeFolder(qint64 folderId);

    Q_INVOKABLE void ensureManagedRoots();
    Q_INVOKABLE void adoptStorageVolume(const QString &handle);
    Q_INVOKABLE void declineStorageVolume(const QString &handle);
    Q_INVOKABLE QVariantList declinedStorageVolumes() const;
    Q_INVOKABLE void rescan(qint64 folderId);
    Q_INVOKABLE void rescanAll();
    Q_INVOKABLE void rescanAllQuiet();
    Q_INVOKABLE void rescanStorageQuiet(const QString &volumeName);
    void setScanRowsHeld(bool held);
    Q_INVOKABLE void cancelScan();

    Q_INVOKABLE void showFolder(qint64 folderId);
    Q_INVOKABLE void showAllFiles();

    void applyLibraryChange(const LibraryChange &change);
    void applyArtworkLanded(const QStringList &paths, bool everything);

    Q_INVOKABLE QString mpvUrlFor(const QString &handle);
    Q_INVOKABLE QString handleFromUrl(const QString &url);
    Q_INVOKABLE bool isPlayableFile(const QString &pathOrUrl);
    Q_INVOKABLE QString takeLaunchPath();

    void setLaunchPath(const QString &path);

    Q_INVOKABLE QVariantMap fileInfo(const QString &handle);
    Q_INVOKABLE QVariantList filmCopies(const QString &handle) const;
    Q_INVOKABLE void recordPlayback(const QString &handle,
                                    double positionSeconds,
                                    double durationSeconds,
                                    bool refreshViews = true);
    Q_INVOKABLE double resumePositionFor(const QString &handle);
    Q_INVOKABLE void recordProbe(const QString &handle,
                                 double durationSeconds,
                                 const QString &container,
                                 const QString &videoCodec,
                                 const QString &audioCodec,
                                 int width,
                                 int height,
                                 bool hdr,
                                 int audioTrackCount,
                                 int subtitleTrackCount);
    Q_INVOKABLE QString attachSubtitle(
        const QString &fileHandle, const QString &subtitleUrl,
        const QString &origin = QStringLiteral("manual"));
    Q_INVOKABLE void detachSubtitle(const QString &fileHandle,
                                    const QString &subtitleHandle = QString());
    Q_INVOKABLE QVariantList attachedSubtitles(const QString &fileHandle) const;
    Q_INVOKABLE QString subtitleMpvUrl(const QString &subtitleHandle);
    Q_INVOKABLE QVariantList siblingSubtitleTracks(const QString &fileHandle);
    Q_INVOKABLE bool canRequestSubtitleFolder(const QString &fileHandle) const;
    Q_INVOKABLE void requestSubtitleFolder(const QString &fileHandle);

    Q_INVOKABLE QVariantList previewFilenameParsing() const;
    Q_INVOKABLE QVariantMap parsedNameFor(const QString &fileHandle) const;

    Q_INVOKABLE void clearThumbnailCache();

    void setThumbnailsDeferred(bool deferred);

    void setThumbnailsEnabled(bool enabled);

    void setProbesDeferred(bool deferred);
    void setProbesEnabled(bool enabled);

    void setMatchingBusy(bool busy);
    Q_INVOKABLE void probeUnprobed(bool retryFailed = false);

    Q_INVOKABLE void setWatched(qint64 fileId, bool watched);
    Q_INVOKABLE void setAllWatched(const QStringList &fileHandles, bool watched);
    Q_INVOKABLE void removeFromContinueWatching(qint64 fileId);

signals:
    void scanningChanged();
    void scanProgressChanged();
    void fileCountChanged();
    void viewChanged();
    void browseChanged();
    void searchChanged();
    void browseOptionsChanged();
    void gridOptionsChanged();
    void gridGenresChanged();
    void pickerChanged();
    void hiddenCollectionsChanged();
    void discardedShowsChanged();
    void remoteChanged();
    void folderAdded(const QString &displayName);
    void storageVolumeOffered(const QString &handle, const QString &displayName);
    void subtitleAttached(const QString &fileHandle, const QString &displayName);
    void quietScanFinished(int newFiles);
    void folderUnavailable(const QString &displayName);
    void errorRaised(const QString &message);
    void fileMissing(const QString &handle, const QString &displayName);
    void scanQueueFinished(const QStringList &writtenHandles);
    void newFilesWritten(int count);

private:
    void connectPlatform();
    void beginScan(qint64 folderId);
    void endQuietScan(bool notify);
    bool addManagedRoot(const QString &handle, const QString &displayName);
    void onFolderPicked(const QString &handle, const QString &displayName);
    void onBatchReady(const QString &rootHandle, const QList<MediaFileInfo> &batch);
    void onScanProgress(const QString &rootHandle, int filesSeen);
    void onScanFinished(const QString &rootHandle, int filesFound);
    void onScanCancelled(const QString &rootHandle);
    void onRootUnavailable(const QString &rootHandle);
    void confirmRootUnavailable(const QString &rootHandle);
    QString convertedSubtitle(const QString &subtitleHandle);
    QString keepSubtitleCopy(const QString &subtitleHandle, const QString &displayName);
    void startNextQueuedScan();
    void finishScanState();
    void finishScanQueue();
    void settleMovedFiles();
    void mergeScannedFiles(const QList<LibraryFile> &files);
    void probeFiles(const QStringList &handles, const QList<qint64> &folderIds);
    void placeFiles(const QList<LibraryFile> &files);
    void placeTitles(const QList<qint64> &mediaIds);
    void refreshRecentTitles();
    void refreshGenreRows();
    void refreshGrids();
    void setGridOption(int &option, int value, const QString &key);
    void setGridGenre(QString &option, const QString &value, const QString &key);
    void refreshPlayedFiles(const QList<qint64> &fileIds);
    void recountFiles();
    void reloadFolders();
    void discardThumbnailsWithPosters();
    void trackThumbnailWants(QAbstractItemModel *model,
                             std::function<QString(int)> handleAt,
                             std::function<bool(int)> wantsAt);
    void wantThumbnail(QAbstractItemModel *model, const QString &handle, bool wanted);
    void releaseThumbnailWant(const QString &handle);
    void scheduleThumbnailFlush();
    void flushThumbnailWants();
    void onProbed(const ProbeInfo &info);
    bool refusesWhileHeld(const char *what) const;
    void takeOutOfLibrary(const QList<LibraryFile> &files);
    void rescanFoldersQuiet(const QList<qint64> &folderIds);
    void reloadCurrentView();
    QList<LibraryFile> applyOptions(const QList<LibraryFile> &files) const;

    enum class View {
        Folder,
        All,
        ContinueWatching
    };

    Database &m_database;
    PlatformServices &m_platform;

    FolderRepository m_folderRepository;
    FileRepository m_fileRepository;
    PlaybackStateRepository m_playbackRepository;
    SubtitleRepository m_subtitleRepository;
    MediaRepository m_mediaRepository;

    FolderListModel *m_folderModel = nullptr;
    FileListModel *m_fileModel = nullptr;
    FileListModel *m_continueModel = nullptr;
    FileListModel *m_searchModel = nullptr;
    FileListModel *m_searchEpisodeModel = nullptr;
    MediaListModel *m_searchMovieModel = nullptr;
    MediaListModel *m_searchShowModel = nullptr;
    CollectionListModel *m_searchCollectionModel = nullptr;
    bool m_searchMoviesCapped = false;
    bool m_searchShowsCapped = false;
    bool m_searchEpisodesCapped = false;
    bool m_searchFilesCapped = false;
    ThumbnailService *m_thumbnails = nullptr;
    QHash<QAbstractItemModel *, QSet<QString>> m_thumbnailWantsByModel;
    QHash<QString, int> m_thumbnailWantCounts;
    QSet<QString> m_thumbnailsToWant;
    QStringList m_thumbnailWantOrder;
    QSet<QString> m_thumbnailsToUnwant;
    bool m_thumbnailFlushScheduled = false;
    ProbeService *m_probes = nullptr;
    QStringList m_reportedUnavailable;
    QSettings m_settings;
    QString m_searchQuery;
    QString m_launchPath;
    int m_sortMode = SortRecentlyAdded;
    int m_filterMode = FilterEverything;
    BrowseListModel *m_browseModel = nullptr;
    MediaListModel *m_movieModel = nullptr;
    MediaListModel *m_showModel = nullptr;
    MediaListModel *m_suggestionModel = nullptr;
    MediaListModel *m_recentTitleModel = nullptr;
    GenreRowModel *m_genreRowModel = nullptr;
    MediaListModel *m_movieGridModel = nullptr;
    MediaListModel *m_showGridModel = nullptr;
    CollectionListModel *m_collectionModel = nullptr;
    CollectionListModel *m_collectionGridModel = nullptr;
    CollectionPageModel *m_collectionPageModel = nullptr;
    MediaListModel *m_pickerModel = nullptr;
    QString m_pickerQuery;
    int m_pickerKind = 0;
    int m_collectionFilter = 0;
    int m_collectionGridSize = 1;
    QList<UniverseCatalog::Universe> m_universes;
    QList<CollectionShelf::Summary> m_collections;
    int m_movieSort = 0;
    int m_movieFilter = 0;
    int m_showSort = 0;
    int m_showFilter = 0;
    QString m_movieGenre;
    QString m_showGenre;
    int m_movieGridSize = 1;
    int m_showGridSize = 1;
    QStringList m_movieGenres;
    QStringList m_showGenres;
    UnmatchedShowModel *m_unmatchedShows = nullptr;
    ShowEpisodeModel *m_showEpisodes = nullptr;
    RemovedFileModel *m_removedFiles = nullptr;
    bool m_heldStill = false;
    bool m_scanInterruptedByHold = false;
    bool m_remote = false;
    QStringList m_unmatchedHandles;
    bool m_unmatchedShowsStale = false;
    QString m_browseHandle;
    QString m_browseRootHandle;
    int m_browseFileCount = 0;
    int m_browseFolderCount = 0;

    View m_view = View::All;
    qint64 m_currentFolderId = -1;
    QString m_currentParentHandle;

    bool m_scanning = false;
    bool m_quietScan = false;
    int m_quietScanBaseline = 0;
    QString m_scanningRoot;
    QString m_scanningName;
    int m_scanFilesSeen = 0;
    QQueue<QString> m_scanQueue;

    qint64 m_scanFolderId = -1;
    ScanIndex m_scanIndex;
    bool m_scanIndexLoaded = false;
    QSet<QString> m_scanSeen;
    QStringList m_scanWritten;
    QStringList m_scanMissing;
    bool m_scanRowsHeld = false;
    QStringList m_heldRows;
    QSet<qint64> m_heldFolderIds;
    QList<qint64> m_scanFolderIds;

    int m_fileCount = 0;
    int m_unmatchedCount = 0;
    int m_continueCount = 0;
};
