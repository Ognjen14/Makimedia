#include "Library/LibraryController.h"

#include "Data/Database.h"
#include "MmLog.h"
#include "Streaming/StreamProtocol.h"
#include "Library/FileListModel.h"
#include "Library/BrowseListModel.h"
#include "Library/CollectionShelf.h"
#include "Library/FolderListModel.h"
#include "Library/MediaHandle.h"
#include "Library/MediaListModel.h"
#include "Library/ProbeService.h"
#include "Library/ThumbnailService.h"
#include "Metadata/FileNameParser.h"
#include "TextFold.h"
#include "Library/SubtitleEncoding.h"
#include "Library/SubtitleFinder.h"
#include "Library/SubtitleNaming.h"
#include "Library/TitleArrangement.h"
#include "Metadata/ShowGrouping.h"
#include "Platform/IFolderPicker.h"
#include "Platform/IMediaSource.h"
#include "Platform/IScanner.h"
#include "Platform/PlatformServices.h"

#ifdef Q_OS_ANDROID
#  include "Platform/Android/AndroidMediaSource.h"
#endif

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QHash>
#include <QSet>
#include <QLocale>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <iterator>

namespace {

constexpr int kHomeRowLength = 50;

bool continuesWatching(const LibraryFile &file)
{
    return file.isValid()
        && !file.missing
        && file.playback.lastPlayed.isValid()
        && PlaybackStateRepository::resumable(file.playback.positionSeconds,
                                              file.playback.durationSeconds,
                                              file.playback.watched);
}

}

LibraryController::LibraryController(Database &database,
                                     PlatformServices &platform,
                                     QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_platform(platform)
    , m_folderRepository(database)
    , m_fileRepository(database)
    , m_playbackRepository(database)
    , m_subtitleRepository(database)
    , m_mediaRepository(database)
    , m_folderModel(new FolderListModel(this))
    , m_fileModel(new FileListModel(this))
    , m_continueModel(new FileListModel(this))
    , m_searchModel(new FileListModel(this))
    , m_searchEpisodeModel(new FileListModel(this))
    , m_searchMovieModel(new MediaListModel(this))
    , m_searchShowModel(new MediaListModel(this))
    , m_searchCollectionModel(new CollectionListModel(this))
    , m_browseModel(new BrowseListModel(this))
    , m_movieModel(new MediaListModel(this))
    , m_showModel(new MediaListModel(this))
    , m_suggestionModel(new MediaListModel(this))
    , m_recentTitleModel(new MediaListModel(this))
    , m_genreRowModel(new GenreRowModel(this))
    , m_movieGridModel(new MediaListModel(this))
    , m_showGridModel(new MediaListModel(this))
    , m_collectionModel(new CollectionListModel(this))
    , m_collectionGridModel(new CollectionListModel(this))
    , m_collectionPageModel(new CollectionPageModel(this))
    , m_pickerModel(new MediaListModel(this))
    , m_unmatchedShows(new UnmatchedShowModel(this))
    , m_showEpisodes(new ShowEpisodeModel(m_mediaRepository, this))
    , m_removedFiles(new RemovedFileModel(this))
{
    m_thumbnails = new ThumbnailService(this);
    m_thumbnails->setMediaSource(m_platform.mediaSource());

    m_probes = new ProbeService(this);
    m_probes->setMediaSource(m_platform.mediaSource());
    connect(m_probes, &ProbeService::probed, this,
            &LibraryController::onProbed);

    const QList<FileListModel *> models = {
        m_fileModel, m_continueModel, m_searchModel, m_searchEpisodeModel
    };
    for (FileListModel *model : models) {
        model->setThumbnailService(m_thumbnails);
        connect(m_thumbnails, &ThumbnailService::thumbnailReady, model,
                [model](const QString &handle, const QString &url) {
            Q_UNUSED(url)
            model->onThumbnailReady(handle);
        });
        trackThumbnailWants(model,
                            [model](int row) { return model->handleAt(row); },
                            [model](int row) { return model->wantsThumbnailAt(row); });
    }

    m_browseModel->setThumbnailService(m_thumbnails);
    connect(m_thumbnails, &ThumbnailService::thumbnailReady, m_browseModel,
            [this](const QString &handle, const QString &url) {
        Q_UNUSED(url)
        m_browseModel->onThumbnailReady(handle);
    });
    trackThumbnailWants(m_browseModel,
                        [this](int row) { return m_browseModel->handleAt(row); },
                        [this](int row) { return m_browseModel->wantsThumbnailAt(row); });

    connectPlatform();

    m_removedFiles->setFiles(m_fileRepository.removedFiles());

    m_movieSort = m_settings.value(QStringLiteral("movies/sort"), 0).toInt();
    m_movieFilter = m_settings.value(QStringLiteral("movies/filter"), 0).toInt();
    m_showSort = m_settings.value(QStringLiteral("shows/sort"), 0).toInt();
    m_showFilter = m_settings.value(QStringLiteral("shows/filter"), 0).toInt();
    m_movieGenre = m_settings.value(QStringLiteral("movies/genre")).toString();
    m_showGenre = m_settings.value(QStringLiteral("shows/genre")).toString();
    m_movieGridSize = m_settings.value(QStringLiteral("movies/gridSize"), 1).toInt();
    m_showGridSize = m_settings.value(QStringLiteral("shows/gridSize"), 1).toInt();
    m_collectionFilter = m_settings.value(QStringLiteral("collections/filter"), 0).toInt();
    m_collectionGridSize = m_settings.value(QStringLiteral("collections/gridSize"), 1).toInt();
    m_universes = UniverseCatalog::load(QStringLiteral(":/assets/universes.json"));
    m_collectionPageModel->setOrder(
        m_settings.value(QStringLiteral("collections/order"), CollectionPage::Release).toInt());
    connect(m_collectionPageModel, &CollectionPageModel::collectionIdChanged, this,
            &LibraryController::refreshCollectionPage);
    connect(m_collectionPageModel, &CollectionPageModel::orderChanged, this, [this]() {
        m_settings.setValue(QStringLiteral("collections/order"), m_collectionPageModel->order());
        MM_LOG_I() << "collection page order ->"
                   << (m_collectionPageModel->order() == CollectionPage::Story ? "story" : "release");
        refreshCollectionPage();
    });

    recountFiles();
    reloadFolders();
    reloadCurrentView();
    reloadMedia();
    if (!m_settings.value(QStringLiteral("thumbnails/posterSweepDone")).toBool()) {
        discardThumbnailsWithPosters();
        m_settings.setValue(QStringLiteral("thumbnails/posterSweepDone"), true);
    }
    MM_LOG_I() << "library controller ready, files in database"
               << m_fileCount;
}

LibraryController::~LibraryController() = default;

bool LibraryController::ready() const
{
    return m_database.isOpen() && m_platform.available();
}

QStringList LibraryController::videoFormats() const
{
    return MediaFormats::videoSuffixes();
}

QStringList LibraryController::subtitleFormats() const
{
    return SubtitleNaming::subtitleSuffixes();
}

QStringList LibraryController::subtitleFolderNames() const
{
    return SubtitleFinder::subtitleFolderNames();
}

bool LibraryController::scanning() const
{
    return m_scanning;
}

QString LibraryController::scanningFolderName() const
{
    return m_scanningName;
}

int LibraryController::scanFilesSeen() const
{
    return m_scanFilesSeen;
}

int LibraryController::fileCount() const
{
    return m_fileCount;
}

int LibraryController::unmatchedCount() const
{
    return m_unmatchedCount;
}

int LibraryController::continueCount() const
{
    return m_continueCount;
}

QString LibraryController::currentParentHandle() const
{
    return m_currentParentHandle;
}

FolderListModel *LibraryController::folders() const
{
    return m_folderModel;
}

FileListModel *LibraryController::files() const
{
    return m_fileModel;
}

FileListModel *LibraryController::continueFiles() const
{
    return m_continueModel;
}


FileListModel *LibraryController::searchResults() const
{
    return m_searchModel;
}

MediaListModel *LibraryController::searchMovies() const
{
    return m_searchMovieModel;
}

MediaListModel *LibraryController::searchShows() const
{
    return m_searchShowModel;
}

FileListModel *LibraryController::searchEpisodes() const
{
    return m_searchEpisodeModel;
}

CollectionListModel *LibraryController::searchCollections() const
{
    return m_searchCollectionModel;
}

int LibraryController::searchCount() const
{
    return m_searchMovieModel->rowCount() + m_searchShowModel->rowCount()
        + m_searchEpisodeModel->rowCount() + m_searchCollectionModel->rowCount()
        + m_searchModel->rowCount();
}

bool LibraryController::searchMoviesCapped() const
{
    return m_searchMoviesCapped;
}

bool LibraryController::searchShowsCapped() const
{
    return m_searchShowsCapped;
}

bool LibraryController::searchEpisodesCapped() const
{
    return m_searchEpisodesCapped;
}

bool LibraryController::searchFilesCapped() const
{
    return m_searchFilesCapped;
}

QString LibraryController::searchQuery() const
{
    return m_searchQuery;
}

int LibraryController::sortMode() const
{
    return m_sortMode;
}

void LibraryController::setSortMode(int mode)
{
    if (m_sortMode == mode) {
        return;
    }
    m_sortMode = mode;
    MM_LOG_I() << "sort mode ->" << mode;
    reloadCurrentView();
    emit browseOptionsChanged();
}

int LibraryController::filterMode() const
{
    return m_filterMode;
}

void LibraryController::setFilterMode(int mode)
{
    if (m_filterMode == mode) {
        return;
    }
    m_filterMode = mode;
    MM_LOG_I() << "filter mode ->" << mode;
    reloadCurrentView();
    emit browseOptionsChanged();
}

bool LibraryController::hasCustomOptions() const
{
    return m_sortMode != SortRecentlyAdded || m_filterMode != FilterEverything;
}

void LibraryController::resetOptions()
{
    if (!hasCustomOptions()) {
        return;
    }
    m_sortMode = SortRecentlyAdded;
    m_filterMode = FilterEverything;
    MM_LOG_I() << "sort and filter reset to defaults";
    reloadCurrentView();
    emit browseOptionsChanged();
}

QList<LibraryFile> LibraryController::applyOptions(const QList<LibraryFile> &files) const
{
    QElapsedTimer timer;
    timer.start();

    const QList<LibraryFile> result =
        BrowseOptions::apply(files, m_sortMode, m_filterMode);

    MM_LOG_D() << "sort" << m_sortMode << "filter" << m_filterMode
               << "->" << result.size() << "of" << files.size() << "files in"
               << timer.elapsed() << "ms";
    return result;
}

void LibraryController::search(const QString &query)
{
    if (m_searchQuery == query) {
        return;
    }

    m_searchQuery = query;

    const int limit = 60;

    QList<MediaRecord> movies =
        m_mediaRepository.searchOfKind(QStringLiteral("movie"), query, limit + 1);
    QList<MediaRecord> shows =
        m_mediaRepository.searchOfKind(QStringLiteral("tv"), query, limit + 1);
    QList<LibraryFile> episodes = m_fileRepository.searchEpisodes(query, limit + 1);
    QList<LibraryFile> loose = m_fileRepository.searchUnmatched(query, limit + 1);

    const auto trim = [limit](auto &hits) {
        if (hits.size() <= limit) {
            return false;
        }
        while (hits.size() > limit) {
            hits.removeLast();
        }
        return true;
    };

    m_searchMoviesCapped = trim(movies);
    m_searchShowsCapped = trim(shows);
    m_searchEpisodesCapped = trim(episodes);
    m_searchFilesCapped = trim(loose);

    const QString needle = TextFold::key(query.trimmed());
    QList<CollectionShelf::Summary> collections;
    for (const CollectionShelf::Summary &collection : std::as_const(m_collections)) {
        if (TextFold::key(collection.name).contains(needle)) {
            collections.append(collection);
        }
    }

    m_searchMovieModel->setItems(movies);
    m_searchShowModel->setItems(shows);
    m_searchEpisodeModel->setFiles(episodes);
    m_searchCollectionModel->setItems(collections);
    m_searchModel->setFiles(loose);

    MM_LOG_I() << "search" << query << "->" << movies.size() << "films,"
               << shows.size() << "shows," << episodes.size() << "episodes,"
               << collections.size() << "collections," << loose.size()
               << "unmatched files";
    emit searchChanged();
}

void LibraryController::clearSearch()
{
    if (m_searchQuery.isEmpty()) {
        return;
    }
    m_searchQuery.clear();
    m_searchModel->setFiles(QList<LibraryFile>());
    m_searchEpisodeModel->setFiles(QList<LibraryFile>());
    m_searchMovieModel->setItems(QList<MediaRecord>());
    m_searchShowModel->setItems(QList<MediaRecord>());
    m_searchCollectionModel->setItems(QList<CollectionShelf::Summary>());
    m_searchMoviesCapped = false;
    m_searchShowsCapped = false;
    m_searchEpisodesCapped = false;
    m_searchFilesCapped = false;
    emit searchChanged();
}

BrowseListModel *LibraryController::browseEntries() const
{
    return m_browseModel;
}

QString LibraryController::browsePath() const
{
    IMediaSource *source = m_platform.mediaSource();
    if (!source || m_browseHandle.isEmpty()) {
        return QString();
    }
    return source->displayPath(m_browseHandle);
}

QString LibraryController::browseName() const
{
    if (m_browseHandle.isEmpty()) {
        return QString();
    }
    const int slash = qMax(m_browseHandle.lastIndexOf(QLatin1Char('/')),
                           m_browseHandle.lastIndexOf(QLatin1Char('\\')));
    return slash >= 0 ? m_browseHandle.mid(slash + 1) : m_browseHandle;
}

bool LibraryController::canBrowseUp() const
{
    return !m_browseHandle.isEmpty()
        && !m_browseRootHandle.isEmpty()
        && m_browseHandle != m_browseRootHandle;
}

int LibraryController::browseFileCount() const
{
    return m_browseFileCount;
}

int LibraryController::browseFolderCount() const
{
    return m_browseFolderCount;
}

void LibraryController::browseRoot(qint64 folderId)
{
    const ScanFolder folder = m_folderRepository.byId(folderId);
    if (!folder.isValid()) {
        return;
    }
    m_browseRootHandle = folder.handle;
    m_browseHandle = folder.handle;
    MM_LOG_I() << "browsing root" << folder.handle;
    browseRefresh();
}

void LibraryController::browseInto(const QString &handle)
{
    if (handle.isEmpty()) {
        return;
    }
    m_browseHandle = handle;
    if (m_browseRootHandle.isEmpty()) {
        m_browseRootHandle = handle;
    }
    MM_LOG_D() << "browsing into" << handle;
    browseRefresh();
}

void LibraryController::browseRootList()
{
    if (m_browseHandle.isEmpty() && m_browseRootHandle.isEmpty()) {
        return;
    }
    MM_LOG_D() << "leaving browse tree" << m_browseHandle << "for the root list";
    m_browseHandle.clear();
    m_browseRootHandle.clear();
    browseRefresh();
}

void LibraryController::browseUp()
{
    IMediaSource *source = m_platform.mediaSource();
    if (!source || !canBrowseUp()) {
        return;
    }
    const QString parent = source->parentOf(m_browseHandle);
    if (parent.isEmpty()) {
        return;
    }
    m_browseHandle = parent;
    MM_LOG_D() << "browsing up to" << parent;
    browseRefresh();
}

void LibraryController::browseRefresh()
{
    IMediaSource *source = m_platform.mediaSource();
    if (!source || m_browseHandle.isEmpty()) {
        m_browseModel->setEntries(QList<BrowseEntry>());
        m_browseFileCount = 0;
        m_browseFolderCount = 0;
        emit browseChanged();
        return;
    }

    if (!source->isRootAvailable(m_browseHandle)) {
        MM_LOG_W() << "browse target unavailable" << m_browseHandle;
        m_browseModel->setEntries(QList<BrowseEntry>());
        m_browseFileCount = 0;
        m_browseFolderCount = 0;
        emit browseChanged();
        return;
    }

    QElapsedTimer timer;
    timer.start();

    const QList<MediaFileInfo> children = source->listChildren(m_browseHandle);
    const QHash<QString, FolderSummary> summaries =
        m_fileRepository.summariesBelow(m_browseHandle);
    const QHash<QString, LibraryFile> indexed =
        m_fileRepository.filesIn(m_browseHandle);

    QList<BrowseEntry> entries;
    entries.reserve(children.size());
    int files = 0;
    int folders = 0;

    for (const MediaFileInfo &child : children) {
        BrowseEntry entry;
        entry.handle = child.handle;
        entry.displayName = child.displayName;
        entry.isFolder = child.isFolder;
        entry.sizeBytes = child.sizeBytes;

        if (child.isFolder) {
            ++folders;
            const FolderSummary summary = summaries.value(child.handle);
            entry.childFileCount = summary.fileCount;
            entry.childBytes = summary.totalBytes;
        } else {
            ++files;
            const auto found = indexed.constFind(child.handle);
            if (found != indexed.constEnd()) {
                const LibraryFile &known = found.value();
                entry.indexed = true;
                entry.durationSeconds = known.playback.durationSeconds > 0.0
                    ? known.playback.durationSeconds
                    : known.durationSeconds;
                entry.progress = known.playback.progress();
                entry.watched = known.playback.watched;
                entry.matchedPosterPath = known.matchedPosterPath;
            }
        }

        entries.append(entry);
    }

    m_browseFileCount = files;
    m_browseFolderCount = folders;
    m_browseModel->setEntries(entries);
    MM_LOG_I() << "browse listing" << m_browseHandle
               << folders << "folders," << files << "files in"
               << timer.elapsed() << "ms";
    emit browseChanged();
}

void LibraryController::connectPlatform()
{
    IFolderPicker *picker = m_platform.folderPicker();
    if (picker) {
        connect(picker, &IFolderPicker::folderPicked,
                this, &LibraryController::onFolderPicked);
        connect(picker, &IFolderPicker::folderPickFailed,
                this, &LibraryController::errorRaised);
    }

    IScanner *scanner = m_platform.scanner();
    if (scanner) {
        connect(scanner, &IScanner::batchReady,
                this, &LibraryController::onBatchReady);
        connect(scanner, &IScanner::scanProgress,
                this, &LibraryController::onScanProgress);
        connect(scanner, &IScanner::scanFinished,
                this, &LibraryController::onScanFinished);
        connect(scanner, &IScanner::scanCancelled,
                this, &LibraryController::onScanCancelled);
        connect(scanner, &IScanner::rootUnavailable,
                this, &LibraryController::onRootUnavailable);
    }

    if (!scanner) {
        MM_LOG_E() << "library controller started without a scanner;"
                   << "nothing can be indexed";
    }
    if (!picker) {
        MM_LOG_I() << "no folder picker on this platform;"
                   << "storage roots are managed instead";
    }
}

void LibraryController::addFolder()
{
    if (refusesWhileHeld("adding a folder")) {
        return;
    }
    IFolderPicker *picker = m_platform.folderPicker();
    if (!picker) {
        emit errorRaised(tr("Adding folders is not available on this platform yet."));
        return;
    }
    MM_LOG_I() << "folder pick requested";
    picker->requestFolder();
}

bool LibraryController::addManagedRoot(const QString &handle, const QString &displayName)
{
    const QList<ScanFolder> existing = m_folderRepository.all();
    for (const ScanFolder &folder : existing) {
        if (folder.handle == handle) {
            return false;
        }
    }

    const qint64 id = m_folderRepository.add(handle, displayName);
    if (id < 0) {
        MM_LOG_E() << "could not save managed scan root" << handle;
        return false;
    }

    MM_LOG_I() << "managed scan root added" << handle << "as" << displayName;
    reloadFolders();
    rescan(id);
    return true;
}

void LibraryController::ensureManagedRoots()
{
    if (refusesWhileHeld("offering drives")) {
        return;
    }
#ifdef Q_OS_ANDROID
    const QString primary = AndroidMediaSource::primaryRoot();
    if (primary.isEmpty()) {
        MM_LOG_W() << "no readable internal storage; the video permission is"
                   << "probably not granted yet";
        return;
    }

    addManagedRoot(primary, AndroidMediaSource::volumeLabel(primary));

    const QStringList declined =
        m_settings.value(QStringLiteral("declinedStorageVolumes")).toStringList();
    const QList<ScanFolder> known = m_folderRepository.all();

    for (const QString &volume : AndroidMediaSource::storageRoots()) {
        if (volume == primary || declined.contains(volume)) {
            continue;
        }

        bool alreadyKnown = false;
        for (const ScanFolder &folder : known) {
            if (folder.handle == volume) {
                alreadyKnown = true;
                break;
            }
        }
        if (alreadyKnown) {
            continue;
        }

        const QString label = AndroidMediaSource::volumeLabel(volume);
        MM_LOG_I() << "offering external storage volume" << volume << "as" << label;
        emit storageVolumeOffered(volume, label);
    }
#endif
}

void LibraryController::adoptStorageVolume(const QString &handle)
{
    if (refusesWhileHeld("adding a drive")) {
        return;
    }
#ifdef Q_OS_ANDROID
    QStringList declined =
        m_settings.value(QStringLiteral("declinedStorageVolumes")).toStringList();
    if (declined.removeAll(handle) > 0) {
        m_settings.setValue(QStringLiteral("declinedStorageVolumes"), declined);
    }

    if (addManagedRoot(handle, AndroidMediaSource::volumeLabel(handle))) {
        emit folderAdded(AndroidMediaSource::volumeLabel(handle));
    }
#else
    Q_UNUSED(handle)
#endif
}

void LibraryController::declineStorageVolume(const QString &handle)
{
    QStringList declined =
        m_settings.value(QStringLiteral("declinedStorageVolumes")).toStringList();
    if (!declined.contains(handle)) {
        declined.append(handle);
        m_settings.setValue(QStringLiteral("declinedStorageVolumes"), declined);
    }
    MM_LOG_I() << "storage volume declined, it will not be offered again" << handle;
}

QVariantList LibraryController::declinedStorageVolumes() const
{
    QVariantList result;
#ifdef Q_OS_ANDROID
    const QStringList declined =
        m_settings.value(QStringLiteral("declinedStorageVolumes")).toStringList();
    for (const QString &handle : declined) {
        QVariantMap entry;
        entry.insert(QStringLiteral("handle"), handle);
        entry.insert(QStringLiteral("displayName"),
                     AndroidMediaSource::volumeLabel(handle));
        result.append(entry);
    }
#endif
    return result;
}

void LibraryController::onFolderPicked(const QString &handle, const QString &displayName)
{
    const QString candidate = handle.endsWith(QLatin1Char('/'))
        ? handle
        : handle + QLatin1Char('/');

    const QList<ScanFolder> existing = m_folderRepository.all();
    for (const ScanFolder &folder : existing) {
        const QString other = folder.handle.endsWith(QLatin1Char('/'))
            ? folder.handle
            : folder.handle + QLatin1Char('/');

        if (candidate == other) {
            MM_LOG_W() << "folder already a scan root" << handle;
            emit errorRaised(tr("%1 is already a scan folder.").arg(folder.displayName));
            return;
        }
        if (candidate.startsWith(other, Qt::CaseInsensitive)) {
            MM_LOG_W() << "folder is inside an existing root" << handle
                       << "under" << folder.handle;
            emit errorRaised(tr("That folder is already covered by %1.")
                                 .arg(folder.displayName));
            return;
        }
        if (other.startsWith(candidate, Qt::CaseInsensitive)) {
            MM_LOG_W() << "folder would contain an existing root" << handle
                       << "over" << folder.handle;
            emit errorRaised(tr("That folder contains %1, which is already a scan folder. Remove it first.")
                                 .arg(folder.displayName));
            return;
        }
    }

    const qint64 id = m_folderRepository.add(handle, displayName);
    if (id < 0) {
        emit errorRaised(tr("That folder could not be saved."));
        return;
    }

    reloadFolders();
    emit folderAdded(displayName);
    rescan(id);
}

void LibraryController::removeFolder(qint64 folderId)
{
    const ScanFolder folder = m_folderRepository.byId(folderId);
    if (!folder.isValid()) {
        return;
    }

    if (m_scanning && m_scanningRoot == folder.handle) {
        cancelScan();
    }

    if (!m_folderRepository.remove(folderId)) {
        emit errorRaised(tr("That folder could not be removed."));
        return;
    }

    if (m_currentFolderId == folderId) {
        showAllFiles();
    }

    reloadFolders();
    reloadCurrentView();
    reloadMedia();
    recountFiles();
    if (m_showEpisodes->mediaId() > 0) {
        m_showEpisodes->reload();
    }
}

bool LibraryController::scanQuiet() const
{
    return m_quietScan;
}

void LibraryController::rescan(qint64 folderId)
{
    if (refusesWhileHeld("a rescan")) {
        return;
    }
    endQuietScan(false);
    beginScan(folderId);
}

bool LibraryController::heldStill() const
{
    return m_heldStill;
}

void LibraryController::setHeldStill(bool held)
{
    if (m_heldStill == held) {
        return;
    }
    m_heldStill = held;

    if (held) {
        m_scanInterruptedByHold = m_scanning || !m_scanQueue.isEmpty();
        if (m_scanInterruptedByHold) {
            MM_LOG_I() << "the library holds still: stopping the scan under way,"
                       << "it runs again when streaming stops";
            cancelScan();
        } else {
            MM_LOG_I() << "the library holds still: no scan starts until streaming stops";
        }
        return;
    }

    MM_LOG_I() << "the library is free to change again";
    if (m_scanInterruptedByHold) {
        m_scanInterruptedByHold = false;
        rescanAllQuiet();
    }
}

bool LibraryController::isRemote() const
{
    return m_remote;
}

void LibraryController::setRemote(bool remote)
{
    if (m_remote == remote) {
        return;
    }
    m_remote = remote;
    MM_LOG_I() << "the library is now" << (remote ? "the PC's" : "this device's own");
    emit remoteChanged();
}

void LibraryController::reloadAfterSwitch()
{
    QElapsedTimer timer;
    timer.start();

    m_scanQueue.clear();
    m_scanIndex.clear();
    m_scanIndexLoaded = false;
    m_scanSeen.clear();
    m_unmatchedHandles.clear();
    m_unmatchedShowsStale = false;

    m_collectionPageModel->setCollectionId(0);
    m_showEpisodes->setMediaId(0);
    clearSearch();
    m_removedFiles->setFiles(m_fileRepository.removedFiles());

    showAllFiles();
    recountFiles();
    reloadFolders();
    reloadCurrentView();
    reloadMedia();

    MM_LOG_I() << "library switched: every list read again in" << timer.elapsed() << "ms,"
               << m_fileCount << "files," << m_movieModel->rowCount() << "films,"
               << m_showModel->rowCount() << "shows";
}

bool LibraryController::refusesWhileHeld(const char *what) const
{
    if (!m_heldStill) {
        return false;
    }
    MM_LOG_I() << "the library holds still while streaming, so" << what << "waits";
    return true;
}

void LibraryController::endQuietScan(bool notify)
{
    if (!m_quietScan) {
        return;
    }

    m_quietScan = false;

    if (notify) {
        const int added = qMax(0, m_fileCount - m_quietScanBaseline);
        MM_LOG_I() << "silent rescan finished, new files" << added;
        emit quietScanFinished(added);
    } else {
        MM_LOG_I() << "silent rescan interrupted by a visible scan";
    }

    emit scanningChanged();
}

void LibraryController::rescanAllQuiet()
{
    if (refusesWhileHeld("the quiet rescan")) {
        return;
    }
    if (m_scanning || m_quietScan || !m_scanQueue.isEmpty()) {
        return;
    }

    QList<qint64> ids;
    const QList<ScanFolder> all = m_folderRepository.all();
    for (const ScanFolder &folder : all) {
        if (folder.lastScanned.isValid()) {
            ids.append(folder.id);
        }
    }

    if (ids.isEmpty()) {
        return;
    }

    m_quietScan = true;
    m_quietScanBaseline = m_fileCount;
    MM_LOG_I() << "silent startup rescan of" << ids.size()
               << "folders, starting from" << m_quietScanBaseline << "files";
    emit scanningChanged();

    for (qint64 id : ids) {
        beginScan(id);
    }
}

void LibraryController::rescanStorageQuiet(const QString &volumeName)
{
    if (refusesWhileHeld("the storage rescan")) {
        return;
    }
#ifdef Q_OS_ANDROID
    const QString primary = AndroidMediaSource::primaryRoot();
    const bool withPrimary = volumeName.isEmpty()
        || volumeName.compare(QStringLiteral("external_primary"), Qt::CaseInsensitive) == 0;

    QList<qint64> ids;
    const QList<ScanFolder> all = m_folderRepository.all();
    for (const ScanFolder &folder : all) {
        if (!primary.isEmpty() && folder.handle == primary) {
            if (withPrimary && folder.lastScanned.isValid()) {
                ids.append(folder.id);
            }
            continue;
        }
        if (!AndroidMediaSource::isMediaStoreRoot(folder.handle)) {
            continue;
        }
        if (!volumeName.isEmpty()
            && AndroidMediaSource::mediaStoreVolume(folder.handle)
                   .compare(volumeName, Qt::CaseInsensitive) != 0) {
            continue;
        }
        ids.append(folder.id);
    }

    if (ids.isEmpty()) {
        MM_LOG_D() << "no scanned storage matches"
                   << (volumeName.isEmpty() ? QStringLiteral("any volume") : volumeName)
                   << "- nothing to rescan";
        return;
    }

    if (!m_scanning && !m_quietScan) {
        m_quietScan = true;
        m_quietScanBaseline = m_fileCount;
        emit scanningChanged();
    }

    MM_LOG_I() << "quietly rescanning" << ids.size() << "storage roots"
               << (withPrimary ? "with internal storage" : "without internal storage") << "for"
               << (volumeName.isEmpty() ? QStringLiteral("any volume") : volumeName)
               << (m_scanning ? "after the scan under way" : "now");
    for (qint64 id : ids) {
        beginScan(id);
    }
#else
    Q_UNUSED(volumeName)
#endif
}

void LibraryController::beginScan(qint64 folderId)
{
    if (refusesWhileHeld("a scan")) {
        return;
    }

    const ScanFolder folder = m_folderRepository.byId(folderId);
    if (!folder.isValid()) {
        MM_LOG_W() << "rescan requested for unknown folder" << folderId;
        return;
    }

    if (m_scanning) {
        MM_LOG_I() << "queueing rescan of" << folder.handle;
        if (!m_scanQueue.contains(folder.handle)) {
            m_scanQueue.enqueue(folder.handle);
        }
        return;
    }

    IScanner *scanner = m_platform.scanner();
    if (!scanner) {
        emit errorRaised(tr("Scanning is not available on this platform yet."));
        return;
    }

    m_scanning = true;
    m_scanningRoot = folder.handle;
    m_scanningName = folder.displayName;
    m_scanFolderId = folder.id;
    m_scanIndex.clear();
    m_scanIndexLoaded = false;
    m_scanSeen.clear();
    emit scanningChanged();

    scanner->scan(folder.handle);
}

void LibraryController::rescanAll()
{
    if (refusesWhileHeld("a rescan")) {
        return;
    }

    const QList<ScanFolder> all = m_folderRepository.all();
    if (all.isEmpty()) {
        return;
    }

    endQuietScan(false);

    MM_LOG_I() << "rescanning all" << all.size() << "folders";
    for (const ScanFolder &folder : all) {
        beginScan(folder.id);
    }
}

void LibraryController::cancelScan()
{
    IScanner *scanner = m_platform.scanner();
    if (scanner) {
        scanner->cancel();
    }
    m_scanQueue.clear();
}

void LibraryController::onBatchReady(const QString &rootHandle,
                                     const QList<MediaFileInfo> &batch)
{
    if (!m_scanning || rootHandle != m_scanningRoot || m_scanFolderId < 0
        || batch.isEmpty()) {
        return;
    }

    if (!m_scanIndexLoaded) {
        m_scanIndex = m_fileRepository.scanIndex(m_scanFolderId);
        m_scanIndexLoaded = true;
    }

    for (const MediaFileInfo &info : batch) {
        m_scanSeen.insert(info.handle);
    }

    const BatchWrite written =
        m_fileRepository.upsertBatch(m_scanFolderId, batch, m_scanIndex);
    if (written.written() == 0) {
        return;
    }

    const QStringList handles = written.handles();
    m_scanWritten += handles;

    if (!written.inserted.isEmpty()) {
        emit newFilesWritten(int(written.inserted.size()));
    }

    if (m_scanRowsHeld) {
        m_heldRows += handles;
        m_heldFolderIds.insert(m_scanFolderId);
    } else {
        mergeScannedFiles(m_fileRepository.byHandles(handles));
    }

    const int arrived = int(written.inserted.size() + written.revived.size());
    if (arrived > 0) {
        m_fileCount += arrived;
        m_unmatchedCount += int(written.inserted.size());
        emit fileCountChanged();
    }
}

void LibraryController::setScanRowsHeld(bool held)
{
    if (m_scanRowsHeld == held) {
        return;
    }
    m_scanRowsHeld = held;

    if (held) {
        MM_LOG_I() << "scanned rows are held back from the lists until the library is built";
        return;
    }

    QStringList handles;
    QSet<QString> seen;
    handles.reserve(m_heldRows.size());
    for (const QString &handle : std::as_const(m_heldRows)) {
        if (!seen.contains(handle)) {
            seen.insert(handle);
            handles.append(handle);
        }
    }
    const QSet<qint64> folderIds = m_heldFolderIds;
    m_heldRows.clear();
    m_heldFolderIds.clear();

    if (handles.isEmpty()) {
        return;
    }

    QElapsedTimer clock;
    clock.start();
    const QList<LibraryFile> files = m_fileRepository.byHandles(handles);
    mergeScannedFiles(files);
    if (m_view == View::Folder && folderIds.contains(m_currentFolderId)) {
        m_fileModel->setFiles(m_fileRepository.inFolder(m_currentFolderId));
    }
    MM_LOG_I() << "added" << files.size() << "held scanned rows to the lists in"
               << clock.elapsed() << "ms";
}

void LibraryController::mergeScannedFiles(const QList<LibraryFile> &files)
{
    if (files.isEmpty()) {
        return;
    }

    for (const LibraryFile &file : files) {
        m_continueModel->updateFile(file);
        m_searchModel->updateFile(file);
    }

    switch (m_view) {
    case View::All: {
        QSet<QString> touched;
        touched.reserve(files.size());
        for (const LibraryFile &file : files) {
            touched.insert(file.handle);
        }

        const int sort = m_sortMode;
        const auto isBefore = [sort](const LibraryFile &a, const LibraryFile &b) {
            return BrowseOptions::isBefore(sort, a, b);
        };

        QList<LibraryFile> kept;
        kept.reserve(m_fileModel->rowCount());
        for (int row = 0; row < m_fileModel->rowCount(); ++row) {
            LibraryFile file = m_fileModel->at(row);
            if (!touched.contains(file.handle)) {
                kept.append(std::move(file));
            }
        }

        QList<LibraryFile> incoming;
        incoming.reserve(files.size());
        for (const LibraryFile &file : files) {
            if (!file.missing && BrowseOptions::keeps(m_filterMode, file)) {
                incoming.append(file);
            }
        }
        std::stable_sort(incoming.begin(), incoming.end(), isBefore);

        QList<LibraryFile> merged;
        merged.reserve(kept.size() + incoming.size());
        std::merge(kept.cbegin(), kept.cend(), incoming.cbegin(), incoming.cend(),
                   std::back_inserter(merged), isBefore);
        m_fileModel->setFiles(merged);
        break;
    }
    case View::Folder:
        if (m_currentFolderId == m_scanFolderId) {
            m_fileModel->setFiles(m_fileRepository.inFolder(m_currentFolderId));
        } else {
            for (const LibraryFile &file : files) {
                m_fileModel->updateFile(file);
            }
        }
        break;
    case View::ContinueWatching:
    default:
        for (const LibraryFile &file : files) {
            m_fileModel->updateFile(file);
        }
        break;
    }
}

void LibraryController::onScanProgress(const QString &rootHandle, int filesSeen)
{
    Q_UNUSED(rootHandle)
    if (m_scanFilesSeen == filesSeen) {
        return;
    }
    m_scanFilesSeen = filesSeen;
    emit scanProgressChanged();
}

void LibraryController::onScanFinished(const QString &rootHandle, int filesFound)
{
    Q_UNUSED(filesFound)

    const ScanFolder folder = m_folderRepository.byHandle(rootHandle);
    if (folder.isValid()) {
        if (!m_scanIndexLoaded) {
            m_scanIndex = m_fileRepository.scanIndex(folder.id);
            m_scanIndexLoaded = true;
        }

        const QStringList missing =
            m_fileRepository.markMissingOutside(folder.id, m_scanSeen, m_scanIndex);
        m_folderRepository.markScanned(folder.id, QDateTime::currentDateTimeUtc());
        if (!folder.available) {
            m_folderRepository.setAvailable(folder.id, true);
        }
        m_reportedUnavailable.removeAll(rootHandle);
        if (!m_scanFolderIds.contains(folder.id)) {
            m_scanFolderIds.append(folder.id);
        }

        if (!missing.isEmpty()) {
            m_scanMissing += missing;
            mergeScannedFiles(m_fileRepository.byHandles(missing));
            m_continueModel->setFiles(m_fileRepository.continueWatching(kHomeRowLength));
            refreshRecentTitles();
            refreshGenreRows();
            refreshGrids();
            refreshCollections();
        }
    } else {
        MM_LOG_W() << "scan finished for a folder no longer in the database"
                   << rootHandle;
    }

    finishScanState();
    emit scanProgressChanged();

    reloadFolders();
    m_unmatchedShowsStale = true;
    if (m_showEpisodes->mediaId() > 0) {
        m_showEpisodes->reload();
    }

    startNextQueuedScan();
    if (m_scanning) {
        return;
    }

    emit scanningChanged();
    finishScanQueue();
}

void LibraryController::onScanCancelled(const QString &rootHandle)
{
    MM_LOG_I() << "scan cancelled for" << rootHandle << "after"
               << m_scanSeen.size() << "files, which are kept";

    finishScanState();
    emit scanProgressChanged();
    m_unmatchedShowsStale = true;

    startNextQueuedScan();
    if (m_scanning) {
        return;
    }

    emit scanningChanged();
    finishScanQueue();
}

void LibraryController::finishScanState()
{
    m_scanning = false;
    m_scanningRoot.clear();
    m_scanningName.clear();
    m_scanFilesSeen = 0;
    m_scanFolderId = -1;
    m_scanIndex.clear();
    m_scanIndexLoaded = false;
    m_scanSeen.clear();
}

void LibraryController::settleMovedFiles()
{
    const QStringList missing = m_scanMissing;
    m_scanMissing.clear();

    const QList<QPair<QString, QString>> settled =
        m_fileRepository.settleMoves(missing, m_scanWritten);
    if (settled.isEmpty()) {
        return;
    }

    QStringList touched;
    for (const auto &move : settled) {
        touched.append(move.second);
        m_scanWritten.removeAll(move.first);
    }

    MM_LOG_I() << "settled" << settled.size()
               << "files that moved rather than vanished";

    mergeScannedFiles(m_fileRepository.byHandles(touched));
    m_continueModel->setFiles(m_fileRepository.continueWatching(kHomeRowLength));
    refreshRecentTitles();
    refreshGenreRows();
    refreshGrids();
    refreshCollections();
    m_unmatchedShowsStale = true;
}

void LibraryController::finishScanQueue()
{
    settleMovedFiles();

    recountFiles();
    endQuietScan(true);

    const QStringList written = m_scanWritten;
    const QList<qint64> folderIds = m_scanFolderIds;
    m_scanWritten.clear();
    m_scanFolderIds.clear();

    MM_LOG_I() << "scanning done:" << written.size() << "files written across"
               << folderIds.size() << "folders";

    probeFiles(written, folderIds);
    emit scanQueueFinished(written);
}

void LibraryController::probeFiles(const QStringList &handles,
                                   const QList<qint64> &folderIds)
{
    if (!m_probes || !m_probes->isEnabled()) {
        return;
    }

    const QStringList failed = m_fileRepository.failedProbeHandles(folderIds);

    QSet<QString> added;
    QList<ProbeRequest> requests;
    requests.reserve(handles.size() + failed.size());
    for (const QStringList &list : {handles, failed}) {
        for (const QString &handle : list) {
            if (added.contains(handle)) {
                continue;
            }
            added.insert(handle);
            requests.append({handle, handle});
        }
    }

    if (requests.isEmpty()) {
        return;
    }

    MM_LOG_I() << "probing" << handles.size() << "scanned files and retrying"
               << failed.size() << "that failed before";
    m_probes->requestAll(requests);
}

void LibraryController::onRootUnavailable(const QString &rootHandle)
{
    const ScanFolder folder = m_folderRepository.byHandle(rootHandle);
    if (folder.isValid()) {
        if (m_quietScan) {
            constexpr int retryMs = 5000;
            MM_LOG_I() << "root unavailable during a quiet rescan, checking again in"
                       << retryMs << "ms:" << rootHandle;
            QTimer::singleShot(retryMs, this, [this, rootHandle]() {
                confirmRootUnavailable(rootHandle);
            });
        } else {
            m_folderRepository.setAvailable(folder.id, false);
            emit folderUnavailable(folder.displayName);
        }
    }

    finishScanState();
    emit scanProgressChanged();
    m_unmatchedShowsStale = true;

    reloadFolders();
    startNextQueuedScan();
    if (m_scanning) {
        return;
    }

    emit scanningChanged();
    finishScanQueue();
}

void LibraryController::confirmRootUnavailable(const QString &rootHandle)
{
    const ScanFolder folder = m_folderRepository.byHandle(rootHandle);
    IMediaSource *source = m_platform.mediaSource();
    if (!folder.isValid() || !source) {
        return;
    }

    if (source->isRootAvailable(rootHandle)) {
        MM_LOG_I() << "root is back, not reporting it offline:" << rootHandle;
        return;
    }

    MM_LOG_W() << "root still unavailable after a retry:" << rootHandle;
    m_folderRepository.setAvailable(folder.id, false);
    reloadFolders();

    if (m_reportedUnavailable.contains(rootHandle)) {
        return;
    }
    m_reportedUnavailable.append(rootHandle);
    emit folderUnavailable(folder.displayName);
}

void LibraryController::startNextQueuedScan()
{
    if (m_scanQueue.isEmpty()) {
        return;
    }

    const QString next = m_scanQueue.dequeue();
    const ScanFolder folder = m_folderRepository.byHandle(next);
    if (folder.isValid()) {
        beginScan(folder.id);
    } else {
        startNextQueuedScan();
    }
}

void LibraryController::showFolder(qint64 folderId)
{
    const ScanFolder folder = m_folderRepository.byId(folderId);
    if (!folder.isValid()) {
        return;
    }

    m_view = View::Folder;
    m_currentFolderId = folderId;
    m_currentParentHandle = folder.handle;
    emit viewChanged();
    reloadCurrentView();
}

void LibraryController::showAllFiles()
{
    if (m_view == View::All) {
        MM_LOG_D() << "already showing every file, the list is kept as it is";
        return;
    }

    m_view = View::All;
    m_currentFolderId = -1;
    m_currentParentHandle.clear();
    emit viewChanged();
    reloadCurrentView();
}

void LibraryController::recountFiles()
{
    m_playbackRepository.adoptFromMissing(
        m_playbackRepository.mediaWithMissingHistory());

    m_fileCount = m_fileRepository.count();
    m_unmatchedCount = m_fileRepository.unmatchedCount();
    m_continueCount = m_fileRepository.continueWatchingCount();
    MM_LOG_D() << "library counted:" << m_fileCount << "files,"
               << m_unmatchedCount << "unmatched," << m_continueCount
               << "to continue";
    emit fileCountChanged();
}

void LibraryController::placeFiles(const QList<LibraryFile> &files)
{
    const int sort = m_sortMode;
    const FileListModel::Order isBefore =
        [sort](const LibraryFile &a, const LibraryFile &b) {
            return BrowseOptions::isBefore(sort, a, b);
        };

    for (const LibraryFile &file : files) {
        m_continueModel->updateFile(file);
        m_searchModel->updateFile(file);

        if (m_view == View::All) {
            const bool keep = !file.missing
                && BrowseOptions::keeps(m_filterMode, file);
            m_fileModel->placeFile(file, keep, isBefore);
        } else {
            m_fileModel->updateFile(file);
        }
    }
}

void LibraryController::applyArtworkLanded(const QStringList &paths, bool everything)
{
    const QList<FileListModel *> files = {
        m_fileModel, m_continueModel, m_searchModel, m_searchEpisodeModel
    };
    for (FileListModel *model : files) {
        model->touchArtwork(paths, everything);
    }

    m_searchMovieModel->touchArtwork(paths, everything);
    m_searchShowModel->touchArtwork(paths, everything);
    m_searchCollectionModel->touchArtwork(paths, everything);
    m_movieModel->touchArtwork(paths, everything);
    m_showModel->touchArtwork(paths, everything);
    m_suggestionModel->touchArtwork(paths, everything);
    m_recentTitleModel->touchArtwork(paths, everything);
    m_genreRowModel->touchArtwork(paths, everything);
    m_movieGridModel->touchArtwork(paths, everything);
    m_showGridModel->touchArtwork(paths, everything);
    m_collectionModel->touchArtwork(paths, everything);
    m_collectionGridModel->touchArtwork(paths, everything);
    m_collectionPageModel->touchArtwork(paths, everything);
    m_showEpisodes->touchArtwork(paths, everything);
}

void LibraryController::placeTitles(const QList<qint64> &mediaIds)
{
    if (mediaIds.isEmpty()) {
        return;
    }

    const QList<MediaRecord> titles = m_mediaRepository.listedByIds(mediaIds);
    QHash<qint64, MediaRecord> titlesById;
    titlesById.reserve(titles.size());
    for (const MediaRecord &title : titles) {
        titlesById.insert(title.id, title);
    }

    for (const qint64 mediaId : mediaIds) {
        MediaRecord title = titlesById.value(mediaId);
        const bool listed = title.isValid() && title.fileCount > 0;
        title.id = mediaId;
        m_movieModel->placeItem(title, listed && title.kind == QLatin1String("movie"));
        m_showModel->placeItem(title, listed && title.kind == QLatin1String("tv"));
        m_suggestionModel->placeItem(title, title.suggestedFileCount > 0);
    }

    refreshRecentTitles();
    refreshGenreRows();
    refreshGrids();
    refreshCollections();
}

void LibraryController::refreshPlayedFiles(const QList<qint64> &fileIds)
{
    if (fileIds.isEmpty()) {
        return;
    }

    placeFiles(m_fileRepository.byIds(fileIds));

    m_continueModel->setFiles(m_fileRepository.continueWatching(kHomeRowLength));
    if (m_view == View::ContinueWatching) {
        m_fileModel->setFiles(m_fileRepository.continueWatching(50));
    }

    QList<qint64> mediaIds;
    for (const qint64 fileId : fileIds) {
        const FileMediaLink link = m_mediaRepository.linkForFile(fileId);
        if (link.isValid() && !mediaIds.contains(link.mediaId)) {
            mediaIds.append(link.mediaId);
        }
    }
    placeTitles(mediaIds);

    if (m_showEpisodes->concerns({}, fileIds)) {
        m_showEpisodes->reload();
    }
}

void LibraryController::applyLibraryChange(const LibraryChange &change)
{
    if (change.isEmpty()) {
        return;
    }

    QElapsedTimer timer;
    timer.start();

    if (!m_playbackRepository.adoptFromMissing(change.mediaIds).isEmpty()) {
        m_continueCount = m_fileRepository.continueWatchingCount();
        emit fileCountChanged();
    }

    const QList<LibraryFile> files = m_fileRepository.byIds(change.fileIds);
    placeFiles(files);

    QStringList nowMatched;
    for (const LibraryFile &file : files) {
        if (file.isMatched()) {
            nowMatched.append(file.handle);
        }
        if (m_thumbnails && !file.matchedPosterPath.isEmpty()) {
            m_thumbnails->discard(file.handle);
        }
    }

    placeTitles(change.mediaIds);

    if (m_showEpisodes->concerns(change.mediaIds, change.fileIds)) {
        m_showEpisodes->reload();
    }

    const qint64 patched = timer.elapsed();

    if (change.filesMatched > 0 || change.filesUnmatched > 0) {
        m_unmatchedCount = qMax(0, m_unmatchedCount + change.filesUnmatched
                                       - change.filesMatched);

        if (change.filesUnmatched > 0) {
            reloadUnmatchedShows();
        } else {
            m_unmatchedShows->removeFiles(nowMatched);

            const QSet<QString> matched(nowMatched.cbegin(), nowMatched.cend());
            m_unmatchedHandles.removeIf([&matched](const QString &handle) {
                return matched.contains(handle);
            });
        }

        emit fileCountChanged();
    }

    MM_LOG_D() << "library change applied:" << files.size() << "files,"
               << change.mediaIds.size() << "titles in" << patched << "ms,"
               << (timer.elapsed() - patched) << "ms on unmatched shows and counts";
}

void LibraryController::reloadFolders()
{
    m_folderModel->setFolders(m_folderRepository.all());
}

void LibraryController::discardThumbnailsWithPosters()
{
    if (!m_thumbnails) {
        return;
    }

    int removed = 0;
    const QStringList handles = m_fileRepository.handlesWithPoster();
    for (const QString &handle : handles) {
        if (m_thumbnails->discard(handle)) {
            ++removed;
        }
    }

    if (removed > 0) {
        MM_LOG_I() << "deleted" << removed
                   << "generated thumbnails made redundant by poster art";
    }
}

void LibraryController::reloadCurrentView()
{
    QElapsedTimer timer;
    timer.start();

    const QList<LibraryFile> continueWatching =
        m_fileRepository.continueWatching(kHomeRowLength);

    QList<LibraryFile> current;
    switch (m_view) {
    case View::Folder:
        current = m_fileRepository.inFolder(m_currentFolderId);
        break;
    case View::ContinueWatching:
        current = m_fileRepository.continueWatching(50);
        break;
    case View::All:
    default:
        current = applyOptions(m_fileRepository.all());
        break;
    }

    const qint64 queried = timer.elapsed();

    m_continueModel->setFiles(continueWatching);
    m_fileModel->setFiles(current);

    MM_LOG_D() << "file views reloaded -" << queried << "ms in the database,"
               << (timer.elapsed() - queried) << "ms into the models";
}

QString LibraryController::handleFromUrl(const QString &url)
{
    return MediaHandle::fromUrl(url);
}

bool LibraryController::isPlayableFile(const QString &pathOrUrl)
{
    return MediaFormats::looksLikeVideoFile(handleFromUrl(pathOrUrl));
}

QString LibraryController::takeLaunchPath()
{
    const QString path = m_launchPath;
    m_launchPath.clear();
    if (!path.isEmpty()) {
        MM_LOG_I() << "opening file passed on the command line" << path;
    }
    return path;
}

void LibraryController::setLaunchPath(const QString &path)
{
    m_launchPath = path;
}

QString LibraryController::mpvUrlFor(const QString &handle)
{
    IMediaSource *source = m_platform.mediaSource();
    if (!source) {
        MM_LOG_E() << "no media source, cannot resolve" << handle;
        return QString();
    }

    if (!source->exists(handle)) {
        const LibraryFile known = m_fileRepository.byHandle(handle);
        const QString name = known.isValid() ? known.displayName : handle;
        MM_LOG_W() << "file is no longer present" << handle;
        emit fileMissing(handle, name);
        return QString();
    }

    const QString resolved = source->mpvUrl(handle);
    if (resolved.isEmpty()) {
        const LibraryFile known = m_fileRepository.byHandle(handle);
        MM_LOG_W() << "file exists but could not be opened" << handle;
        emit fileMissing(handle, known.isValid() ? known.displayName : handle);
        return QString();
    }
    MM_LOG_D() << "media source resolved path for playback"
               << StreamProtocol::withoutToken(resolved);
    return resolved;
}

QVariantList LibraryController::filmCopies(const QString &handle) const
{
    QVariantList copies;
    if (handle.isEmpty()) {
        return copies;
    }

    const QList<LibraryFile> files = m_fileRepository.copiesOfFilm(handle);
    if (files.size() < 2) {
        return copies;
    }

    IMediaSource *source = m_platform.mediaSource();
    for (const LibraryFile &file : files) {
        QVariantMap copy;
        copy.insert(QStringLiteral("handle"), file.handle);
        copy.insert(QStringLiteral("displayName"), file.displayName);
        copy.insert(QStringLiteral("folderPath"),
                    (source && !file.parentHandle.isEmpty())
                        ? source->displayPath(file.parentHandle)
                        : file.parentHandle);
        copy.insert(QStringLiteral("width"), file.width);
        copy.insert(QStringLiteral("height"), file.height);
        copy.insert(QStringLiteral("hdr"), file.hdr);
        copy.insert(QStringLiteral("videoCodec"), file.videoCodec);
        copy.insert(QStringLiteral("sizeText"),
                    file.sizeBytes > 0
                        ? QLocale().formattedDataSize(file.sizeBytes, 1,
                                                      QLocale::DataSizeIecFormat)
                        : QString());
        copy.insert(QStringLiteral("positionSeconds"), file.playback.positionSeconds);
        copy.insert(QStringLiteral("watched"), file.playback.watched);
        copies.append(copy);
    }

    MM_LOG_I() << "a film has" << files.size() << "copies to choose from:"
               << files.first().matchedTitle;
    return copies;
}

QVariantMap LibraryController::fileInfo(const QString &handle)
{
    QVariantMap info;

    const LibraryFile file = m_fileRepository.byHandle(handle);
    IMediaSource *source = m_platform.mediaSource();

    info.insert(QStringLiteral("handle"), handle);
    info.insert(QStringLiteral("displayName"),
                file.isValid() ? file.displayName : handle);
    info.insert(QStringLiteral("path"),
                source ? source->displayPath(handle) : handle);

    const QString parent = file.isValid() ? file.parentHandle : QString();
    info.insert(QStringLiteral("folderPath"),
                (source && !parent.isEmpty()) ? source->displayPath(parent)
                                              : parent);
    info.insert(QStringLiteral("indexed"), file.isValid());
    info.insert(QStringLiteral("present"), source ? source->exists(handle) : false);

    if (!file.isValid()) {
        MM_LOG_W() << "file info requested for an unindexed handle" << handle;
        return info;
    }

    info.insert(QStringLiteral("fileId"), file.id);
    info.insert(QStringLiteral("sizeBytes"), file.sizeBytes);
    info.insert(QStringLiteral("sizeText"),
                file.sizeBytes > 0
                    ? QLocale().formattedDataSize(file.sizeBytes, 1,
                                                  QLocale::DataSizeIecFormat)
                    : QString());
    info.insert(QStringLiteral("container"), file.container);
    info.insert(QStringLiteral("videoCodec"), file.videoCodec);
    info.insert(QStringLiteral("audioCodec"), file.audioCodec);
    info.insert(QStringLiteral("resolution"),
                (file.width > 0 && file.height > 0)
                    ? QStringLiteral("%1x%2").arg(file.width).arg(file.height)
                    : QString());
    info.insert(QStringLiteral("matched"), file.isMatched());
    info.insert(QStringLiteral("suggested"), file.matchSuggested);
    info.insert(QStringLiteral("matchedTitle"), file.matchedTitle);
    info.insert(QStringLiteral("isEpisode"), file.isEpisode());

    const FileMediaLink link = m_mediaRepository.linkForFile(file.id);
    info.insert(QStringLiteral("mediaId"), link.mediaId);
    info.insert(QStringLiteral("width"), file.width);
    info.insert(QStringLiteral("height"), file.height);
    info.insert(QStringLiteral("hdr"), file.hdr);
    info.insert(QStringLiteral("audioTrackCount"), file.audioTrackCount);
    info.insert(QStringLiteral("subtitleTrackCount"), file.subtitleTrackCount);
    info.insert(QStringLiteral("watched"), file.playback.watched);
    info.insert(QStringLiteral("progress"), file.playback.progress());
    info.insert(QStringLiteral("hasPlaybackState"), file.playback.isValid());
    info.insert(QStringLiteral("continueWatching"), continuesWatching(file));
    info.insert(QStringLiteral("modified"),
                file.modified.isValid()
                    ? QLocale().toString(file.modified.toLocalTime(),
                                         QLocale::ShortFormat)
                    : QString());

    const double duration = file.playback.durationSeconds > 0.0
        ? file.playback.durationSeconds
        : file.durationSeconds;
    if (duration > 0.0) {
        const int total = static_cast<int>(duration);
        info.insert(QStringLiteral("durationText"),
                    QStringLiteral("%1:%2:%3")
                        .arg(total / 3600)
                        .arg((total % 3600) / 60, 2, 10, QLatin1Char('0'))
                        .arg(total % 60, 2, 10, QLatin1Char('0')));
    } else {
        info.insert(QStringLiteral("durationText"), QString());
    }

    if (duration > 0.0 && file.sizeBytes > 0) {
        const double megabitsPerSecond =
            (double(file.sizeBytes) * 8.0) / duration / 1000000.0;
        info.insert(QStringLiteral("bitrateText"),
                    QStringLiteral("%1 Mb/s")
                        .arg(QLocale().toString(megabitsPerSecond, 'f',
                                                megabitsPerSecond < 10 ? 1 : 0)));
    } else {
        info.insert(QStringLiteral("bitrateText"), QString());
    }

    info.insert(QStringLiteral("durationSeconds"), duration);
    info.insert(QStringLiteral("positionSeconds"), file.playback.positionSeconds);
    info.insert(QStringLiteral("resumeSeconds"),
                PlaybackStateRepository::resumeSeconds(
                    file.playback.positionSeconds,
                    file.playback.durationSeconds > 0.0 ? file.playback.durationSeconds
                                                        : duration,
                    file.playback.watched));
    info.insert(QStringLiteral("finished"), BrowseOptions::isFinished(file));

    return info;
}

void LibraryController::recordPlayback(const QString &handle,
                                       double positionSeconds,
                                       double durationSeconds,
                                       bool refreshViews)
{
    const LibraryFile file = m_fileRepository.byHandle(handle);
    if (!file.isValid()) {
        return;
    }

    PlaybackState state;
    state.fileId = file.id;
    state.positionSeconds = positionSeconds;
    state.durationSeconds = durationSeconds;
    state.watchedSeconds = positionSeconds;
    state.lastPlayed = QDateTime::currentDateTimeUtc();

    const bool wasContinuing = continuesWatching(file);
    if (!m_playbackRepository.save(state)) {
        return;
    }

    const bool continuing = !file.missing
        && PlaybackStateRepository::resumable(positionSeconds, durationSeconds,
                                              file.playback.watched);
    m_continueCount += int(continuing) - int(wasContinuing);

    MM_LOG_D() << "playback saved for file" << file.id << "at" << positionSeconds
               << (refreshViews ? "and its rows refreshed" : "without refreshing views");

    if (refreshViews) {
        refreshPlayedFiles({file.id});
        emit fileCountChanged();
    } else if (continuing != wasContinuing) {
        emit fileCountChanged();
    }
}

double LibraryController::resumePositionFor(const QString &handle)
{
    const LibraryFile file = m_fileRepository.byHandle(handle);
    if (!file.isValid()) {
        return 0.0;
    }

    const PlaybackState state = m_playbackRepository.forFile(file.id);
    if (!state.isValid()) {
        return 0.0;
    }

    const double duration = state.durationSeconds > 0.0 ? state.durationSeconds
                                                        : file.durationSeconds;
    return PlaybackStateRepository::resumeSeconds(state.positionSeconds, duration,
                                                  state.watched);
}

void LibraryController::recordProbe(const QString &handle,
                                    double durationSeconds,
                                    const QString &container,
                                    const QString &videoCodec,
                                    const QString &audioCodec,
                                    int width,
                                    int height,
                                    bool hdr,
                                    int audioTrackCount,
                                    int subtitleTrackCount)
{
    if (durationSeconds <= 0.0 || m_remote) {
        return;
    }

    const LibraryFile file = m_fileRepository.probeFieldsFor(handle);
    if (!file.isValid()) {
        return;
    }

    const bool unchanged = qFuzzyCompare(file.durationSeconds + 1.0,
                                         durationSeconds + 1.0)
        && file.container == container
        && file.videoCodec == videoCodec
        && file.audioCodec == audioCodec
        && file.width == width
        && file.height == height
        && file.hdr == hdr
        && file.audioTrackCount == audioTrackCount
        && file.subtitleTrackCount == subtitleTrackCount;
    if (unchanged) {
        return;
    }

    if (!m_fileRepository.updateProbeInfo(file.id, durationSeconds, container,
                                          videoCodec, audioCodec, width, height,
                                          hdr, audioTrackCount,
                                          subtitleTrackCount)) {
        return;
    }

    MM_LOG_D() << "probe stored for" << handle
               << durationSeconds << "s," << container
               << videoCodec << audioCodec << width << "x" << height
               << (hdr ? "hdr" : "sdr") << audioTrackCount << "audio,"
               << subtitleTrackCount << "subtitle";

    const QList<FileListModel *> models = {
        m_fileModel, m_continueModel, m_searchModel, m_searchEpisodeModel
    };
    for (FileListModel *model : models) {
        LibraryFile row = model->fileWithHandle(handle);
        if (!row.isValid()) {
            continue;
        }
        row.durationSeconds = durationSeconds;
        row.container = container;
        row.videoCodec = videoCodec;
        row.audioCodec = audioCodec;
        row.width = width;
        row.height = height;
        row.hdr = hdr;
        row.audioTrackCount = audioTrackCount;
        row.subtitleTrackCount = subtitleTrackCount;
        model->updateFile(row);
    }
}

QString LibraryController::attachSubtitle(const QString &fileHandle,
                                          const QString &subtitleUrl,
                                          const QString &origin)
{
    if (fileHandle.isEmpty() || subtitleUrl.isEmpty()) {
        return QString();
    }

    const QString subHandle = handleFromUrl(subtitleUrl);

    if (m_remote) {
        IMediaSource *local = m_platform.localMediaSource();
        const QString rawName = local ? local->displayPath(subHandle) : subHandle;
        MM_LOG_I() << "a subtitle for a film from the PC is played, not kept:" << rawName;
        return SubtitleNaming::baseName(rawName);
    }

    MM_LOG_I() << "attaching subtitle" << subtitleUrl << "as" << subHandle
               << "to" << fileHandle;

#ifdef Q_OS_ANDROID
    const bool persisted = AndroidMediaSource::persistAccess(subHandle);
    MM_LOG_I() << "subtitle access kept across restarts:" << persisted;
#endif

    IMediaSource *source = m_platform.mediaSource();
    const QString rawName = source ? source->displayPath(subHandle) : subHandle;
    const QString fileName = SubtitleNaming::baseName(rawName);
    const QString language = SubtitleNaming::languageOf(fileName);
    const QString title = SubtitleNaming::titleFor(fileName, language);

    const QString kept = keepSubtitleCopy(subHandle, rawName);
    const QString storedHandle = kept.isEmpty() ? subHandle : kept;

    if (!m_subtitleRepository.attach(fileHandle, storedHandle, title, language,
                                     origin)) {
        emit errorRaised(tr("That subtitle could not be saved."));
        return QString();
    }

    MM_LOG_I() << "subtitle attached as" << title << "named" << rawName;
    emit subtitleAttached(fileHandle, title);
    return title;
}

void LibraryController::detachSubtitle(const QString &fileHandle,
                                       const QString &subtitleHandle)
{
    const bool ok = subtitleHandle.isEmpty()
        ? m_subtitleRepository.detachAll(fileHandle)
        : m_subtitleRepository.detach(fileHandle, subtitleHandle);

    if (ok) {
        emit subtitleAttached(fileHandle, QString());
    }
}

QVariantList LibraryController::attachedSubtitles(const QString &fileHandle) const
{
    QVariantList result;
    const QList<ExternalSubtitle> subs = m_subtitleRepository.forFile(fileHandle);
    for (const ExternalSubtitle &sub : subs) {
        QVariantMap entry;
        entry.insert(QStringLiteral("handle"), sub.subHandle);
        entry.insert(QStringLiteral("displayName"), sub.displayName);
        entry.insert(QStringLiteral("language"), sub.language);
        entry.insert(QStringLiteral("downloaded"), sub.downloaded());
        result.append(entry);
    }
    return result;
}

QString LibraryController::subtitleMpvUrl(const QString &subtitleHandle)
{
    IMediaSource *source = m_platform.localMediaSource();
    if (!source || subtitleHandle.isEmpty()) {
        return QString();
    }

    const QString converted = convertedSubtitle(subtitleHandle);
    if (!converted.isEmpty()) {
        return converted;
    }

    const QString url = source->mpvUrl(subtitleHandle);
    if (url.isEmpty()) {
        MM_LOG_W() << "attached subtitle could not be opened for playback" << subtitleHandle;
    }
    return url;
}

QString LibraryController::keepSubtitleCopy(const QString &subtitleHandle,
                                            const QString &displayName)
{
#ifdef Q_OS_ANDROID
    constexpr qint64 maxBytes = 8 * 1024 * 1024;
    const QByteArray bytes = AndroidMediaSource::readHead(subtitleHandle, maxBytes);
    if (bytes.isEmpty()) {
        MM_LOG_W() << "could not read the subtitle to keep a copy of it" << subtitleHandle;
        return QString();
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/subtitles");
    if (!QDir().mkpath(dir)) {
        MM_LOG_W() << "could not create the kept subtitle folder" << dir;
        return QString();
    }

    QString suffix = QFileInfo(displayName).suffix().toLower();
    if (suffix.isEmpty()) {
        suffix = QStringLiteral("srt");
    }

    const QString target = dir + QLatin1Char('/')
        + QString::fromLatin1(QCryptographicHash::hash(subtitleHandle.toUtf8(),
                                                       QCryptographicHash::Sha1).toHex())
        + QLatin1Char('.') + suffix;

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(bytes) != bytes.size()) {
        MM_LOG_W() << "could not keep a copy of the subtitle at" << target << file.errorString();
        return QString();
    }

    MM_LOG_I() << "kept a copy of" << subtitleHandle << "at" << target;
    return target;
#else
    Q_UNUSED(subtitleHandle)
    Q_UNUSED(displayName)
    return QString();
#endif
}

QString LibraryController::convertedSubtitle(const QString &subtitleHandle)
{
    constexpr qint64 maxBytes = 8 * 1024 * 1024;
#ifdef Q_OS_ANDROID
    const QByteArray bytes = AndroidMediaSource::readHead(subtitleHandle, maxBytes);
#else
    QByteArray bytes;
    {
        QFile subtitle(subtitleHandle);
        if (subtitle.open(QIODevice::ReadOnly)) {
            bytes = subtitle.read(maxBytes);
        }
    }
#endif
    if (bytes.isEmpty()) {
        return QString();
    }

    const QString encoding = SubtitleEncoding::guess(bytes);
    if (encoding == QLatin1String("utf-8")) {
        return QString();
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + QStringLiteral("/subtitles");
    if (!QDir().mkpath(dir)) {
        MM_LOG_W() << "could not create the converted subtitle folder" << dir;
        return QString();
    }

    IMediaSource *source = m_platform.mediaSource();
    QString suffix = QFileInfo(source ? source->displayPath(subtitleHandle)
                                      : subtitleHandle).suffix().toLower();
    if (suffix.isEmpty()) {
        suffix = QStringLiteral("srt");
    }

    const QString target = dir + QLatin1Char('/')
        + QString::fromLatin1(QCryptographicHash::hash(subtitleHandle.toUtf8(),
                                                       QCryptographicHash::Sha1).toHex())
        + QLatin1Char('.') + suffix;

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(SubtitleEncoding::decode(bytes, encoding).toUtf8()) < 0) {
        MM_LOG_W() << "could not write the converted subtitle" << target << file.errorString();
        return QString();
    }

    MM_LOG_I() << "subtitle" << subtitleHandle << "was" << encoding
               << "- converted to UTF-8 at" << target;
    return target;
}

QVariantList LibraryController::siblingSubtitleTracks(const QString &fileHandle)
{
    QVariantList tracks;
    IMediaSource *source = m_platform.mediaSource();
    if (!source || fileHandle.isEmpty()) {
        return tracks;
    }

    QStringList found = source->siblingSubtitles(fileHandle);
    if (found.isEmpty()) {
        return tracks;
    }

    QSet<QString> attached;
    const QVariantList attachedList = attachedSubtitles(fileHandle);
    for (const QVariant &entry : attachedList) {
        attached.insert(QDir::cleanPath(QDir::fromNativeSeparators(
            entry.toMap().value(QStringLiteral("handle")).toString())).toLower());
    }
    found.erase(std::remove_if(found.begin(), found.end(), [&attached](const QString &path) {
        return attached.contains(QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower());
    }), found.end());
    if (found.isEmpty()) {
        return tracks;
    }

    struct Candidate
    {
        QString handle;
        QString base;
        QString code;
        QString title;
    };

    const auto isUrl = [](const QString &handle) {
        return handle.startsWith(QLatin1String("http://"), Qt::CaseInsensitive);
    };
    const auto nameOf = [source, isUrl](const QString &handle) {
#ifdef Q_OS_ANDROID
        const bool named = AndroidMediaSource::isContentUri(handle) || isUrl(handle);
#else
        const bool named = isUrl(handle);
#endif
        return named ? source->displayPath(handle) : handle;
    };
    const QString videoName = nameOf(fileHandle);

    QList<Candidate> candidates;
    for (const QString &handle : found) {
        const QString subtitleName = nameOf(handle);
        const SubtitleNaming::Description description =
            SubtitleNaming::describe(videoName, subtitleName);
        candidates.append({handle, SubtitleNaming::baseName(subtitleName),
                           description.code, description.title});
    }

    const QString videoBase = SubtitleNaming::baseName(videoName);
    const QString localCode = QLocale::system().name().left(2);
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](const Candidate &a, const Candidate &b) {
        const bool aExact = a.base.compare(videoBase, Qt::CaseInsensitive) == 0;
        const bool bExact = b.base.compare(videoBase, Qt::CaseInsensitive) == 0;
        if (aExact != bExact) {
            return aExact;
        }
        const bool aLocal = !a.code.isEmpty() && a.code == localCode;
        const bool bLocal = !b.code.isEmpty() && b.code == localCode;
        if (aLocal != bLocal) {
            return aLocal;
        }
        const bool aForced = a.base.contains(QLatin1String("forced"), Qt::CaseInsensitive);
        const bool bForced = b.base.contains(QLatin1String("forced"), Qt::CaseInsensitive);
        if (aForced != bForced) {
            return bForced;
        }
        return a.base.compare(b.base, Qt::CaseInsensitive) < 0;
    });

    for (const Candidate &candidate : std::as_const(candidates)) {
        const QString url = isUrl(candidate.handle) ? candidate.handle
                                                    : subtitleMpvUrl(candidate.handle);
        if (url.isEmpty()) {
            continue;
        }
        QVariantMap track;
        track.insert(QStringLiteral("url"), url);
        track.insert(QStringLiteral("title"), candidate.title);
        track.insert(QStringLiteral("language"), candidate.code);
        tracks.append(track);
    }

    MM_LOG_I() << tracks.size() << "subtitles beside" << videoBase << "will load with it";
    return tracks;
}

bool LibraryController::canRequestSubtitleFolder(const QString &fileHandle) const
{
    if (m_remote) {
        return false;
    }
#ifdef Q_OS_ANDROID
    if (fileHandle.isEmpty() || AndroidMediaSource::isContentUri(fileHandle)) {
        return false;
    }
    return !AndroidMediaSource::hasSubtitleFolderAccess(QFileInfo(fileHandle).absolutePath());
#else
    Q_UNUSED(fileHandle)
    return false;
#endif
}

void LibraryController::requestSubtitleFolder(const QString &fileHandle)
{
    if (m_remote) {
        return;
    }
#ifdef Q_OS_ANDROID
    if (fileHandle.isEmpty() || AndroidMediaSource::isContentUri(fileHandle)) {
        return;
    }
    AndroidMediaSource::requestSubtitleFolderAccess(QFileInfo(fileHandle).absolutePath());
#else
    Q_UNUSED(fileHandle)
#endif
}

QVariantList LibraryController::previewFilenameParsing() const
{
    QVariantList result;

    const QList<LibraryFile> files = m_fileRepository.all();
    for (const LibraryFile &file : files) {
        const ParsedFileName parsed = FileNameParser::parse(file.handle);

        QVariantMap entry;
        entry.insert(QStringLiteral("handle"), file.handle);
        entry.insert(QStringLiteral("fileName"), file.displayName);
        entry.insert(QStringLiteral("title"), parsed.title);
        entry.insert(QStringLiteral("year"), parsed.year);
        entry.insert(QStringLiteral("season"), parsed.season);
        entry.insert(QStringLiteral("episode"), parsed.episode);
        entry.insert(QStringLiteral("episodeTitle"), parsed.episodeTitle);
        entry.insert(QStringLiteral("releaseGroup"), parsed.releaseGroup);
        entry.insert(QStringLiteral("isEpisode"), parsed.looksLikeEpisode());
        result.append(entry);
    }

    MM_LOG_I() << "parsed" << result.size() << "filenames for review";
    return result;
}

MediaListModel *LibraryController::movies() const
{
    return m_movieModel;
}

MediaListModel *LibraryController::shows() const
{
    return m_showModel;
}

MediaListModel *LibraryController::suggestions() const
{
    return m_suggestionModel;
}

MediaListModel *LibraryController::recentTitles() const
{
    return m_recentTitleModel;
}

void LibraryController::refreshRecentTitles()
{
    constexpr qint64 kFreshSeconds = 7 * 24 * 60 * 60;

    m_recentTitleModel->setItems(
        m_mediaRepository.recentlyAddedTitles(kHomeRowLength, kFreshSeconds));
}

QVariantMap LibraryController::surpriseMe(qint64 notThisOne) const
{
    QList<MediaRecord> unwatched;
    for (const MediaListModel *list : {m_movieModel, m_showModel}) {
        for (int row = 0; row < list->rowCount(); ++row) {
            const MediaRecord title = list->at(row);
            if (title.fileCount > 0 && title.watchedCount < title.fileCount) {
                unwatched.append(title);
            }
        }
    }

    if (unwatched.size() > 1) {
        unwatched.erase(std::remove_if(unwatched.begin(), unwatched.end(),
                                       [notThisOne](const MediaRecord &title) {
                                           return title.id == notThisOne;
                                       }),
                        unwatched.end());
    }

    if (unwatched.isEmpty()) {
        MM_LOG_I() << "surprise me found nothing unwatched";
        return QVariantMap();
    }

    const MediaRecord pick =
        unwatched.at(int(QRandomGenerator::global()->bounded(qint64(unwatched.size()))));
    MM_LOG_I() << "surprise me picked" << pick.title << pick.kind << "out of"
               << unwatched.size() << "unwatched titles";

    QVariantMap result;
    result.insert(QStringLiteral("mediaId"), pick.id);
    result.insert(QStringLiteral("kind"), pick.kind);
    result.insert(QStringLiteral("title"), pick.title);
    result.insert(QStringLiteral("year"), pick.year);
    result.insert(QStringLiteral("runtimeMinutes"), pick.runtimeMinutes);
    result.insert(QStringLiteral("seasonCount"), pick.seasonCount);
    result.insert(QStringLiteral("genres"), pick.genres);
    result.insert(QStringLiteral("posterPath"), pick.posterPath);
    result.insert(QStringLiteral("handle"), pick.firstFileHandle);

    if (pick.kind == QLatin1String("tv")) {
        const QList<EpisodeRecord> episodes = m_mediaRepository.episodesFor(pick.id);
        for (const EpisodeRecord &episode : episodes) {
            if (episode.hasFile() && !episode.watched && !episode.missing) {
                result.insert(QStringLiteral("playHandle"), episode.fileHandle);
                result.insert(QStringLiteral("playSeason"), episode.season);
                result.insert(QStringLiteral("playEpisode"), episode.episode);
                result.insert(QStringLiteral("resuming"),
                              PlaybackStateRepository::resumable(
                                  episode.positionSeconds, episode.durationSeconds,
                                  episode.watched));
                break;
            }
        }
    } else {
        result.insert(QStringLiteral("playHandle"), pick.firstFileHandle);
    }
    return result;
}

GenreRowModel *LibraryController::genreRows() const
{
    return m_genreRowModel;
}

MediaListModel *LibraryController::movieGrid() const
{
    return m_movieGridModel;
}

MediaListModel *LibraryController::showGrid() const
{
    return m_showGridModel;
}

int LibraryController::movieSort() const
{
    return m_movieSort;
}

void LibraryController::setMovieSort(int sort)
{
    setGridOption(m_movieSort, sort, QStringLiteral("movies/sort"));
}

int LibraryController::movieFilter() const
{
    return m_movieFilter;
}

void LibraryController::setMovieFilter(int filter)
{
    setGridOption(m_movieFilter, filter, QStringLiteral("movies/filter"));
}

int LibraryController::showSort() const
{
    return m_showSort;
}

void LibraryController::setShowSort(int sort)
{
    setGridOption(m_showSort, sort, QStringLiteral("shows/sort"));
}

int LibraryController::showFilter() const
{
    return m_showFilter;
}

void LibraryController::setShowFilter(int filter)
{
    setGridOption(m_showFilter, filter, QStringLiteral("shows/filter"));
}

QString LibraryController::movieGenre() const
{
    return m_movieGenre;
}

void LibraryController::setMovieGenre(const QString &genre)
{
    setGridGenre(m_movieGenre, genre, QStringLiteral("movies/genre"));
}

QString LibraryController::showGenre() const
{
    return m_showGenre;
}

void LibraryController::setShowGenre(const QString &genre)
{
    setGridGenre(m_showGenre, genre, QStringLiteral("shows/genre"));
}

int LibraryController::movieGridSize() const
{
    return m_movieGridSize;
}

void LibraryController::setMovieGridSize(int size)
{
    if (m_movieGridSize == size) {
        return;
    }
    m_movieGridSize = size;
    m_settings.setValue(QStringLiteral("movies/gridSize"), size);
    emit gridOptionsChanged();
}

int LibraryController::showGridSize() const
{
    return m_showGridSize;
}

void LibraryController::setShowGridSize(int size)
{
    if (m_showGridSize == size) {
        return;
    }
    m_showGridSize = size;
    m_settings.setValue(QStringLiteral("shows/gridSize"), size);
    emit gridOptionsChanged();
}

QStringList LibraryController::movieGenres() const
{
    return m_movieGenres;
}

QStringList LibraryController::showGenres() const
{
    return m_showGenres;
}

void LibraryController::setGridGenre(QString &option, const QString &value, const QString &key)
{
    if (option == value) {
        return;
    }
    option = value;
    m_settings.setValue(key, value);
    MM_LOG_I() << "grid option" << key << "->" << value;
    emit gridOptionsChanged();
    refreshGrids();
}

void LibraryController::setGridOption(int &option, int value, const QString &key)
{
    if (option == value) {
        return;
    }
    option = value;
    m_settings.setValue(key, value);
    MM_LOG_I() << "grid option" << key << "->" << value;
    emit gridOptionsChanged();
    refreshGrids();
}

void LibraryController::refreshGrids()
{
    QElapsedTimer timer;
    timer.start();

    const QHash<qint64, qint64> lastAdded = m_mediaRepository.lastAddedByTitle();

    QList<MediaRecord> movies;
    movies.reserve(m_movieModel->rowCount());
    for (int row = 0; row < m_movieModel->rowCount(); ++row) {
        movies.append(m_movieModel->at(row));
    }
    QList<MediaRecord> shows;
    shows.reserve(m_showModel->rowCount());
    for (int row = 0; row < m_showModel->rowCount(); ++row) {
        shows.append(m_showModel->at(row));
    }

    const QStringList movieGenres = TitleArrangement::genresIn(movies);
    const QStringList showGenres = TitleArrangement::genresIn(shows);
    if (movieGenres != m_movieGenres || showGenres != m_showGenres) {
        m_movieGenres = movieGenres;
        m_showGenres = showGenres;
        emit gridGenresChanged();
    }

    const QString movieGenre = m_movieGenres.contains(m_movieGenre) ? m_movieGenre : QString();
    const QString showGenre = m_showGenres.contains(m_showGenre) ? m_showGenre : QString();

    m_movieGridModel->setItems(TitleArrangement::arrange(
        movies, m_movieSort, m_movieFilter, movieGenre, lastAdded));
    m_showGridModel->setItems(TitleArrangement::arrange(
        shows, m_showSort, m_showFilter, showGenre, lastAdded));

    MM_LOG_D() << "movie and show pages arranged:" << m_movieGridModel->rowCount()
               << "of" << movies.size() << "films," << m_showGridModel->rowCount()
               << "of" << shows.size() << "shows in" << timer.elapsed() << "ms";
}

CollectionListModel *LibraryController::collections() const
{
    return m_collectionModel;
}

CollectionListModel *LibraryController::collectionGrid() const
{
    return m_collectionGridModel;
}

int LibraryController::collectionFilter() const
{
    return m_collectionFilter;
}

void LibraryController::setCollectionFilter(int filter)
{
    if (m_collectionFilter == filter) {
        return;
    }
    m_collectionFilter = filter;
    m_settings.setValue(QStringLiteral("collections/filter"), filter);
    MM_LOG_I() << "grid option collections/filter ->" << filter;
    emit gridOptionsChanged();
    m_collectionGridModel->setItems(
        CollectionShelf::filtered(m_collectionModel->items(), m_collectionFilter));
}

int LibraryController::collectionGridSize() const
{
    return m_collectionGridSize;
}

void LibraryController::setCollectionGridSize(int size)
{
    if (m_collectionGridSize == size) {
        return;
    }
    m_collectionGridSize = size;
    m_settings.setValue(QStringLiteral("collections/gridSize"), size);
    emit gridOptionsChanged();
}

QList<UniverseCatalog::Universe> LibraryController::universes() const
{
    return m_universes;
}

void LibraryController::refreshCollections()
{
    QElapsedTimer timer;
    timer.start();

    QList<MediaRecord> films;
    films.reserve(m_movieModel->rowCount() + m_showModel->rowCount());
    for (int row = 0; row < m_movieModel->rowCount(); ++row) {
        films.append(m_movieModel->at(row));
    }
    for (int row = 0; row < m_showModel->rowCount(); ++row) {
        films.append(m_showModel->at(row));
    }

    const QList<CollectionShelf::Summary> all = CollectionShelf::build(
        m_mediaRepository.ownedCollections(), m_mediaRepository.collectionsByMedia(),
        films, QDate::currentDate(), m_universes, m_mediaRepository.customCollections(),
        m_mediaRepository.hiddenCollectionIds(),
        m_mediaRepository.hiddenCollectionFilmsByScope(),
        m_mediaRepository.addedCollectionFilmsByScope());

    m_collections = all;
    m_collectionModel->setItems(all);
    m_collectionGridModel->setItems(CollectionShelf::filtered(all, m_collectionFilter));

    MM_LOG_D() << "collections arranged:" << m_collectionGridModel->rowCount() << "of"
               << all.size() << "shown, filter" << m_collectionFilter << "in"
               << timer.elapsed() << "ms";

    refreshCollectionPage();
}

void LibraryController::noteUi(const QString &what) const
{
    MM_LOG_D() << "ui:" << what;
}

MediaListModel *LibraryController::pickerTitles() const
{
    return m_pickerModel;
}

QString LibraryController::pickerQuery() const
{
    return m_pickerQuery;
}

void LibraryController::setPickerQuery(const QString &query)
{
    if (m_pickerQuery == query) {
        return;
    }
    m_pickerQuery = query;
    emit pickerChanged();
    refreshPickerTitles();
}

int LibraryController::pickerKind() const
{
    return m_pickerKind;
}

void LibraryController::setPickerKind(int kind)
{
    if (m_pickerKind == kind) {
        return;
    }
    m_pickerKind = kind;
    emit pickerChanged();
    refreshPickerTitles();
}

void LibraryController::refreshPickerTitles()
{
    const QString wanted = TextFold::key(m_pickerQuery.trimmed());

    QList<MediaRecord> titles;
    const auto take = [&](MediaListModel *model) {
        for (int row = 0; row < model->rowCount(); ++row) {
            const MediaRecord title = model->at(row);
            if (wanted.isEmpty() || TextFold::key(title.title).contains(wanted)) {
                titles.append(title);
            }
        }
    };

    if (m_pickerKind != 2) {
        take(m_movieModel);
    }
    if (m_pickerKind != 1) {
        take(m_showModel);
    }

    std::sort(titles.begin(), titles.end(), &MediaRepository::listsBefore);
    m_pickerModel->setItems(titles);
}

qint64 LibraryController::createCollection(const QString &name,
                                           const QString &description,
                                           const QString &coverMode,
                                           const QVariantList &mediaIds)
{
    if (refusesWhileHeld("making a collection")) {
        return 0;
    }
    CustomCollectionRecord collection;
    collection.name = name;
    collection.description = description;
    collection.coverMode = coverMode;
    for (const QVariant &mediaId : mediaIds) {
        const qint64 id = mediaId.toLongLong();
        if (id > 0 && !collection.mediaIds.contains(id)) {
            collection.mediaIds.append(id);
        }
    }

    const qint64 id = m_mediaRepository.createCustomCollection(collection);
    if (id <= 0) {
        emit errorRaised(tr("The collection could not be saved."));
        return 0;
    }

    refreshCollections();
    return CollectionShelf::customId(id);
}

QVariantMap LibraryController::collectionOfMedia(qint64 mediaId) const
{
    QVariantMap result;
    if (mediaId <= 0) {
        return result;
    }

    const qint64 collectionId = m_mediaRepository.collectionIdFor(mediaId);
    const MediaRecord media = m_mediaRepository.mediaById(mediaId);

    qint64 wanted = collectionId;
    int place = 0;
    for (int index = 0; index < m_universes.size(); ++index) {
        const UniverseCatalog::Universe &universe = m_universes.at(index);
        if (!universe.contains(media.tmdbId, collectionId, media.title, media.originalTitle,
                               media.genres)) {
            continue;
        }
        wanted = -(index + 1);
        for (int at = 0; at < universe.storyOrder.size(); ++at) {
            if (universe.storyOrder.at(at).tmdbId == media.tmdbId) {
                place = at + 1;
                break;
            }
        }
        break;
    }

    if (wanted == 0) {
        return result;
    }

    if (place == 0 && collectionId > 0) {
        const CollectionRecord collection =
            m_mediaRepository.collectionById(collectionId);
        for (int at = 0; at < collection.parts.size(); ++at) {
            if (collection.parts.at(at).tmdbId == media.tmdbId) {
                place = at + 1;
                break;
            }
        }
    }

    for (const CollectionShelf::Summary &summary : m_collections) {
        if (summary.id != wanted) {
            continue;
        }
        result.insert(QStringLiteral("id"), summary.id);
        result.insert(QStringLiteral("name"), summary.name);
        result.insert(QStringLiteral("owned"), summary.owned);
        result.insert(QStringLiteral("total"), summary.total);
        result.insert(QStringLiteral("place"), place);
        result.insert(QStringLiteral("posterPath"), summary.posterPath);
        result.insert(QStringLiteral("backdropPath"), summary.backdropPath);
        break;
    }
    return result;
}

void LibraryController::hideCollection(qint64 collectionId, const QString &name)
{
    if (refusesWhileHeld("removing a collection")) {
        return;
    }
    if (collectionId == 0 || !m_mediaRepository.hideCollection(collectionId, name)) {
        return;
    }

    if (m_collectionPageModel->collectionId() == collectionId) {
        m_collectionPageModel->setCollectionId(0);
    }
    refreshCollections();
    emit hiddenCollectionsChanged();
}

void LibraryController::restoreCollection(qint64 collectionId)
{
    if (refusesWhileHeld("putting a collection back")) {
        return;
    }
    if (collectionId == 0 || !m_mediaRepository.restoreCollection(collectionId)) {
        return;
    }

    refreshCollections();
    emit hiddenCollectionsChanged();
}

QString LibraryController::scopeOfCollection(qint64 collectionId) const
{
    if (collectionId < 0) {
        const qint64 index = -collectionId - 1;
        if (index >= m_universes.size()) {
            return QString();
        }
        return CollectionShelf::universeScope(m_universes.at(int(index)).key);
    }
    if (collectionId > 0) {
        return CollectionShelf::collectionScope(collectionId);
    }
    return QString();
}

QString LibraryController::nameOfCollection(qint64 collectionId) const
{
    for (const CollectionShelf::Summary &summary : m_collections) {
        if (summary.id == collectionId) {
            return summary.name;
        }
    }
    return QString();
}

void LibraryController::hideCollectionFilm(qint64 collectionId,
                                           qint64 tmdbId,
                                           const QString &title)
{
    if (refusesWhileHeld("taking a film out of a collection")) {
        return;
    }
    const QString scope = scopeOfCollection(collectionId);
    if (scope.isEmpty() || tmdbId <= 0) {
        return;
    }

    const bool wasAdded =
        m_mediaRepository.addedCollectionFilmsByScope().value(scope).contains(tmdbId);
    const bool written =
        wasAdded ? m_mediaRepository.removeAddedCollectionFilm(scope, tmdbId)
                 : m_mediaRepository.hideCollectionFilm(scope, tmdbId, title,
                                                        nameOfCollection(collectionId));
    if (!written) {
        return;
    }

    refreshCollections();
    emit hiddenCollectionsChanged();
}

void LibraryController::addFilmToCollection(qint64 collectionId,
                                            qint64 tmdbId,
                                            const QString &title)
{
    if (refusesWhileHeld("putting a film into a collection")) {
        return;
    }
    const QString scope = scopeOfCollection(collectionId);
    if (scope.isEmpty() || tmdbId <= 0) {
        return;
    }

    m_mediaRepository.restoreCollectionFilm(scope, tmdbId);

    if (!m_mediaRepository.addCollectionFilm(scope, tmdbId, title,
                                             nameOfCollection(collectionId))) {
        return;
    }

    refreshCollections();
    emit hiddenCollectionsChanged();
}

void LibraryController::restoreCollectionFilm(const QString &scope, qint64 tmdbId)
{
    if (refusesWhileHeld("putting a film back into a collection")) {
        return;
    }
    if (scope.isEmpty() || tmdbId <= 0
        || !m_mediaRepository.restoreCollectionFilm(scope, tmdbId)) {
        return;
    }

    refreshCollections();
    emit hiddenCollectionsChanged();
}

QVariantList LibraryController::hiddenCollectionFilms() const
{
    QVariantList list;
    const QList<HiddenCollectionFilmRecord> hidden = m_mediaRepository.hiddenCollectionFilms();
    for (const HiddenCollectionFilmRecord &entry : hidden) {
        QVariantMap row;
        row.insert(QStringLiteral("scope"), entry.scope);
        row.insert(QStringLiteral("tmdbId"), entry.tmdbId);
        row.insert(QStringLiteral("title"), entry.title);
        row.insert(QStringLiteral("collectionName"), entry.collectionName);
        list.append(row);
    }
    return list;
}

void LibraryController::discardShow(const QStringList &fileHandles,
                                    const QString &title, const QString &folder)
{
    if (refusesWhileHeld("refusing a show")) {
        return;
    }
    if (fileHandles.isEmpty()
        || !m_mediaRepository.discardShow(fileHandles, title, folder)) {
        return;
    }

    m_unmatchedHandles.clear();
    reloadUnmatchedShows();
    emit discardedShowsChanged();
}

void LibraryController::restoreShow(const QString &title)
{
    if (refusesWhileHeld("putting a show back")) {
        return;
    }
    if (title.isEmpty() || !m_mediaRepository.restoreShow(title)) {
        return;
    }

    m_unmatchedHandles.clear();
    reloadUnmatchedShows();
    emit discardedShowsChanged();
}

RemovedFileModel *LibraryController::removedFiles() const
{
    return m_removedFiles;
}

void LibraryController::removeFromLibrary(const QString &handle)
{
    if (refusesWhileHeld("removing a file")) {
        return;
    }
    const LibraryFile file = m_fileRepository.byHandle(handle);
    if (!file.isValid()) {
        MM_LOG_W() << "asked to remove a file the library does not have" << handle;
        return;
    }
    IMediaSource *source = m_platform.mediaSource();
    const bool gone = file.missing || !source || !source->exists(handle);

    if ((file.isMatched() || file.matchSuggested) && !gone) {
        MM_LOG_W() << "a matched file that is still there stays in the library:"
                   << file.displayName;
        return;
    }
    if (gone) {
        MM_LOG_I() << "taking a missing file out of the library:" << file.displayName;
    }

    takeOutOfLibrary({ file });
}

int LibraryController::hideableUnmatchedCount()
{
    if (m_unmatchedShowsStale) {
        m_unmatchedShowsStale = false;
        reloadUnmatchedShows();
    }

    int count = 0;
    const QList<LibraryFile> unmatched = m_fileRepository.unmatched();
    for (const LibraryFile &file : unmatched) {
        if (!m_unmatchedShows->containsFile(file.handle)) {
            ++count;
        }
    }
    return count;
}

void LibraryController::hideAllUnmatched()
{
    if (refusesWhileHeld("hiding unmatched files")) {
        return;
    }
    if (m_unmatchedShowsStale) {
        m_unmatchedShowsStale = false;
        reloadUnmatchedShows();
    }

    const QList<LibraryFile> unmatched = m_fileRepository.unmatched();

    QList<LibraryFile> hidden;
    hidden.reserve(unmatched.size());
    int grouped = 0;
    for (const LibraryFile &file : unmatched) {
        if (m_unmatchedShows->containsFile(file.handle)) {
            ++grouped;
            continue;
        }
        hidden.append(file);
    }

    MM_LOG_I() << "hiding" << hidden.size() << "unmatched files; kept" << grouped
               << "waiting in Identify shows and every suggested match";
    takeOutOfLibrary(hidden);
}

void LibraryController::takeOutOfLibrary(const QList<LibraryFile> &files)
{
    if (files.isEmpty()) {
        return;
    }

    QHash<QString, QString> folderPaths;
    if (IMediaSource *source = m_platform.mediaSource()) {
        for (const LibraryFile &file : files) {
            if (!file.parentHandle.isEmpty() && !folderPaths.contains(file.parentHandle)) {
                folderPaths.insert(file.parentHandle, source->displayPath(file.parentHandle));
            }
        }
    }

    QList<qint64> titles;
    for (const LibraryFile &file : files) {
        const MediaRecord media = m_mediaRepository.mediaForFile(file.id);
        if (media.isValid() && !titles.contains(media.id)) {
            titles.append(media.id);
        }
    }

    const QList<RemovedFileRecord> removed =
        m_fileRepository.removeFromLibrary(files, folderPaths);
    if (removed.isEmpty()) {
        emit errorRaised(tr("Those files could not be taken out of the library."));
        return;
    }

    QStringList handles;
    handles.reserve(removed.size());
    for (const RemovedFileRecord &record : removed) {
        handles.append(record.handle);
        m_fileModel->removeFile(record.handle);
        m_continueModel->removeFile(record.handle);
        m_searchModel->removeFile(record.handle);
        m_searchEpisodeModel->removeFile(record.handle);
        if (m_scanIndexLoaded && record.folderId == m_scanFolderId) {
            ScanIndexRow row;
            row.removed = true;
            m_scanIndex.insert(record.handle, row);
        }
    }

    m_unmatchedShows->removeFiles(handles);
    m_removedFiles->prependFiles(removed);
    placeTitles(titles);
    recountFiles();
}

void LibraryController::restoreRemoved(const QString &handle)
{
    if (refusesWhileHeld("putting a file back")) {
        return;
    }
    const QList<qint64> folderIds = m_fileRepository.restoreRemoved({ handle });
    m_removedFiles->removeHandles({ handle });
    rescanFoldersQuiet(folderIds);
}

void LibraryController::restoreAllRemoved()
{
    if (refusesWhileHeld("putting files back")) {
        return;
    }
    const QStringList handles = m_removedFiles->handles();
    if (handles.isEmpty()) {
        return;
    }

    const QList<qint64> folderIds = m_fileRepository.restoreRemoved(handles);
    m_removedFiles->removeHandles(handles);
    rescanFoldersQuiet(folderIds);
}

void LibraryController::rescanFoldersQuiet(const QList<qint64> &folderIds)
{
    if (folderIds.isEmpty() || refusesWhileHeld("bringing files back")) {
        return;
    }

    if (!m_scanning && !m_quietScan) {
        m_quietScan = true;
        m_quietScanBaseline = m_fileCount;
        emit scanningChanged();
    }

    MM_LOG_I() << "quietly rescanning" << folderIds.size()
               << "folders to bring files back";
    for (qint64 id : folderIds) {
        beginScan(id);
    }
}

QVariantList LibraryController::discardedShows() const
{
    QVariantList result;

    const QList<DiscardedShowRecord> discarded = m_mediaRepository.discardedShows();
    for (const DiscardedShowRecord &show : discarded) {
        QVariantMap entry;
        entry.insert(QStringLiteral("title"), show.title);
        entry.insert(QStringLiteral("folder"), show.folder);
        entry.insert(QStringLiteral("fileCount"), show.fileCount);
        result.append(entry);
    }
    return result;
}

QVariantList LibraryController::hiddenCollections() const
{
    QVariantList list;
    const QList<HiddenCollectionRecord> hidden = m_mediaRepository.hiddenCollections();
    for (const HiddenCollectionRecord &entry : hidden) {
        QVariantMap row;
        row.insert(QStringLiteral("collectionId"), entry.collectionId);
        row.insert(QStringLiteral("name"), entry.name);
        row.insert(QStringLiteral("custom"), CollectionShelf::isCustom(entry.collectionId));
        list.append(row);
    }
    return list;
}

CollectionPageModel *LibraryController::collectionPage() const
{
    return m_collectionPageModel;
}

void LibraryController::refreshCollectionPage()
{
    const qint64 collectionId = m_collectionPageModel->collectionId();
    if (collectionId == 0) {
        m_collectionPageModel->setPage(CollectionPage::Page());
        return;
    }

    QElapsedTimer timer;
    timer.start();

    QList<MediaRecord> films;
    films.reserve(m_movieModel->rowCount() + m_showModel->rowCount());
    for (int row = 0; row < m_movieModel->rowCount(); ++row) {
        films.append(m_movieModel->at(row));
    }
    for (int row = 0; row < m_showModel->rowCount(); ++row) {
        films.append(m_showModel->at(row));
    }

    const CollectionPage::Page page = CollectionPage::build(
        collectionId, m_collectionPageModel->order(), m_mediaRepository.ownedCollections(),
        m_mediaRepository.collectionsByMedia(), films, m_universes, QDate::currentDate(),
        m_mediaRepository.filmDetails(), m_mediaRepository.customCollections(),
        m_mediaRepository.hiddenCollectionFilmsByScope(),
        m_mediaRepository.addedCollectionFilmsByScope());
    m_collectionPageModel->setPage(page);

    MM_LOG_D() << "collection page" << collectionId << page.header.name << ":"
               << page.header.total << "films," << page.header.owned << "in the library,"
               << page.rows.size() << "rows in" << timer.elapsed() << "ms";
}

void LibraryController::refreshGenreRows()
{
    constexpr int kSmallestRow = 6;

    QElapsedTimer timer;
    timer.start();

    QList<MediaRecord> titles;
    titles.reserve(m_movieModel->rowCount() + m_showModel->rowCount());
    for (int row = 0; row < m_movieModel->rowCount(); ++row) {
        titles.append(m_movieModel->at(row));
    }
    for (int row = 0; row < m_showModel->rowCount(); ++row) {
        titles.append(m_showModel->at(row));
    }

    const QList<GenreShelf::Row> rows =
        GenreShelf::build(titles, m_mediaRepository.lastAddedByTitle(), kSmallestRow);
    m_genreRowModel->setRows(rows);

    MM_LOG_D() << "genre rows:" << rows.size() << "from" << titles.size()
               << "titles in" << timer.elapsed() << "ms";
}

UnmatchedShowModel *LibraryController::unmatchedShows() const
{
    return m_unmatchedShows;
}

void LibraryController::reloadUnmatchedShows()
{
    QStringList handles;
    const QList<LibraryFile> unmatched = m_fileRepository.unmatched();
    handles.reserve(unmatched.size());
    for (const LibraryFile &file : unmatched) {
        handles.append(file.handle);
    }

    if (handles == m_unmatchedHandles) {
        MM_LOG_D() << "unmatched files unchanged, show grouping skipped for"
                   << handles.size() << "files";
        return;
    }
    m_unmatchedHandles = handles;

    const QSet<QString> refused = m_mediaRepository.discardedShowFiles();

    QList<ShowGrouping::File> files;
    files.reserve(unmatched.size());
    for (const LibraryFile &file : unmatched) {
        if (refused.contains(file.handle)) {
            continue;
        }

        QString path = file.handle;
        if (!file.parentHandle.isEmpty()) {
            path = file.parentHandle.endsWith(QLatin1Char('/'))
                ? file.parentHandle + file.displayName
                : file.parentHandle + QLatin1Char('/') + file.displayName;
        }
        files.append({file.handle, path, file.displayName});
    }

    const QList<ShowGrouping::Show> grouped = ShowGrouping::group(files);

    QHash<QString, QString> folderPaths;
    if (IMediaSource *source = m_platform.mediaSource()) {
        for (const ShowGrouping::Show &show : grouped) {
            for (const QString &folder : show.folderHandles) {
                if (!folderPaths.contains(folder)) {
                    folderPaths.insert(folder, source->displayPath(folder));
                }
            }
        }
    }

    m_unmatchedShows->setShows(grouped, folderPaths);

    MM_LOG_I() << "unmatched files group into" << grouped.size()
               << "shows out of" << handles.size() << "files";
}

void LibraryController::reloadMedia()
{
    QElapsedTimer timer;
    timer.start();

    const QList<MediaRecord> movies =
        m_mediaRepository.allOfKind(QStringLiteral("movie"));
    const QList<MediaRecord> shows =
        m_mediaRepository.allOfKind(QStringLiteral("tv"));
    const qint64 queried = timer.elapsed();

    m_movieModel->setItems(movies);
    m_showModel->setItems(shows);
    m_suggestionModel->setItems(m_mediaRepository.withSuggestions());
    refreshRecentTitles();
    refreshGenreRows();
    refreshGrids();
    refreshCollections();
    refreshPickerTitles();
    reloadUnmatchedShows();

    MM_LOG_D() << "media lists reloaded:" << m_movieModel->rowCount()
               << "movies," << m_showModel->rowCount() << "shows -"
               << queried << "ms in the database,"
               << (timer.elapsed() - queried) << "ms into the models";
}

ShowEpisodeModel *LibraryController::showEpisodes() const
{
    return m_showEpisodes;
}

QVariantMap LibraryController::parsedNameFor(const QString &fileHandle) const
{
    QString fileName;
    const LibraryFile file = m_fileRepository.byHandle(fileHandle);
    if (file.isValid() && !file.displayName.isEmpty()) {
        fileName = file.displayName;
    } else {
        const IMediaSource *source = m_platform.mediaSource();
        fileName = QFileInfo(source ? source->displayPath(fileHandle) : fileHandle).fileName();
    }

    const ParsedFileName parsed = FileNameParser::parse(fileName);

    QVariantMap entry;
    entry.insert(QStringLiteral("title"), parsed.title);
    entry.insert(QStringLiteral("year"), parsed.year);
    entry.insert(QStringLiteral("season"), parsed.season);
    entry.insert(QStringLiteral("episode"), parsed.episode);
    entry.insert(QStringLiteral("isEpisode"), parsed.looksLikeEpisode());
    return entry;
}

void LibraryController::trackThumbnailWants(QAbstractItemModel *model,
                                            std::function<QString(int)> handleAt,
                                            std::function<bool(int)> wantsAt)
{
    const auto markRows = [this, model, handleAt, wantsAt](int first, int last) {
        for (int row = first; row <= last; ++row) {
            wantThumbnail(model, handleAt(row), wantsAt(row));
        }
    };

    connect(model, &QAbstractItemModel::rowsInserted, this,
            [markRows](const QModelIndex &, int first, int last) {
        markRows(first, last);
    });

    connect(model, &QAbstractItemModel::dataChanged, this,
            [markRows](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
        markRows(topLeft.row(), bottomRight.row());
    });

    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this, model, handleAt](const QModelIndex &, int first, int last) {
        for (int row = first; row <= last; ++row) {
            wantThumbnail(model, handleAt(row), false);
        }
    });

    connect(model, &QAbstractItemModel::modelAboutToBeReset, this, [this, model]() {
        const QSet<QString> held = m_thumbnailWantsByModel.take(model);
        for (const QString &handle : held) {
            releaseThumbnailWant(handle);
        }
        if (!held.isEmpty()) {
            scheduleThumbnailFlush();
        }
    });

    connect(model, &QAbstractItemModel::modelReset, this, [model, markRows]() {
        markRows(0, model->rowCount() - 1);
    });
}

void LibraryController::wantThumbnail(QAbstractItemModel *model,
                                      const QString &handle,
                                      bool wanted)
{
    if (handle.isEmpty()) {
        return;
    }

    QSet<QString> &held = m_thumbnailWantsByModel[model];
    if (wanted == held.contains(handle)) {
        return;
    }

    if (wanted) {
        held.insert(handle);
        if (++m_thumbnailWantCounts[handle] == 1) {
            m_thumbnailsToUnwant.remove(handle);
            if (!m_thumbnailsToWant.contains(handle)) {
                m_thumbnailsToWant.insert(handle);
                m_thumbnailWantOrder.append(handle);
            }
        }
    } else {
        held.remove(handle);
        releaseThumbnailWant(handle);
    }

    scheduleThumbnailFlush();
}

void LibraryController::releaseThumbnailWant(const QString &handle)
{
    const auto it = m_thumbnailWantCounts.find(handle);
    if (it == m_thumbnailWantCounts.end()) {
        return;
    }
    if (--it.value() > 0) {
        return;
    }

    m_thumbnailWantCounts.erase(it);
    m_thumbnailsToWant.remove(handle);
    m_thumbnailsToUnwant.insert(handle);
}

void LibraryController::scheduleThumbnailFlush()
{
    if (m_thumbnailFlushScheduled) {
        return;
    }
    m_thumbnailFlushScheduled = true;
    QMetaObject::invokeMethod(this, &LibraryController::flushThumbnailWants,
                              Qt::QueuedConnection);
}

void LibraryController::flushThumbnailWants()
{
    m_thumbnailFlushScheduled = false;

    QStringList added;
    added.reserve(m_thumbnailsToWant.size());
    for (const QString &handle : std::as_const(m_thumbnailWantOrder)) {
        if (m_thumbnailsToWant.remove(handle)) {
            added.append(handle);
        }
    }
    m_thumbnailWantOrder.clear();
    m_thumbnailsToWant.clear();

    const QStringList gone(m_thumbnailsToUnwant.cbegin(), m_thumbnailsToUnwant.cend());
    m_thumbnailsToUnwant.clear();

    if (!m_thumbnails || (added.isEmpty() && gone.isEmpty())) {
        return;
    }

    m_thumbnails->unwant(gone);
    m_thumbnails->want(added);

    MM_LOG_D() << "frames wanted:" << added.size() << "more," << gone.size()
               << "fewer," << m_thumbnailWantCounts.size() << "in all";
}

void LibraryController::clearThumbnailCache()
{
    if (!m_thumbnails) {
        return;
    }
    m_thumbnails->clearCache();
    reloadCurrentView();
}

void LibraryController::setThumbnailsEnabled(bool enabled)
{
    if (m_thumbnails) {
        m_thumbnails->setEnabled(enabled);
    }
}

void LibraryController::setThumbnailsDeferred(bool deferred)
{
    if (!m_thumbnails) {
        return;
    }

    m_thumbnails->setDeferred(deferred);
}

void LibraryController::setProbesDeferred(bool deferred)
{
    if (!m_probes) {
        return;
    }
    m_probes->setDeferred(deferred);
}

void LibraryController::setMatchingBusy(bool busy)
{
    if (busy || !m_unmatchedShowsStale) {
        return;
    }

    m_unmatchedShowsStale = false;
    MM_LOG_D() << "scan and lookup have gone quiet, grouping what is still unmatched";
    reloadUnmatchedShows();
}

void LibraryController::setProbesEnabled(bool enabled)
{
    if (!m_probes) {
        return;
    }
    m_probes->setEnabled(enabled);
}

void LibraryController::probeUnprobed(bool retryFailed)
{
    if (!m_probes || !m_probes->isEnabled() || refusesWhileHeld("probing")) {
        return;
    }

    if (retryFailed) {
        m_fileRepository.clearProbeAttempts();
    }

    const QList<LibraryFile> files = m_fileRepository.neverProbed();
    if (files.isEmpty()) {
        return;
    }

    QList<ProbeRequest> requests;
    requests.reserve(files.size());
    for (const LibraryFile &file : files) {
        requests.append({file.handle, file.handle});
    }

    MM_LOG_I() << files.size() << "files have never been probed";
    m_probes->requestAll(requests);
}

void LibraryController::onProbed(const ProbeInfo &info)
{
    if (!info.valid) {
        m_fileRepository.markProbeAttemptedFor(info.handle);
        return;
    }

    recordProbe(info.handle,
                info.durationSeconds,
                info.container,
                info.videoCodec,
                info.audioCodec,
                info.width,
                info.height,
                info.hdr,
                info.audioTrackCount,
                info.subtitleTrackCount);
}

void LibraryController::setWatched(qint64 fileId, bool watched)
{
    MM_LOG_I() << "file" << fileId << (watched ? "marked watched" : "marked unwatched");
    const LibraryFile file = m_fileRepository.byId(fileId);
    if (m_playbackRepository.setWatched(fileId, watched) && continuesWatching(file)) {
        m_continueCount = qMax(0, m_continueCount - 1);
    }
    refreshPlayedFiles({fileId});
    emit fileCountChanged();
}

void LibraryController::setAllWatched(const QStringList &fileHandles, bool watched)
{
    QList<qint64> fileIds;
    fileIds.reserve(fileHandles.size());
    int continuing = 0;
    for (const QString &handle : fileHandles) {
        const LibraryFile file = m_fileRepository.byHandle(handle);
        if (file.isValid()) {
            fileIds.append(file.id);
            if (continuesWatching(file)) {
                ++continuing;
            }
        }
    }

    if (fileIds.isEmpty()) {
        MM_LOG_W() << "none of" << fileHandles.size()
                   << "files to mark are in the library";
        return;
    }

    MM_LOG_I() << fileIds.size() << "of" << fileHandles.size() << "files"
               << (watched ? "being marked watched together"
                           : "being marked unwatched together");

    if (m_playbackRepository.setWatched(fileIds, watched)) {
        m_continueCount = qMax(0, m_continueCount - continuing);
    }
    refreshPlayedFiles(fileIds);
    emit fileCountChanged();
}

void LibraryController::removeFromContinueWatching(qint64 fileId)
{
    MM_LOG_I() << "file" << fileId << "removed from continue watching";
    const LibraryFile file = m_fileRepository.byId(fileId);
    if (m_playbackRepository.clear(fileId) && continuesWatching(file)) {
        m_continueCount = qMax(0, m_continueCount - 1);
    }
    refreshPlayedFiles({fileId});
    emit fileCountChanged();
}
