#include "Metadata/MetadataService.h"

#include "AppSettings.h"
#include "Data/Database.h"
#include "MmLog.h"
#include "Library/CollectionPage.h"
#include "Metadata/FileNameParser.h"
#include "Metadata/MatchDecision.h"
#include "Metadata/MatchScorer.h"
#include "Metadata/PosterUrlResolver.h"
#include "Metadata/ShowGrouping.h"
#include "Metadata/TmdbClient.h"
#include "Streaming/StreamProtocol.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QSslSocket>
#include <QStringList>
#include <QUrl>

#include <algorithm>
#include <utility>

using namespace Makimedia::Tmdb;

namespace {

constexpr qint64 kCollectionMaxAgeSeconds = 30 * 24 * 60 * 60;

ParsedFileName parsedFor(const LibraryFile &file)
{
    if (file.parentHandle.isEmpty()) {
        return FileNameParser::parse(file.displayName);
    }
    return FileNameParser::parsePath(file.parentHandle + QLatin1Char('/')
                                     + file.displayName);
}

}

MetadataService::MetadataService(const AppSettings &settings,
                                 Database &database,
                                 QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_fileRepository(database)
    , m_mediaRepository(database)
    , m_network(new QNetworkAccessManager)
    , m_config(new MakimediaTmdbConfig(settings))
{
    m_requestBuilder.reset(new TmdbRequestBuilder(*m_config));
    m_client.reset(new TmdbClient(*m_network, *m_requestBuilder));

    if (m_config->hasKey()) {
        m_posters.reset(new PosterUrlResolver(*m_client));

        connect(m_posters.data(), &PosterUrlResolver::configurationLoaded,
                this, [this]() {
            ++m_artworkRevision;
            MM_LOG_I() << "image configuration ready, refreshing artwork, revision"
                       << m_artworkRevision;
            emit artworkChanged();
            emit artworkLanded(QStringList(), true);
            m_prefetchStart.start();

            if (m_setupImagesWait.isActive()) {
                m_setupImagesWait.stop();
                startSetupImages();
            }
        });
    }

    m_setupImagesWait.setSingleShot(true);
    m_setupImagesWait.setInterval(15000);
    connect(&m_setupImagesWait, &QTimer::timeout, this, [this]() {
        MM_LOG_W() << "setup gave up waiting for TMDB's image configuration,"
                   << "no artwork can be downloaded";
        startSetupImages();
    });

    m_posterCache.reset(new PosterCache(*m_network));

    connect(m_posterCache.data(), &PosterCache::posterSaved,
            this, [this](const QString &key, const QString &) {
        const bool fromServer = m_fromServer.remove(key);

        if (m_prefetchedKeys.remove(key)) {
            return;
        }
        if (fromServer) {
            ++m_landedFromServer;
        } else {
            ++m_landedFromTmdb;
        }
        m_landedPaths.insert(pathOfImageKey(key));
        if (!m_artworkRefresh.isActive()) {
            m_artworkRefresh.start();
        }
    });

    connect(m_posterCache.data(), &PosterCache::posterSaved,
            this, [this](const QString &key, const QString &) {
        settleSetupImage(key, true);
    });
    connect(m_posterCache.data(), &PosterCache::posterFailed,
            this, [this](const QString &key) {
        settleSetupImage(key, false);
    });

    connect(m_posterCache.data(), &PosterCache::posterFailed,
            this, [this](const QString &key) {
        if (m_artworkServer.isEmpty() || m_notOnArtworkServer.contains(key)) {
            return;
        }
        m_notOnArtworkServer.insert(key);
        m_fromServer.remove(key);
        ++m_missedOnServer;
        m_prefetchedKeys.remove(key);
        m_landedPaths.insert(pathOfImageKey(key));
        if (!m_artworkRefresh.isActive()) {
            m_artworkRefresh.start();
        }
    });

    m_artworkRefresh.setSingleShot(true);
    m_artworkRefresh.setInterval(250);
    connect(&m_artworkRefresh, &QTimer::timeout, this, [this]() {
        ++m_artworkRevision;
        const QStringList landed(m_landedPaths.cbegin(), m_landedPaths.cend());
        m_landedPaths.clear();
        if (m_artworkServer.isEmpty()) {
            MM_LOG_D() << "downloaded artwork landed for" << landed.size()
                       << "images, revision" << m_artworkRevision;
        } else {
            MM_LOG_D() << "artwork landed for" << landed.size() << "images -"
                       << m_landedFromServer << "from the PC,"
                       << m_landedFromTmdb << "from TMDB,"
                       << m_missedOnServer << "the PC did not have - revision"
                       << m_artworkRevision;
        }
        m_landedFromServer = 0;
        m_landedFromTmdb = 0;
        m_missedOnServer = 0;
        emit artworkChanged();
        emit artworkLanded(landed, false);
    });

    m_prefetchTimer.setInterval(400);
    connect(&m_prefetchTimer, &QTimer::timeout, this, &MetadataService::drainPrefetch);

    m_prefetchStart.setSingleShot(true);
    m_prefetchStart.setInterval(5000);
    connect(&m_prefetchStart, &QTimer::timeout, this,
            &MetadataService::prefetchArtwork);
    m_prefetchStart.start();

    m_collectionCheckStart.setSingleShot(true);
    m_collectionCheckStart.setInterval(8000);
    connect(&m_collectionCheckStart, &QTimer::timeout, this,
            &MetadataService::checkCollections);
    m_collectionCheckStart.start();

    m_missedDrain.setInterval(200);
    connect(&m_missedDrain, &QTimer::timeout, this,
            &MetadataService::drainMissedArtwork);

    m_creditsSweep.setSingleShot(true);
    m_creditsSweep.setInterval(12000);
    connect(&m_creditsSweep, &QTimer::timeout, this,
            &MetadataService::sweepCredits);
    m_creditsSweep.start();

    m_collectionsChangedSoon.setSingleShot(true);
    m_collectionsChangedSoon.setInterval(300);
    connect(&m_collectionsChangedSoon, &QTimer::timeout, this,
            &MetadataService::collectionsChanged);

    m_pendingFlush.setSingleShot(true);
    m_pendingFlush.setInterval(0);
    connect(&m_pendingFlush, &QTimer::timeout, this, &MetadataService::flushPending);

    m_changeFlush.setSingleShot(true);
    m_changeFlush.setInterval(0);
    connect(&m_changeFlush, &QTimer::timeout, this, &MetadataService::flushChanges);

    m_matchDrain.setSingleShot(true);
    m_matchDrain.setInterval(0);
    connect(&m_matchDrain, &QTimer::timeout, this, &MetadataService::drainMatches);

    m_setupReadTimer.setSingleShot(true);
    m_setupReadTimer.setInterval(0);
    connect(&m_setupReadTimer, &QTimer::timeout, this, &MetadataService::readSetupSlice);

    m_matchedCount = m_mediaRepository.matchedCount();
    m_suggestedCount = m_mediaRepository.suggestedCount();

    connect(this, &MetadataService::pendingChanged, this, [this]() {
        if (pending() == 0) {
            m_prefetchStart.start();
            startCreditsSweep();
        }
    });

    MM_LOG_I() << "metadata service ready, tmdb key"
               << (m_config->hasKey() ? "present" : "MISSING");

    if (!QSslSocket::supportsSsl()) {
        MM_LOG_E() << "no TLS backend, so every TMDB request will fail."
                   << "Built against" << QSslSocket::sslLibraryBuildVersionString()
                   << "but found" << QSslSocket::sslLibraryVersionString();
    } else {
        MM_LOG_I() << "TLS backend:" << QSslSocket::sslLibraryVersionString();
    }
}

MetadataService::~MetadataService()
{
    if (m_client) {
        m_client->cancelAllRequests();
    }
}

int MetadataService::artworkRevision() const
{
    return m_artworkRevision;
}

bool MetadataService::available() const
{
    return m_config && m_config->hasKey();
}

int MetadataService::pending() const
{
    return m_pending + int(m_matchQueue.size());
}

void MetadataService::cancelAll()
{
    m_matchDrain.stop();
    m_matchQueue.clear();
    m_matching.clear();
    m_setupMatching.clear();
    m_setupTitleFiles.clear();
    m_setupTitleQueue.clear();

    if (!m_client) {
        announcePending();
        return;
    }
    m_client->cancelAllRequests();
    m_pending = 0;
    announcePending();
}

void MetadataService::finish(const QVariantMap &result)
{
    if (m_pending > 0) {
        --m_pending;
    }
    const qint64 fileId = result.value(QStringLiteral("fileId")).toLongLong();
    m_matching.remove(fileId);
    m_setupTitleFiles.remove(fileId);
    if (m_setupMatchRunning && m_setupMatching.remove(fileId)) {
        if (result.value(QStringLiteral("unreachable")).toBool()) {
            ++m_setupUnreachable;
        }
        emit setupMatchProgress(m_setupMatchTotal - int(m_setupMatching.size()),
                                m_setupMatchTotal);
    }
    announcePending();
    emit previewReady(result);
}

void MetadataService::setHeldForSetup(bool held)
{
    if (m_heldForSetup == held) {
        return;
    }
    m_heldForSetup = held;

    if (held) {
        MM_LOG_I() << "setup holds matching and library changes until it gets to them";
        return;
    }

    MM_LOG_I() << "setup releases the library changes it held";
    flushChanges();
    m_collectionCheckStart.start();

    if (m_matchAskedWhileHeld) {
        m_matchAskedWhileHeld = false;
        matchUnmatched();
    }
}

void MetadataService::setUniverses(const QList<UniverseCatalog::Universe> &universes)
{
    m_universes = universes;
}

void MetadataService::readForSetup()
{
    m_setupToRead = m_fileRepository.matchCandidates(FileRepository::MatchScope::Unasked);
    m_setupReadIndex = 0;
    m_setupParsed.clear();
    m_setupTitles.clear();

    MM_LOG_I() << "setup reads the names of" << m_setupToRead.size()
               << "files TMDB has not been asked about";
    emit setupReadProgress(0, int(m_setupToRead.size()));
    m_setupReadTimer.start();
}

void MetadataService::readSetupSlice()
{
    constexpr int kSlice = 50;
    const int total = int(m_setupToRead.size());

    for (int i = 0; i < kSlice && m_setupReadIndex < total; ++i, ++m_setupReadIndex) {
        const LibraryFile &file = m_setupToRead.at(m_setupReadIndex);
        const ParsedFileName parsed = file.parentHandle.isEmpty()
            ? FileNameParser::parse(file.displayName)
            : FileNameParser::parsePath(file.parentHandle + QLatin1Char('/')
                                        + file.displayName);
        m_setupParsed.insert(file.id, parsed);
        if (!parsed.title.isEmpty()) {
            m_setupTitles.insert(TmdbAsk::searchKey(parsed, true));
        }
    }

    emit setupReadProgress(m_setupReadIndex, total);

    if (m_setupReadIndex < total) {
        m_setupReadTimer.start();
        return;
    }

    MM_LOG_I() << "setup read" << total << "names into"
               << m_setupTitles.size() << "titles";
    emit setupReadFinished(total, int(m_setupTitles.size()));
}

void MetadataService::matchForSetup(bool oneTitleAtATime)
{
    const QList<LibraryFile> files = m_setupToRead;
    m_setupToRead.clear();
    m_setupReadIndex = 0;

    m_setupMatching.clear();
    QList<LibraryFile> own;
    own.reserve(files.size());
    for (const LibraryFile &file : files) {
        if (!m_matching.contains(file.id)) {
            m_setupMatching.insert(file.id);
            own.append(file);
        }
    }
    m_setupMatchTotal = int(m_setupMatching.size());
    m_setupUnreachable = 0;
    m_setupOneTitleAtATime = oneTitleAtATime;
    m_setupTitleQueue.clear();
    m_setupTitleFiles.clear();
    m_setupTitlesStarted = 0;
    m_setupTitlesTotal = 0;
    m_setupMatchRunning = true;

    emit setupMatchProgress(0, m_setupMatchTotal);

    if (!available() || m_setupMatchTotal == 0) {
        m_setupMatching.clear();
        QTimer::singleShot(0, this, &MetadataService::checkSetupMatchDone);
        return;
    }

    if (!oneTitleAtATime) {
        queueMatches(own, "files for setup", true);
        return;
    }

    QHash<QString, int> titleIndex;
    for (const LibraryFile &file : std::as_const(own)) {
        const auto parsed = m_setupParsed.constFind(file.id);
        const QString key = parsed == m_setupParsed.constEnd() || parsed->title.isEmpty()
            ? QStringLiteral("untitled")
            : TmdbAsk::searchKey(parsed.value(), true);

        const auto found = titleIndex.constFind(key);
        if (found == titleIndex.constEnd()) {
            titleIndex.insert(key, int(m_setupTitleQueue.size()));
            m_setupTitleQueue.append({file});
        } else {
            m_setupTitleQueue[found.value()].append(file);
        }
    }

    m_setupTitlesTotal = int(m_setupTitleQueue.size());
    if (m_client) {
        m_client->setConcurrentRequestLimit(1);
    }
    MM_LOG_I() << "setup matches" << m_setupMatchTotal << "files one title at a time,"
               << m_setupTitlesTotal << "titles";

    startNextSetupTitle();
}

void MetadataService::startNextSetupTitle()
{
    while (!m_setupTitleQueue.isEmpty()) {
        const QList<LibraryFile> title = m_setupTitleQueue.takeFirst();
        ++m_setupTitlesStarted;

        m_setupTitleFiles.clear();
        for (const LibraryFile &file : title) {
            if (m_setupMatching.contains(file.id)) {
                m_setupTitleFiles.insert(file.id);
            }
        }
        if (m_setupTitleFiles.isEmpty()) {
            continue;
        }

        MM_LOG_D() << "setup title" << m_setupTitlesStarted << "of" << m_setupTitlesTotal
                   << "-" << title.first().displayName << "and"
                   << (title.size() - 1) << "more files";
        queueMatches(title, "files of one title for setup", true);
        return;
    }

    QTimer::singleShot(0, this, &MetadataService::checkSetupMatchDone);
}

void MetadataService::checkSetupMatchDone()
{
    if (!m_setupMatchRunning || pending() > 0) {
        return;
    }

    if (m_setupOneTitleAtATime) {
        if (!m_setupTitleFiles.isEmpty()) {
            return;
        }
        if (!m_setupTitleQueue.isEmpty()) {
            startNextSetupTitle();
            return;
        }
    }

    if (!m_setupMatching.isEmpty()) {
        return;
    }

    if (m_setupOneTitleAtATime && m_client) {
        m_client->setConcurrentRequestLimit(TmdbClient::MaximumConcurrentRequests);
    }
    m_setupOneTitleAtATime = false;
    m_setupMatchRunning = false;
    m_setupParsed.clear();
    m_setupTitles.clear();
    MM_LOG_I() << "setup matching done for" << m_setupMatchTotal << "files,"
               << m_setupUnreachable << "could not reach TMDB";
    emit setupMatchFinished(m_setupUnreachable);
}

void MetadataService::downloadArtworkForSetup(bool television)
{
    m_setupImages.clear();
    m_setupImagesInFlight.clear();
    m_setupImagesTotal = 0;
    m_setupImagesSettled = 0;
    m_setupImagesFailed = 0;
    m_setupImagesTelevision = television;
    m_setupImagesParallel = television ? 1 : 6;
    m_setupImagesRunning = true;

    if (!available() || !m_posterCache || !m_posterCache->isUsable()) {
        QTimer::singleShot(0, this, &MetadataService::finishSetupImages);
        return;
    }

    if (!m_posters || !m_posters->isReady()) {
        MM_LOG_I() << "setup waits for TMDB's image configuration before downloading artwork";
        m_setupImagesWait.start();
        return;
    }

    startSetupImages();
}

void MetadataService::startSetupImages()
{
    if (!m_setupImagesRunning) {
        return;
    }

    QElapsedTimer clock;
    clock.start();

    const QList<int> posterWidths = m_setupImagesTelevision
        ? QList<int>{342}
        : QList<int>{185, 342, 500};
    const QList<int> backdropWidths{780, 1280};
    const QList<int> stillWidths{300};

    const ArtworkPaths paths = m_mediaRepository.artworkPaths();
    QSet<QString> seen;
    int cached = 0;

    const auto add = [&](ImageKind kind, const QStringList &list, const QList<int> &widths) {
        for (const QString &path : list) {
            for (const int requested : widths) {
                const int width = TmdbImageUrl::widthFor(kind, requested);
                const QString key = imageKey(kind, path, width);
                if (seen.contains(key)) {
                    continue;
                }
                seen.insert(key);
                if (m_posterCache->isCached(key)) {
                    ++cached;
                    continue;
                }
                m_setupImages.append({kind, path, width, key});
            }
        }
    };

    add(ImageKind::Poster, paths.posters, posterWidths);
    add(ImageKind::Poster, paths.collectionPosters, QList<int>{342});
    const QHash<qint64, qint64> ownedFilms = m_mediaRepository.collectionsByOwnedFilm();
    const QStringList universePosters = UniverseCatalog::ownedPosters(m_universes, ownedFilms);
    const QStringList universeBackdrops = UniverseCatalog::ownedBackdrops(m_universes, ownedFilms);
    add(ImageKind::Poster, universePosters, QList<int>{342});
    add(ImageKind::Backdrop, universeBackdrops, backdropWidths);
    MM_LOG_D() << "setup artwork includes" << paths.collectionPosters.size()
               << "collection posters," << universePosters.size() << "universe posters and"
               << universeBackdrops.size() << "universe backdrops";
    if (!m_setupImagesTelevision) {
        add(ImageKind::Poster, paths.seasonPosters, QList<int>{342, 500});
    }
    add(ImageKind::Backdrop, paths.backdrops, backdropWidths);
    add(ImageKind::Backdrop, paths.seasonBackdrops, backdropWidths);
    add(ImageKind::Still, paths.stills, stillWidths);

    m_setupImagesTotal = int(m_setupImages.size());
    MM_LOG_I() << "setup downloads" << m_setupImagesTotal << "images,"
               << cached << "already on disk, listed in" << clock.elapsed() << "ms,"
               << m_setupImagesParallel << "at a time";
    emit setupArtworkProgress(0, m_setupImagesTotal);

    pumpSetupImages();
}

void MetadataService::pumpSetupImages()
{
    if (!m_setupImagesRunning || m_setupImagesPumping) {
        return;
    }
    m_setupImagesPumping = true;

    while (m_setupImagesInFlight.size() < m_setupImagesParallel && !m_setupImages.isEmpty()) {
        const SetupImage image = m_setupImages.takeFirst();

        const QString remote = m_posters && m_posters->isReady()
            ? m_posters->resolveUrl(image.kind, image.path, image.width)
            : QString();
        if (remote.isEmpty()) {
            ++m_setupImagesSettled;
            ++m_setupImagesFailed;
            continue;
        }

        m_setupImagesInFlight.insert(image.key);
        m_prefetchedKeys.insert(image.key);
        if (!m_posterCache->fetch(remote, image.key) && m_posterCache->isCached(image.key)) {
            m_setupImagesInFlight.remove(image.key);
            m_prefetchedKeys.remove(image.key);
            ++m_setupImagesSettled;
        }
    }

    m_setupImagesPumping = false;
    emit setupArtworkProgress(m_setupImagesSettled, m_setupImagesTotal);

    if (m_setupImages.isEmpty() && m_setupImagesInFlight.isEmpty()) {
        finishSetupImages();
    }
}

void MetadataService::settleSetupImage(const QString &key, bool saved)
{
    if (!m_setupImagesRunning || !m_setupImagesInFlight.remove(key)) {
        return;
    }

    ++m_setupImagesSettled;
    if (!saved) {
        ++m_setupImagesFailed;
        m_prefetchedKeys.remove(key);
    }

    if (m_setupImagesPumping) {
        return;
    }
    pumpSetupImages();
}

void MetadataService::finishSetupImages()
{
    if (!m_setupImagesRunning) {
        return;
    }

    m_setupImagesRunning = false;
    m_setupImagesWait.stop();

    const int skipped = int(m_setupImages.size());
    m_setupImagesFailed += skipped;
    m_setupImagesSettled += skipped;
    m_setupImages.clear();

    MM_LOG_I() << "setup artwork done:" << m_setupImagesSettled << "of"
               << m_setupImagesTotal << "images," << m_setupImagesFailed << "failed";
    emit setupArtworkFinished(m_setupImagesFailed);
}

void MetadataService::announcePending()
{
    if (!m_pendingFlush.isActive()) {
        m_pendingFlush.start();
    }
}

void MetadataService::flushPending()
{
    checkSetupMatchDone();

    const int now = pending();
    if (now == m_announcedPending) {
        return;
    }
    m_announcedPending = now;
    emit pendingChanged();
}

void MetadataService::noteRelink(const LibraryFile &file,
                                 const FileMediaLink &before,
                                 qint64 mediaAfter,
                                 bool suggestedAfter)
{
    if (before.isValid()) {
        if (before.suggested) {
            --m_suggestedCount;
        } else {
            --m_matchedCount;
        }
        m_changedMedia.insert(before.mediaId);
    }

    if (mediaAfter > 0) {
        if (suggestedAfter) {
            ++m_suggestedCount;
        } else {
            ++m_matchedCount;
        }
        m_changedMedia.insert(mediaAfter);
    }

    if (!file.missing) {
        if (!before.isValid() && mediaAfter > 0) {
            ++m_filesMatched;
        } else if (before.isValid() && mediaAfter <= 0) {
            ++m_filesUnmatched;
        }
    }

    m_changedFiles.insert(file.id);
    if (!file.handle.isEmpty()) {
        m_changedHandles.insert(file.handle);
    }
    if (!m_changeFlush.isActive()) {
        m_changeFlush.start();
    }
}

namespace {

QVariantList castOf(const QList<CreditRecord> &credits)
{
    QVariantList people;
    for (const CreditRecord &credit : credits) {
        if (credit.kind != CreditRecord::Cast) {
            continue;
        }

        QVariantMap person;
        person.insert(QStringLiteral("name"), credit.name);
        person.insert(QStringLiteral("role"), credit.role);
        person.insert(QStringLiteral("profilePath"), credit.profilePath);
        people.append(person);
    }
    return people;
}

QString namesOf(const QList<CreditRecord> &credits, int kind)
{
    QStringList names;
    for (const CreditRecord &credit : credits) {
        if (credit.kind == kind && !names.contains(credit.name)) {
            names.append(credit.name);
        }
    }
    return names.join(QStringLiteral(", "));
}

void appendCredits(QList<CreditRecord> &into,
                   const std::vector<TmdbCreditDto> &people,
                   int kind)
{
    for (const TmdbCreditDto &person : people) {
        CreditRecord record;
        record.kind = kind;
        record.name = QString::fromStdString(person.name);
        record.role = QString::fromStdString(person.role);
        record.profilePath = person.profilePath.has_value()
            ? QString::fromStdString(*person.profilePath) : QString();
        into.append(record);
    }
}

}

void MetadataService::storeTitleCredits(qint64 mediaId,
                                        const TmdbTitleDetailsDto &details)
{
    QList<CreditRecord> credits;
    appendCredits(credits, details.cast, CreditRecord::Cast);
    appendCredits(credits, details.directors, CreditRecord::Director);
    appendCredits(credits, details.writers, CreditRecord::Writer);
    appendCredits(credits, details.creators, CreditRecord::Creator);

    if (credits.isEmpty()) {
        return;
    }

    m_mediaRepository.replaceCredits(mediaId, 0, credits);
    MM_LOG_D() << "media" << mediaId << "is credited to"
               << details.cast.size() << "in front of the camera and"
               << (details.directors.size() + details.writers.size()
                   + details.creators.size())
               << "behind it";
}

void MetadataService::storeEpisodeCredits(qint64 mediaId, qint64 episodeId,
                                          const TmdbEpisodeDto &episode)
{
    QList<CreditRecord> credits;
    appendCredits(credits, episode.directors, CreditRecord::Director);
    appendCredits(credits, episode.writers, CreditRecord::Writer);

    if (credits.isEmpty()) {
        return;
    }

    m_mediaRepository.replaceCredits(mediaId, episodeId, credits);
}

void MetadataService::noteTitleChanged(qint64 mediaId)
{
    if (mediaId <= 0) {
        return;
    }

    m_changedMedia.insert(mediaId);
    const QList<qint64> files = m_mediaRepository.fileIdsFor(mediaId);
    for (const qint64 fileId : files) {
        m_changedFiles.insert(fileId);
    }

    if (!m_changeFlush.isActive()) {
        m_changeFlush.start();
    }
}

void MetadataService::flushChanges()
{
    if (m_heldForSetup) {
        return;
    }

    LibraryChange change;
    change.fileIds = m_changedFiles.values();
    change.mediaIds = m_changedMedia.values();
    change.filesMatched = m_filesMatched;
    change.filesUnmatched = m_filesUnmatched;

    const QStringList handles(m_changedHandles.cbegin(), m_changedHandles.cend());

    m_changedFiles.clear();
    m_changedMedia.clear();
    m_changedHandles.clear();
    m_filesMatched = 0;
    m_filesUnmatched = 0;

    if (change.isEmpty()) {
        return;
    }

    for (const qint64 mediaId : std::as_const(change.mediaIds)) {
        m_warmTitles.insert(mediaId);
    }

    MM_LOG_D() << "library change:" << change.fileIds.size() << "files,"
               << change.mediaIds.size() << "titles," << change.filesMatched
               << "newly matched," << change.filesUnmatched << "unmatched";
    emit libraryChanged(change);
    emit matchesChanged();

    QVariantList mediaIds;
    mediaIds.reserve(change.mediaIds.size());
    for (const qint64 mediaId : std::as_const(change.mediaIds)) {
        mediaIds.append(mediaId);
    }
    emit matchesChangedFor(handles, mediaIds);
}

int MetadataService::matchedCount() const
{
    return m_matchedCount;
}

int MetadataService::suggestedCount() const
{
    return m_suggestedCount;
}

QVariantMap MetadataService::metadataForFile(const QString &fileHandle) const
{
    QVariantMap result;

    const LibraryFile file = m_fileRepository.byHandle(fileHandle);
    if (!file.isValid()) {
        return result;
    }

    const MediaRecord media = m_mediaRepository.mediaForFile(file.id);
    if (!media.isValid()) {
        return result;
    }

    const FileMediaLink link = m_mediaRepository.linkForFile(file.id);

    result.insert(QStringLiteral("mediaId"), media.id);
    result.insert(QStringLiteral("tmdbId"), media.tmdbId);
    result.insert(QStringLiteral("kind"), media.kind);
    result.insert(QStringLiteral("title"), media.title);
    result.insert(QStringLiteral("year"), media.year);
    result.insert(QStringLiteral("overview"), media.overview);
    result.insert(QStringLiteral("rating"), media.rating);
    result.insert(QStringLiteral("genres"), media.genres);
    result.insert(QStringLiteral("certification"), media.certification);
    result.insert(QStringLiteral("runtimeMinutes"), media.runtimeMinutes);
    result.insert(QStringLiteral("posterPath"), media.posterPath);
    result.insert(QStringLiteral("backdropPath"), media.backdropPath);
    result.insert(QStringLiteral("confidence"), link.confidence);
    result.insert(QStringLiteral("suggested"), link.suggested);
    result.insert(QStringLiteral("pinned"),
                  m_mediaRepository.overrideFor(fileHandle).isValid());

    const EpisodeRecord episode = m_mediaRepository.episodeForFile(file.id);
    const bool isEpisode = episode.episode > 0;
    result.insert(QStringLiteral("isEpisode"), isEpisode);
    if (isEpisode) {
        result.insert(QStringLiteral("season"), episode.season);
        result.insert(QStringLiteral("episode"), episode.episode);
        result.insert(QStringLiteral("episodeTitle"), episode.title);
        result.insert(QStringLiteral("episodeOverview"), episode.overview);
        result.insert(QStringLiteral("episodeStillPath"), episode.stillPath);
        result.insert(QStringLiteral("episodeAirDate"), episode.airDate);
        result.insert(QStringLiteral("episodeRuntimeMinutes"),
                      episode.runtimeMinutes);
        result.insert(QStringLiteral("seasonPosterPath"),
                      m_mediaRepository.seasonPosters(media.id)
                          .value(episode.season));
    }

    const QList<CreditRecord> titleCredits =
        m_mediaRepository.creditsFor(media.id);
    result.insert(QStringLiteral("cast"), castOf(titleCredits));
    result.insert(QStringLiteral("creators"),
                  namesOf(titleCredits, CreditRecord::Creator));

    QList<CreditRecord> crew = titleCredits;
    if (isEpisode) {
        const qint64 episodeId = m_mediaRepository.episodeRowId(
            media.id, episode.season, episode.episode);
        const QList<CreditRecord> own =
            episodeId > 0 ? m_mediaRepository.creditsFor(media.id, episodeId)
                          : QList<CreditRecord>();
        if (!own.isEmpty()) {
            crew = own;
        }
    }

    result.insert(QStringLiteral("directors"),
                  namesOf(crew, CreditRecord::Director));
    result.insert(QStringLiteral("writers"),
                  namesOf(crew, CreditRecord::Writer));

    return result;
}

QVariantMap MetadataService::nextEpisodeFor(const QString &fileHandle) const
{
    QVariantMap result;

    const LibraryFile file = m_fileRepository.byHandle(fileHandle);
    if (!file.isValid()) {
        return result;
    }

    const EpisodeRecord current = m_mediaRepository.episodeForFile(file.id);
    if (current.season <= 0 || current.episode <= 0) {
        return result;
    }

    const EpisodeRecord next = m_mediaRepository.nextEpisodeWithFile(
        current.mediaId, current.season, current.episode);
    if (!next.hasFile()) {
        return result;
    }

    result.insert(QStringLiteral("handle"), next.fileHandle);
    result.insert(QStringLiteral("fileName"), next.fileName);
    result.insert(QStringLiteral("season"), next.season);
    result.insert(QStringLiteral("episode"), next.episode);
    result.insert(QStringLiteral("title"), next.title);
    result.insert(QStringLiteral("overview"), next.overview);
    result.insert(QStringLiteral("stillPath"), next.stillPath);
    result.insert(QStringLiteral("airDate"), next.airDate);
    result.insert(QStringLiteral("runtimeMinutes"), next.runtimeMinutes);
    result.insert(QStringLiteral("watched"), next.watched);
    result.insert(QStringLiteral("positionSeconds"), next.positionSeconds);

    return result;
}

QString MetadataService::imageKey(ImageKind kind, const QString &path, int width)
{
    switch (kind) {
    case ImageKind::Still:
        return QStringLiteral("still:%1@%2").arg(path).arg(width);
    case ImageKind::Backdrop:
        return QStringLiteral("backdrop:%1@%2").arg(path).arg(width);
    case ImageKind::Profile:
        return QStringLiteral("profile:%1@%2").arg(path).arg(width);
    case ImageKind::Poster:
    default:
        return QStringLiteral("%1@%2").arg(path).arg(width);
    }
}

QString MetadataService::imageUrl(ImageKind kind,
                                  const QString &path,
                                  int pixelWidth,
                                  ImageUse use) const
{
    if (path.isEmpty()) {
        return QString();
    }

    const int width = TmdbImageUrl::widthFor(kind, pixelWidth);
    const QString key = imageKey(kind, path, width);

    QString shown;
    if (m_posterCache) {
        shown = m_posterCache->localUrl(key);
        if (!shown.isEmpty()) {
            return shown;
        }

        const std::vector<int> widths = TmdbImageUrl::widths(kind);
        for (auto other = widths.crbegin();
             other != widths.crend() && shown.isEmpty(); ++other) {
            if (*other != width) {
                shown = m_posterCache->localUrl(imageKey(kind, path, *other));
            }
        }
    }

    if (!m_artworkServer.isEmpty() && m_posterCache && m_posterCache->isUsable()
        && !m_notOnArtworkServer.contains(key)) {
        const QString fromServer = m_artworkServer + QStringLiteral("/v1/art?k=")
                                   + QString::fromLatin1(QUrl::toPercentEncoding(key));
        m_fromServer.insert(key);
        if (use == ImageUse::Warmed) {
            if (m_posterCache->fetch(fromServer, key)) {
                m_prefetchedKeys.insert(key);
            }
            return shown;
        }
        m_prefetchedKeys.remove(key);
        m_posterCache->fetch(fromServer, key);
        return shown;
    }

    if (!m_posters) {
        return shown;
    }

    const QString remote = m_posters->resolveUrl(kind, path, width);
    if (remote.isEmpty()) {
        return shown;
    }
    if (!m_posterCache || !m_posterCache->isUsable()) {
        return remote;
    }

    m_fromServer.remove(key);

    if (use == ImageUse::Warmed) {
        if (m_posterCache->fetch(remote, key)) {
            m_prefetchedKeys.insert(key);
        }
        return shown;
    }

    m_prefetchedKeys.remove(key);
    m_posterCache->fetch(remote, key);
    return shown;
}

QString MetadataService::posterUrl(const QString &posterPath, int pixelWidth) const
{
    return imageUrl(ImageKind::Poster, posterPath, pixelWidth, ImageUse::Shown);
}

QString MetadataService::stillUrl(const QString &stillPath, int pixelWidth) const
{
    return imageUrl(ImageKind::Still, stillPath, pixelWidth, ImageUse::Shown);
}

QString MetadataService::backdropUrl(const QString &backdropPath, int pixelWidth) const
{
    return imageUrl(ImageKind::Backdrop, backdropPath, pixelWidth, ImageUse::Shown);
}

QString MetadataService::profileUrl(const QString &profilePath, int pixelWidth) const
{
    return imageUrl(ImageKind::Profile, profilePath, pixelWidth, ImageUse::Shown);
}

void MetadataService::prefetchArtwork()
{
    const bool fromTmdb = available() && m_posters && m_posters->isReady();
    if (!m_posterCache || (!fromTmdb && m_artworkServer.isEmpty())) {
        return;
    }
    if (!m_prefetchQueue.isEmpty() || m_prefetchTimer.isActive()) {
        return;
    }

    if (m_warmAgain) {
        m_warmAgain = false;
        m_warmedAll = false;
    }

    ArtworkPaths paths;
    if (!m_warmedAll) {
        paths = m_mediaRepository.artworkPaths();
        m_warmedAll = true;
        m_warmTitles.clear();
    } else {
        if (m_warmTitles.isEmpty()) {
            return;
        }
        paths = m_mediaRepository.artworkPathsFor(m_warmTitles.values());
        m_warmTitles.clear();
    }

    for (const QString &path : paths.backdrops) {
        m_prefetchQueue.append({PrefetchItem::Backdrop, path});
    }
    for (const QString &path : paths.posters) {
        m_prefetchQueue.append({PrefetchItem::Poster, path});
    }
    for (const QString &path : paths.stills) {
        m_prefetchQueue.append({PrefetchItem::Still, path});
    }
    int faces = 0;
    if (m_artworkServer.isEmpty()) {
        for (const QString &path : paths.profiles) {
            m_prefetchQueue.append({PrefetchItem::Profile, path});
            ++faces;
        }
    }

    if (m_prefetchQueue.isEmpty()) {
        return;
    }

    MM_LOG_I() << "warming artwork for" << m_prefetchQueue.size()
               << "images in the background," << faces << "of them faces";
    m_warmDownloads = 0;
    noteBackgroundWork(m_creditsLeft, m_prefetchQueue.size());
    m_prefetchTimer.start();
}

void MetadataService::drainPrefetch()
{
    if (m_prefetchQueue.isEmpty()) {
        m_prefetchTimer.stop();
        MM_LOG_I() << "artwork warming finished," << m_warmDownloads
                   << "of them actually downloaded";
        noteBackgroundWork(m_creditsLeft, 0);
        if (m_warmAgain) {
            m_prefetchStart.start();
        }
        return;
    }

    int fetched = 0;
    int checked = 0;
    while (fetched < 2 && checked < 200 && !m_prefetchQueue.isEmpty()) {
        const PrefetchItem item = m_prefetchQueue.takeFirst();
        ++checked;

        ImageKind kind = ImageKind::Poster;
        int width = 500;
        switch (item.kind) {
        case PrefetchItem::Backdrop:
            kind = ImageKind::Backdrop;
            width = 780;
            break;
        case PrefetchItem::Still:
            kind = ImageKind::Still;
            width = 300;
            break;
        case PrefetchItem::Profile:
            kind = ImageKind::Profile;
            width = 185;
            break;
        case PrefetchItem::Poster:
        default:
            break;
        }

        if (m_posterCache
            && !m_posterCache->localUrl(imageKey(kind, item.path, width)).isEmpty()) {
            continue;
        }

        imageUrl(kind, item.path, width, ImageUse::Warmed);
        ++fetched;
        ++m_warmDownloads;
    }

    noteBackgroundWork(m_creditsLeft, m_prefetchQueue.size());
}

void MetadataService::clearPosterCache()
{
    if (m_posterCache) {
        m_posterCache->clear();
        m_prefetchQueue.clear();
        m_prefetchedKeys.clear();
        m_prefetchTimer.stop();
        noteBackgroundWork(m_creditsLeft, 0);
        m_warmedAll = false;
        m_prefetchStart.start();
        ++m_artworkRevision;
        emit artworkChanged();
        emit artworkLanded(QStringList(), true);
    }
}

QString MetadataService::pathOfImageKey(const QString &key)
{
    QString path = key;
    if (path.startsWith(QLatin1String("still:"))) {
        path.remove(0, 6);
    } else if (path.startsWith(QLatin1String("backdrop:"))) {
        path.remove(0, 9);
    } else if (path.startsWith(QLatin1String("profile:"))) {
        path.remove(0, 8);
    }

    const qsizetype at = path.lastIndexOf(QLatin1Char('@'));
    return at >= 0 ? path.left(at) : path;
}

void MetadataService::warmArtworkKey(const QString &key)
{
    if (!m_posterCache || !m_posterCache->isUsable() || !m_posters
        || !m_posters->isReady() || m_posterCache->isCached(key)) {
        return;
    }

    if (m_missedArtwork.size() >= kMissedArtworkCap
        || m_missedArtwork.contains(key)) {
        return;
    }

    m_missedArtwork.enqueue(key);
    if (!m_missedDrain.isActive()) {
        m_missedDrain.start();
    }
}

void MetadataService::drainMissedArtwork()
{
    if (m_missedArtwork.isEmpty()) {
        m_missedDrain.stop();
        return;
    }

    for (int i = 0; i < 2 && !m_missedArtwork.isEmpty(); ++i) {
        fetchArtworkKey(m_missedArtwork.dequeue());
    }
}

void MetadataService::fetchArtworkKey(const QString &key)
{
    if (!m_posterCache || !m_posters || m_posterCache->isCached(key)) {
        return;
    }

    ImageKind kind = ImageKind::Poster;
    QString rest = key;
    if (rest.startsWith(QLatin1String("still:"))) {
        kind = ImageKind::Still;
        rest.remove(0, 6);
    } else if (rest.startsWith(QLatin1String("backdrop:"))) {
        kind = ImageKind::Backdrop;
        rest.remove(0, 9);
    } else if (rest.startsWith(QLatin1String("profile:"))) {
        kind = ImageKind::Profile;
        rest.remove(0, 8);
    }

    const qsizetype at = rest.lastIndexOf(QLatin1Char('@'));
    if (at <= 0) {
        return;
    }
    const QString path = rest.left(at);
    const int width = rest.mid(at + 1).toInt();
    if (path.isEmpty() || width <= 0) {
        return;
    }

    const QString remote = m_posters->resolveUrl(kind, path, width);
    if (remote.isEmpty()) {
        return;
    }

    if (m_posterCache->fetch(remote, key)) {
        MM_LOG_D() << "a device wanted" << key
                   << "and this PC did not have it - fetching, so the next"
                   << "device asking is answered from here";
    }
}

void MetadataService::previewMatch(const QString &fileHandle, const QString &fileName)
{
    const LibraryFile file = m_fileRepository.byHandle(fileHandle);
    runMatch(fileHandle, fileName,
             file.isValid() ? file.parentHandle : QString(), false);
}

void MetadataService::matchLibrary()
{
    if (!available()) {
        return;
    }

    queueMatches(m_fileRepository.matchCandidates(FileRepository::MatchScope::Unpinned),
                 "files that are not pinned");
}

void MetadataService::matchUnmatched()
{
    if (!available()) {
        return;
    }

    queueMatches(m_fileRepository.matchCandidates(FileRepository::MatchScope::Unasked),
                 "files TMDB has not been asked about");
}

void MetadataService::matchScanned(const QStringList &fileHandles)
{
    if (!available() || fileHandles.isEmpty()) {
        return;
    }

    queueMatches(m_fileRepository.matchCandidates(FileRepository::MatchScope::Unasked,
                                                  fileHandles),
                 "files the scan wrote");
}

bool MetadataService::refusesWhileStreaming(const char *what) const
{
    if (!m_heldForStreaming) {
        return false;
    }
    MM_LOG_I() << "streaming holds the library still, so" << what << "waits";
    return true;
}

void MetadataService::forgetLibraryState()
{
    cancelAll();
    m_changeFlush.stop();
    m_changedFiles.clear();
    m_changedMedia.clear();
    m_changedHandles.clear();
    m_warmTitles.clear();
    m_prefetchQueue.clear();
    m_prefetchTimer.stop();
    m_warmedAll = false;
    m_notOnArtworkServer.clear();
    MM_LOG_I() << "matching forgot what it knew about the library it leaves";
}

void MetadataService::readLibraryAfterSwitch()
{
    m_matchedCount = m_mediaRepository.matchedCount();
    m_suggestedCount = m_mediaRepository.suggestedCount();
    emit matchesChanged();
    ++m_artworkRevision;
    emit artworkChanged();
    prefetchArtwork();
}

void MetadataService::setArtworkServer(const QString &baseUrl)
{
    if (m_artworkServer == baseUrl) {
        return;
    }
    m_artworkServer = baseUrl;
    m_notOnArtworkServer.clear();
    MM_LOG_I() << "artwork comes"
               << (baseUrl.isEmpty() ? QStringLiteral("from TMDB")
                                     : QStringLiteral("from ") + StreamProtocol::withoutToken(baseUrl))
               << "first";
}

void MetadataService::setHeldForStreaming(bool held)
{
    if (m_heldForStreaming == held) {
        return;
    }
    m_heldForStreaming = held;

    if (held) {
        MM_LOG_I() << "matching holds still while streaming;" << pending()
                   << "lookups under way are dropped and asked again afterwards";
        m_creditsSweep.stop();
        emit backgroundWorkChanged();
        cancelAll();
        return;
    }

    MM_LOG_I() << "matching is free again after streaming";
    matchUnmatched();
    startCreditsSweep();
}

void MetadataService::queueMatches(const QList<LibraryFile> &files, const char *what,
                                   bool forSetup)
{
    if (m_heldForStreaming) {
        if (!files.isEmpty()) {
            MM_LOG_I() << "streaming holds the library still, so" << files.size() << what
                       << "wait for it to stop";
        }
        return;
    }

    if (m_heldForSetup && !forSetup) {
        if (!files.isEmpty()) {
            m_matchAskedWhileHeld = true;
            MM_LOG_I() << "setup is running, so" << files.size() << what
                       << "wait for it";
        }
        return;
    }

    int queued = 0;
    for (const LibraryFile &file : files) {
        if (m_matching.contains(file.id)) {
            continue;
        }
        m_matching.insert(file.id);
        m_matchQueue.enqueue(file);
        ++queued;
    }

    MM_LOG_I() << "matching" << queued << what << "-"
               << (files.size() - queued) << "already under way";

    if (queued == 0) {
        return;
    }

    announcePending();
    if (!m_matchDrain.isActive()) {
        m_matchDrain.start();
    }
}

void MetadataService::drainMatches()
{
    constexpr int kSlice = 20;

    for (int i = 0; i < kSlice && !m_matchQueue.isEmpty(); ++i) {
        const LibraryFile file = m_matchQueue.dequeue();
        const auto parsed = m_setupParsed.constFind(file.id);
        if (parsed != m_setupParsed.constEnd()) {
            runParsedMatch(parsed.value(), file.handle, file.displayName, true, file.id);
        } else {
            runMatch(file.handle, file.displayName, file.parentHandle, true, file.id);
        }
    }

    if (!m_matchQueue.isEmpty()) {
        m_matchDrain.start();
    }
}

void MetadataService::matchUnmatchedAgain()
{
    if (!available()) {
        return;
    }

    MM_LOG_I() << "forgetting previous TMDB answers before asking again";
    m_fileRepository.clearMatchAttempts();
    matchUnmatched();
}

void MetadataService::recordNoMatch(qint64 fileId, const QString &fileHandle,
                                    const QString &why)
{
    if (fileId <= 0) {
        const LibraryFile file = m_fileRepository.byHandle(fileHandle);
        if (!file.isValid()) {
            return;
        }
        fileId = file.id;
    }

    MM_LOG_I() << "no match for" << fileHandle << "-" << why
               << "- it will not be asked about again";
    m_fileRepository.markMatchAttempted(fileId);
}

void MetadataService::searchCandidates(const QString &query, bool tv)
{
    if (!available() || query.trimmed().isEmpty()) {
        emit candidatesReady(QVariantList());
        return;
    }

    TmdbSearchRequestDto request;
    request.mediaType = tv ? TmdbMediaType::Tv : TmdbMediaType::Movie;
    request.query = query.trimmed().toStdString();
    request.page = 1;

    ++m_pending;
    announcePending();

    auto handler = [this, tv](TmdbClient::DiscoverResult result) {
        if (m_pending > 0) {
            --m_pending;
        }
        announcePending();

        QVariantList candidates;
        if (auto *page = std::get_if<TmdbDiscoverPageDto>(&result)) {
            for (const TmdbTitleResultDto &item : page->results) {
                QVariantMap entry;
                entry.insert(QStringLiteral("tmdbId"), qint64(item.id));
                entry.insert(QStringLiteral("kind"),
                             tv ? QStringLiteral("tv") : QStringLiteral("movie"));
                entry.insert(QStringLiteral("title"),
                             QString::fromStdString(item.title));
                entry.insert(QStringLiteral("year"),
                             item.releaseDate.has_value()
                                     && item.releaseDate->size() >= 4
                                 ? QString::fromStdString(
                                       item.releaseDate->substr(0, 4)).toInt()
                                 : 0);
                entry.insert(QStringLiteral("overview"),
                             QString::fromStdString(item.overview));
                entry.insert(QStringLiteral("rating"), item.rating);
                entry.insert(QStringLiteral("posterPath"),
                             item.posterPath.has_value()
                                 ? QString::fromStdString(*item.posterPath)
                                 : QString());
                candidates.append(entry);
            }
        }

        emit candidatesReady(candidates);
    };

    const auto id = tv ? m_client->searchTv(request, handler)
                       : m_client->searchMovie(request, handler);
    Q_UNUSED(id)
}

bool MetadataService::pinMatch(const QString &fileHandle,
                               const QVariantMap &candidate)
{
    if (refusesWhileStreaming("a fix match")) {
        return false;
    }
    const qint64 tmdbId = candidate.value(QStringLiteral("tmdbId")).toLongLong();
    const QString kind = candidate.value(QStringLiteral("kind")).toString();
    if (tmdbId <= 0 || kind.isEmpty()) {
        return false;
    }

    const LibraryFile file = m_fileRepository.byHandle(fileHandle);
    if (!file.isValid()) {
        return false;
    }

    if (applyPin(file, candidate, true).mediaId <= 0) {
        MM_LOG_W() << "pin for" << fileHandle
                   << "did not take, the file is left as it was";
        return false;
    }

    return true;
}

int MetadataService::pinMatchForShow(const QStringList &fileHandles,
                                     const QVariantMap &candidate)
{
    if (refusesWhileStreaming("identifying a show")) {
        return 0;
    }
    const qint64 tmdbId = candidate.value(QStringLiteral("tmdbId")).toLongLong();
    const QString kind = candidate.value(QStringLiteral("kind")).toString();
    if (tmdbId <= 0 || kind.isEmpty() || fileHandles.isEmpty()) {
        return 0;
    }

    QSet<int> seasons;
    qint64 mediaId = 0;
    int pinned = 0;

    const bool grouped = m_database.transaction();

    for (const QString &fileHandle : fileHandles) {
        const LibraryFile file = m_fileRepository.byHandle(fileHandle);
        if (!file.isValid()) {
            continue;
        }

        const int hint = kind == QLatin1String("tv")
            ? ShowGrouping::episodeFromBareName(file.displayName,
                                                parsedFor(file).season)
            : 0;

        const PinResult result = applyPin(file, candidate, false, hint);
        if (result.mediaId <= 0) {
            continue;
        }

        ++pinned;
        mediaId = result.mediaId;
        if (result.season > 0) {
            seasons.insert(result.season);
        }
    }

    if (grouped && !m_database.commit()) {
        m_database.rollback();
        MM_LOG_E() << "pinning" << fileHandles.size()
                   << "files as one show could not be committed";
        return 0;
    }

    if (pinned == 0) {
        MM_LOG_W() << "nothing pinned for"
                   << candidate.value(QStringLiteral("title")).toString();
        return 0;
    }

    if (mediaId > 0 && m_mediaRepository.settleSuggestions(mediaId) > 0) {
        m_suggestedCount = m_mediaRepository.suggestedCount();
        emit matchesChanged();
    }

    fetchDetails(tmdbId, kind);
    if (kind == QLatin1String("tv")) {
        for (const int season : seasons) {
            fetchSeason(tmdbId, mediaId, season);
        }
    }

    MM_LOG_I() << "pinned" << pinned << "of" << fileHandles.size()
               << "files to" << candidate.value(QStringLiteral("title")).toString()
               << "as one show, asking for" << seasons.size() << "seasons";

    return pinned;
}

MetadataService::PinResult MetadataService::applyPin(const LibraryFile &file,
                                                     const QVariantMap &candidate,
                                                     bool fetchArtwork,
                                                     int episodeHint)
{
    PinResult pinned;

    const qint64 tmdbId = candidate.value(QStringLiteral("tmdbId")).toLongLong();
    const QString kind = candidate.value(QStringLiteral("kind")).toString();
    if (tmdbId <= 0 || kind.isEmpty()) {
        return pinned;
    }

    ParsedFileName parsed = parsedFor(file);
    if (parsed.episode <= 0 && episodeHint > 0) {
        parsed.episode = episodeHint;
    }

    PinRecord request;
    request.fileId = file.id;
    request.fileHandle = file.handle;
    request.media.tmdbId = tmdbId;
    request.media.kind = kind;
    request.media.title = candidate.value(QStringLiteral("title")).toString();
    request.media.year = candidate.value(QStringLiteral("year")).toInt();
    request.media.overview = candidate.value(QStringLiteral("overview")).toString();
    request.media.rating = candidate.value(QStringLiteral("rating")).toDouble();
    request.media.posterPath =
        candidate.value(QStringLiteral("posterPath")).toString();
    request.season = parsed.season;
    request.episode = parsed.episode;
    request.episodeTitle = parsed.episodeTitle;

    const FileMediaLink before = m_mediaRepository.linkForFile(file.id);
    const qint64 mediaId = m_mediaRepository.pin(request);
    if (mediaId <= 0) {
        return pinned;
    }
    noteRelink(file, before, mediaId, false);

    pinned.mediaId = mediaId;
    pinned.season = parsed.season;

    if (fetchArtwork) {
        if (kind == QLatin1String("tv") && parsed.looksLikeEpisode()) {
            fetchSeason(tmdbId, mediaId, parsed.season);
        }
        fetchDetails(tmdbId, kind);
    }

    return pinned;
}

void MetadataService::unpinMatch(const QString &fileHandle)
{
    if (refusesWhileStreaming("unpinning a match")) {
        return;
    }
    const LibraryFile file = m_fileRepository.byHandle(fileHandle);

    m_mediaRepository.clearOverride(fileHandle);

    if (file.isValid()) {
        const FileMediaLink before = m_mediaRepository.linkForFile(file.id);
        if (m_mediaRepository.unlinkFile(file.id) && before.isValid()) {
            noteRelink(file, before, 0, false);
        }
    }

    MM_LOG_I() << "match unpinned for" << fileHandle;
}

void MetadataService::clearMatch(const QString &fileHandle)
{
    if (refusesWhileStreaming("clearing a match")) {
        return;
    }
    const LibraryFile file = m_fileRepository.byHandle(fileHandle);
    if (!file.isValid()) {
        MM_LOG_W() << "cannot clear the match on an unindexed file" << fileHandle;
        return;
    }

    const FileMediaLink before = m_mediaRepository.linkForFile(file.id);
    m_mediaRepository.clearOverride(fileHandle);
    if (m_mediaRepository.unlinkFile(file.id) && before.isValid()) {
        noteRelink(file, before, 0, false);
    }

    m_fileRepository.markMatchAttempted(file.id);

    MM_LOG_I() << "match cleared for" << fileHandle
               << "- it stays in the library with no title";
}

void MetadataService::clearAllMatches()
{
    if (refusesWhileStreaming("forgetting every match")) {
        return;
    }
    const QList<LinkedFile> linked = m_mediaRepository.unpinnedLinks();
    const bool grouped = m_database.transaction();

    for (const LinkedFile &entry : linked) {
        if (!m_mediaRepository.unlinkFile(entry.link.fileId)) {
            continue;
        }
        LibraryFile file;
        file.id = entry.link.fileId;
        file.handle = entry.fileHandle;
        noteRelink(file, entry.link, 0, false);
    }
    m_mediaRepository.detachUnlinkedEpisodes();

    m_seasonAsks.forgetAnswers();
    m_detailsAsks.forgetAnswers();
    m_searchAsks.forgetAnswers();
    m_mediaRepository.clearFetchedSeasons();

    m_fileRepository.clearMatchAttempts();

    if (grouped && !m_database.commit()) {
        m_database.rollback();
        MM_LOG_E() << "clearing the automatic matches could not be committed";
        return;
    }

    MM_LOG_I() << "cleared" << linked.size() << "automatic matches, pinned ones kept";
}

void MetadataService::applyResult(qint64 fileId,
                                  const QString &fileHandle,
                                  const ParsedFileName &parsed,
                                  const QVariantMap &result)
{
    const qint64 tmdbId = result.value(QStringLiteral("tmdbId")).toLongLong();
    if (tmdbId <= 0) {
        return;
    }

    const int score = result.value(QStringLiteral("score")).toInt();
    const bool confident = result.value(QStringLiteral("confident")).toBool();
    const bool suggested = result.value(QStringLiteral("suggested")).toBool();

    if (!confident && !suggested) {
        recordNoMatch(fileId, fileHandle,
                      QStringLiteral("nothing scored well enough"));
        return;
    }

    const LibraryFile file = fileId > 0 ? m_fileRepository.plainById(fileId)
                                        : m_fileRepository.byHandle(fileHandle);
    if (!file.isValid()) {
        MM_LOG_W() << "matched a file that is no longer indexed" << fileHandle;
        return;
    }

    MediaRecord record;
    record.tmdbId = tmdbId;
    record.kind = parsed.looksLikeEpisode() ? QStringLiteral("tv")
                                            : QStringLiteral("movie");
    record.title = result.value(QStringLiteral("matchedTitle")).toString();
    record.year = result.value(QStringLiteral("matchedYear")).toInt();
    record.overview = result.value(QStringLiteral("overview")).toString();
    record.rating = result.value(QStringLiteral("rating")).toDouble();
    record.posterPath = result.value(QStringLiteral("posterPath")).toString();

    const qint64 mediaId = m_mediaRepository.upsertMedia(record);
    if (mediaId <= 0) {
        return;
    }

    const FileMediaLink before = m_mediaRepository.linkForFile(file.id);
    if (m_mediaRepository.linkFile(file.id, mediaId, score / 100.0, !confident)) {
        noteRelink(file, before, mediaId, !confident);
    }

    MM_LOG_D() << "matched" << file.displayName << "to" << record.title
               << record.year << "score" << score
               << (confident ? "confident" : "suggested");

    if (confident) {
        fetchDetails(tmdbId, record.kind);
    }

    if (parsed.looksLikeEpisode()) {
        EpisodeRecord episode;
        episode.mediaId = mediaId;
        episode.season = parsed.season;
        episode.episode = parsed.episode;
        episode.title = parsed.episodeTitle;
        m_mediaRepository.upsertEpisode(episode, file.id);

        if (confident) {
            fetchSeason(tmdbId, mediaId, parsed.season);
        }
    }
}

bool MetadataService::fetchDetails(qint64 tmdbId, const QString &kind)
{
    const bool isTv = kind == QLatin1String("tv");

    const QString key = TmdbAsk::detailsKey(tmdbId, kind);
    if (m_detailsAsks.ask(key) != TmdbAsk::Verdict::AskNow) {
        return false;
    }

    ++m_pending;
    announcePending();

    const auto id = m_client->titleDetails(
        isTv ? TmdbMediaType::Tv : TmdbMediaType::Movie,
        TmdbId(tmdbId),
        [this, tmdbId, kind, key](TmdbClient::TitleDetailsResult result) {
            if (m_pending > 0) {
                --m_pending;
            }
            announcePending();

            auto *details = std::get_if<TmdbTitleDetailsDto>(&result);
            if (!details) {
                auto *error = std::get_if<TmdbError>(&result);
                m_detailsAsks.fail(key, error ? error->httpStatus()
                                              : std::optional<int>());
                MM_LOG_W() << "no details for tmdb" << tmdbId
                           << "- the summary from search stands";
                return;
            }
            m_detailsAsks.answer(key);

            MediaRecord record = m_mediaRepository.mediaByTmdbId(tmdbId, kind);
            if (!record.isValid()) {
                return;
            }

            QStringList genres;
            for (const std::string &genre : details->genreNames) {
                genres.append(QString::fromStdString(genre));
            }

            record.genres = genres.join(QStringLiteral(", "));
            record.runtimeMinutes = details->runtimeMinutes;
            record.overview = QString::fromStdString(details->overview);
            record.rating = details->rating;
            record.certification = QString::fromStdString(details->certification);
            if (details->backdropPath.has_value()) {
                record.backdropPath = QString::fromStdString(*details->backdropPath);
            }

            m_mediaRepository.upsertMedia(record);
            storeTitleCredits(record.id, *details);
            m_mediaRepository.markCreditsFetched(record.id);
            if (kind == QLatin1String("movie")) {
                noteFilmCollection(record.id, tmdbId, details->collection);
            }
            noteTitleChanged(record.id);
        });
    Q_UNUSED(id)
    return true;
}

void MetadataService::checkCollections()
{
    if (!available() || !m_client || m_heldForSetup || m_heldForStreaming) {
        return;
    }

    ensureCollectionFilms();

    const QList<qint64> unchecked = m_mediaRepository.filmsWithoutCollectionCheck();
    const qint64 staleBefore = QDateTime::currentDateTimeUtc().toSecsSinceEpoch()
        - kCollectionMaxAgeSeconds;
    const QList<qint64> stale = m_mediaRepository.collectionsToFetch(staleBefore);

    if (unchecked.isEmpty() && stale.isEmpty()) {
        MM_LOG_D() << "every film has its collection checked and no collection is stale";
        return;
    }

    MM_LOG_I() << "collections:" << unchecked.size()
               << "films matched before collections get their details once more,"
               << stale.size() << "collections are missing or older than a month";

    for (const qint64 tmdbId : unchecked) {
        fetchDetails(tmdbId, QStringLiteral("movie"));
    }
    for (const qint64 collectionId : stale) {
        fetchCollection(collectionId);
    }
}

QList<qint64> MetadataService::filmsWantingDetails(int *candidateCount) const
{
    const QList<qint64> candidates = CollectionPage::filmsNotInLibrary(
        m_mediaRepository.ownedCollections(), m_mediaRepository.collectionsByOwnedFilm(),
        m_universes, QDate::currentDate());
    const QHash<qint64, FilmDetailsRecord> known = m_mediaRepository.filmDetails();
    const qint64 staleBefore = QDateTime::currentDateTimeUtc().toSecsSinceEpoch()
        - kCollectionMaxAgeSeconds;

    QList<qint64> wanted;
    for (const qint64 tmdbId : candidates) {
        const auto found = known.constFind(tmdbId);
        if (found == known.constEnd() || found->fetchedAt < staleBefore) {
            wanted.append(tmdbId);
        }
    }

    if (candidateCount) {
        *candidateCount = int(candidates.size());
    }
    return wanted;
}

void MetadataService::ensureCollectionFilms()
{
    if (!available() || !m_client || m_heldForSetup || m_setupCollectionsRunning
        || m_heldForStreaming) {
        return;
    }

    int candidates = 0;
    const QList<qint64> wanted = filmsWantingDetails(&candidates);

    if (wanted.isEmpty()) {
        MM_LOG_D() << "every film of the owned collections and universes has its details,"
                   << candidates << "films not in the library";
        return;
    }

    MM_LOG_I() << "collections: asking TMDB for the details of" << wanted.size() << "of"
               << candidates << "films not in the library";
    for (const qint64 tmdbId : std::as_const(wanted)) {
        fetchFilmDetails(tmdbId);
    }
}

void MetadataService::collectionsForSetup(bool oneAtATime)
{
    m_setupCollectionsRunning = true;
    m_setupCollectionsStage = 1;
    m_setupCollectionsWaiting.clear();
    m_setupCollectionsDone = 0;
    m_setupCollectionsTotal = 0;
    m_setupCollectionsFailed = 0;
    m_setupCollectionsOneAtATime = oneAtATime;

    emit setupCollectionsProgress(0, 0);

    if (!available() || !m_client) {
        QTimer::singleShot(0, this, &MetadataService::finishSetupCollections);
        return;
    }

    if (oneAtATime) {
        m_client->setConcurrentRequestLimit(1);
    }

    const qint64 staleBefore = QDateTime::currentDateTimeUtc().toSecsSinceEpoch()
        - kCollectionMaxAgeSeconds;
    const QList<qint64> collections = m_mediaRepository.collectionsToFetch(staleBefore);
    for (const qint64 collectionId : collections) {
        if (fetchCollection(collectionId)) {
            m_setupCollectionsWaiting.insert(TmdbAsk::collectionKey(collectionId));
        }
    }
    m_setupCollectionsTotal = int(m_setupCollectionsWaiting.size());

    MM_LOG_I() << "setup asks TMDB for the films of" << m_setupCollectionsTotal
               << "collections" << (oneAtATime ? "one at a time" : "");
    emit setupCollectionsProgress(0, m_setupCollectionsTotal);

    if (m_setupCollectionsWaiting.isEmpty()) {
        QTimer::singleShot(0, this, &MetadataService::startSetupCollectionFilms);
    }
}

void MetadataService::startSetupCollectionFilms()
{
    if (!m_setupCollectionsRunning) {
        return;
    }

    m_setupCollectionsStage = 2;
    int candidates = 0;
    const QList<qint64> wanted = filmsWantingDetails(&candidates);
    for (const qint64 tmdbId : wanted) {
        if (fetchFilmDetails(tmdbId)) {
            m_setupCollectionsWaiting.insert(QStringLiteral("film/%1").arg(tmdbId));
        }
    }
    m_setupCollectionsTotal += int(m_setupCollectionsWaiting.size());

    MM_LOG_I() << "setup asks TMDB for the details of" << m_setupCollectionsWaiting.size()
               << "of" << candidates << "films of the collections not in the library";
    emit setupCollectionsProgress(m_setupCollectionsDone, m_setupCollectionsTotal);

    if (m_setupCollectionsWaiting.isEmpty()) {
        QTimer::singleShot(0, this, &MetadataService::finishSetupCollections);
    }
}

void MetadataService::settleSetupCollectionWork(const QString &key, bool fetched)
{
    if (!m_setupCollectionsRunning || !m_setupCollectionsWaiting.remove(key)) {
        return;
    }

    ++m_setupCollectionsDone;
    if (!fetched) {
        ++m_setupCollectionsFailed;
    }
    emit setupCollectionsProgress(m_setupCollectionsDone, m_setupCollectionsTotal);

    if (!m_setupCollectionsWaiting.isEmpty()) {
        return;
    }
    if (m_setupCollectionsStage == 1) {
        QTimer::singleShot(0, this, &MetadataService::startSetupCollectionFilms);
    } else {
        QTimer::singleShot(0, this, &MetadataService::finishSetupCollections);
    }
}

void MetadataService::finishSetupCollections()
{
    if (!m_setupCollectionsRunning) {
        return;
    }

    m_setupCollectionsRunning = false;
    m_setupCollectionsWaiting.clear();
    if (m_setupCollectionsOneAtATime && m_client) {
        m_client->setConcurrentRequestLimit(TmdbClient::MaximumConcurrentRequests);
    }
    m_setupCollectionsOneAtATime = false;

    MM_LOG_I() << "setup collections done:" << m_setupCollectionsDone << "of"
               << m_setupCollectionsTotal << "asked," << m_setupCollectionsFailed << "failed";
    m_collectionsChangedSoon.start();
    emit setupCollectionsFinished(m_setupCollectionsFailed);
}

bool MetadataService::fetchFilmDetails(qint64 tmdbId)
{
    if (!m_client) {
        return false;
    }

    const QString key = QStringLiteral("film/%1").arg(tmdbId);
    const TmdbAsk::Verdict verdict = m_filmAsks.ask(key);
    if (verdict != TmdbAsk::Verdict::AskNow) {
        return verdict == TmdbAsk::Verdict::AlreadyAsking;
    }

    ++m_pending;
    announcePending();

    const auto id = m_client->titleDetails(
        TmdbMediaType::Movie,
        TmdbId(tmdbId),
        [this, tmdbId, key](TmdbClient::TitleDetailsResult result) {
            if (m_pending > 0) {
                --m_pending;
            }
            announcePending();

            auto *dto = std::get_if<TmdbTitleDetailsDto>(&result);
            if (!dto) {
                auto *error = std::get_if<TmdbError>(&result);
                m_filmAsks.fail(key, error ? error->httpStatus() : std::optional<int>());
                MM_LOG_W() << "no details for film" << tmdbId << "not in the library";
                settleSetupCollectionWork(key, false);
                return;
            }

            FilmDetailsRecord details;
            details.tmdbId = tmdbId;
            details.title = QString::fromStdString(dto->title);
            if (dto->releaseDate.has_value()) {
                details.releaseDate = QString::fromStdString(*dto->releaseDate);
            }
            if (dto->posterPath.has_value()) {
                details.posterPath = QString::fromStdString(*dto->posterPath);
            }
            if (dto->backdropPath.has_value()) {
                details.backdropPath = QString::fromStdString(*dto->backdropPath);
            }
            details.runtimeMinutes = dto->runtimeMinutes;
            details.rating = dto->rating;

            if (!m_mediaRepository.saveFilmDetails(details)) {
                m_filmAsks.fail(key, std::optional<int>());
                settleSetupCollectionWork(key, false);
                return;
            }
            m_filmAsks.answer(key);
            MM_LOG_D() << "details of film" << tmdbId << details.title << "saved,"
                       << details.runtimeMinutes << "min, rating" << details.rating;
            m_collectionsChangedSoon.start();
            settleSetupCollectionWork(key, true);
        });
    Q_UNUSED(id)
    return true;
}

void MetadataService::noteFilmCollection(qint64 mediaId, qint64 tmdbId,
                                         const std::optional<TmdbCollectionRefDto> &collection)
{
    const qint64 collectionId = collection.has_value() ? qint64(collection->id) : 0;
    const qint64 before = m_mediaRepository.collectionIdFor(mediaId);

    if (!m_mediaRepository.setMediaCollection(mediaId, collectionId)) {
        return;
    }

    if (collectionId <= 0) {
        if (before > 0) {
            MM_LOG_I() << "tmdb" << tmdbId << "is no longer part of collection" << before;
        } else {
            MM_LOG_D() << "tmdb" << tmdbId << "is not part of a collection";
        }
        return;
    }

    CollectionRecord reference;
    reference.tmdbId = collectionId;
    reference.name = QString::fromStdString(collection->name);
    if (collection->posterPath.has_value()) {
        reference.posterPath = QString::fromStdString(*collection->posterPath);
    }
    if (collection->backdropPath.has_value()) {
        reference.backdropPath = QString::fromStdString(*collection->backdropPath);
    }
    m_mediaRepository.noteCollection(reference);

    MM_LOG_D() << "tmdb" << tmdbId << "belongs to collection" << collectionId
               << reference.name;

    const CollectionRecord known = m_mediaRepository.collectionById(collectionId);
    const qint64 staleBefore = QDateTime::currentDateTimeUtc().toSecsSinceEpoch()
        - kCollectionMaxAgeSeconds;
    if (known.fetchedAt <= 0 || known.fetchedAt < staleBefore) {
        if (m_heldForSetup) {
            MM_LOG_D() << "collection" << collectionId << "is asked for in setup's collections step";
            return;
        }
        fetchCollection(collectionId);
    }
}

bool MetadataService::fetchCollection(qint64 collectionId)
{
    if (collectionId <= 0 || !m_client) {
        return false;
    }

    const QString key = TmdbAsk::collectionKey(collectionId);
    const TmdbAsk::Verdict verdict = m_collectionAsks.ask(key);
    if (verdict != TmdbAsk::Verdict::AskNow) {
        return verdict == TmdbAsk::Verdict::AlreadyAsking;
    }

    ++m_pending;
    announcePending();

    const auto id = m_client->collection(
        TmdbId(collectionId),
        [this, collectionId, key](TmdbClient::CollectionResult result) {
            if (m_pending > 0) {
                --m_pending;
            }
            announcePending();

            auto *dto = std::get_if<TmdbCollectionDto>(&result);
            if (!dto) {
                auto *error = std::get_if<TmdbError>(&result);
                m_collectionAsks.fail(key, error ? error->httpStatus()
                                                 : std::optional<int>());
                MM_LOG_W() << "could not read collection" << collectionId
                           << "- it is asked for again next launch";
                settleSetupCollectionWork(key, false);
                return;
            }

            std::vector<TmdbCollectionPartDto> sorted = dto->parts;
            std::stable_sort(sorted.begin(), sorted.end(),
                             [](const TmdbCollectionPartDto &a, const TmdbCollectionPartDto &b) {
                const bool aDated = a.releaseDate.has_value();
                const bool bDated = b.releaseDate.has_value();
                if (aDated != bDated) {
                    return aDated;
                }
                return aDated && *a.releaseDate < *b.releaseDate;
            });

            CollectionRecord collection;
            collection.tmdbId = collectionId;
            collection.name = QString::fromStdString(dto->name);
            collection.overview = QString::fromStdString(dto->overview);
            if (dto->posterPath.has_value()) {
                collection.posterPath = QString::fromStdString(*dto->posterPath);
            }
            if (dto->backdropPath.has_value()) {
                collection.backdropPath = QString::fromStdString(*dto->backdropPath);
            }
            int position = 0;
            for (const TmdbCollectionPartDto &item : sorted) {
                CollectionPartRecord part;
                part.tmdbId = item.id;
                part.position = position++;
                part.title = QString::fromStdString(item.title);
                if (item.releaseDate.has_value()) {
                    part.releaseDate = QString::fromStdString(*item.releaseDate);
                }
                if (item.posterPath.has_value()) {
                    part.posterPath = QString::fromStdString(*item.posterPath);
                }
                if (item.backdropPath.has_value()) {
                    part.backdropPath = QString::fromStdString(*item.backdropPath);
                }
                collection.parts.append(part);
            }

            const bool grouped = m_database.transaction();
            const bool saved = m_mediaRepository.saveCollection(collection);
            if (!saved) {
                if (grouped) {
                    m_database.rollback();
                }
                m_collectionAsks.fail(key, std::optional<int>());
                settleSetupCollectionWork(key, false);
                return;
            }
            if (grouped && !m_database.commit()) {
                m_database.rollback();
                m_collectionAsks.fail(key, std::optional<int>());
                MM_LOG_E() << "collection" << collectionId
                           << "could not be committed - it is asked for again next launch";
                settleSetupCollectionWork(key, false);
                return;
            }

            m_collectionAsks.answer(key);
            MM_LOG_I() << "collection" << collectionId << collection.name << "has"
                       << collection.parts.size() << "films";
            m_collectionsChangedSoon.start();
            settleSetupCollectionWork(key, true);
            ensureCollectionFilms();
        });
    Q_UNUSED(id)
    return true;
}

bool MetadataService::creditsSweeping() const
{
    return !m_creditsSweepDone && !m_heldForStreaming && m_creditsLeft > 0;
}

int MetadataService::creditsRemaining() const
{
    return m_creditsLeft;
}

bool MetadataService::artworkWarming() const
{
    return m_artworkLeft > 0 && m_warmDownloads > 0;
}

int MetadataService::artworkRemaining() const
{
    return m_artworkLeft;
}

void MetadataService::noteBackgroundWork(int creditsLeft, int artworkLeft)
{
    if (creditsLeft == m_creditsLeft && artworkLeft == m_artworkLeft) {
        return;
    }
    m_creditsLeft = creditsLeft;
    m_artworkLeft = artworkLeft;
    emit backgroundWorkChanged();
}

void MetadataService::startCreditsSweep()
{
    m_creditsSweepDone = false;
    noteBackgroundWork(m_mediaRepository.titlesWithoutCreditsCount()
                           + m_mediaRepository.seasonsWithoutCreditsCount(),
                       m_artworkLeft);
    if (!m_creditsSweep.isActive()) {
        m_creditsSweep.start();
    }
}

void MetadataService::sweepCredits()
{
    if (m_creditsSweepDone) {
        return;
    }

    if (m_heldForStreaming) {
        emit backgroundWorkChanged();
        return;
    }

    noteBackgroundWork(m_mediaRepository.titlesWithoutCreditsCount()
                           + m_mediaRepository.seasonsWithoutCreditsCount(),
                       m_artworkLeft);

    if (!available() || !m_client || m_heldForSetup || !m_matchQueue.isEmpty()) {
        m_creditsSweep.start(3000);
        return;
    }

    if (m_pending > 0) {
        m_creditsSweep.start(400);
        return;
    }

    const QList<qint64> titles = m_mediaRepository.titlesWithoutCredits(1);
    if (!titles.isEmpty()) {
        const qint64 mediaId = titles.first();
        const MediaRecord media = m_mediaRepository.mediaById(mediaId);

        if (!media.isValid() || media.tmdbId <= 0
            || !fetchDetails(media.tmdbId, media.kind)) {
            m_mediaRepository.markCreditsFetched(mediaId);
        } else {
            ++m_creditsSwept;
        }

        m_creditsSweep.start(250);
        return;
    }

    const QList<QPair<qint64, int>> seasons =
        m_mediaRepository.seasonsWithoutCredits(1);
    if (!seasons.isEmpty()) {
        const qint64 mediaId = seasons.first().first;
        const int season = seasons.first().second;
        const MediaRecord media = m_mediaRepository.mediaById(mediaId);

        if (!media.isValid() || media.tmdbId <= 0
            || !fetchSeason(media.tmdbId, mediaId, season, true)) {
            m_mediaRepository.markSeasonCreditsFetched(mediaId, season);
        } else {
            ++m_creditsSwept;
        }

        m_creditsSweep.start(250);
        return;
    }

    m_creditsSweepDone = true;
    noteBackgroundWork(0, m_artworkLeft);
    if (m_creditsSwept > 0) {
        MM_LOG_I() << "asked who made" << m_creditsSwept
                   << "titles and seasons - the library is credited";
        m_creditsSwept = 0;
        m_warmAgain = true;
        m_prefetchStart.start();
    }
}

void MetadataService::ensureSeasons(qint64 mediaId)
{
    if (!available() || mediaId <= 0 || refusesWhileStreaming("fetching seasons")) {
        return;
    }

    const MediaRecord media = m_mediaRepository.mediaById(mediaId);
    if (!media.isValid() || media.kind != QLatin1String("tv")) {
        return;
    }

    QSet<int> seasons;
    const QList<EpisodeRecord> known = m_mediaRepository.episodesFor(mediaId);
    for (const EpisodeRecord &episode : known) {
        if (episode.season >= 0 && episode.episode > 0) {
            seasons.insert(episode.season);
        }
    }

    for (int season : seasons) {
        fetchSeason(media.tmdbId, mediaId, season);
    }
}

bool MetadataService::fetchSeason(qint64 tmdbId, qint64 mediaId,
                                  int seasonNumber, bool evenIfFetched)
{
    if (seasonNumber < 0 || !m_client) {
        return false;
    }

    const QString key = TmdbAsk::seasonKey(tmdbId, seasonNumber);
    if (m_seasonAsks.ask(key) != TmdbAsk::Verdict::AskNow) {
        return false;
    }

    if (!evenIfFetched && m_mediaRepository.seasonFetched(mediaId, seasonNumber)) {
        m_seasonAsks.answer(key);
        return false;
    }

    ++m_pending;
    announcePending();

    const auto id = m_client->season(
        TmdbId(tmdbId),
        seasonNumber,
        std::nullopt,
        [this, mediaId, tmdbId, seasonNumber, key](TmdbClient::SeasonResult result) {
            if (m_pending > 0) {
                --m_pending;
            }
            announcePending();

            auto *season = std::get_if<TmdbSeasonDto>(&result);
            if (!season) {
                auto *error = std::get_if<TmdbError>(&result);
                m_seasonAsks.fail(key, error ? error->httpStatus()
                                             : std::optional<int>());

                if (m_seasonAsks.isAnswered(key)) {
                    MM_LOG_I() << "tmdb" << tmdbId << "has no season"
                               << seasonNumber << "- not asking again";
                } else {
                    MM_LOG_W() << "could not read season" << seasonNumber
                               << "for tmdb" << tmdbId << "- will try again";
                }
                return;
            }

            const bool grouped = m_database.transaction();

            for (const TmdbEpisodeDto &item : season->episodes) {
                EpisodeRecord record;
                record.mediaId = mediaId;
                record.season = item.seasonNumber > 0 ? item.seasonNumber
                                                      : seasonNumber;
                record.episode = item.episodeNumber;
                record.title = QString::fromStdString(item.name);
                record.overview = QString::fromStdString(item.overview);
                record.stillPath = item.stillPath.has_value()
                    ? QString::fromStdString(*item.stillPath) : QString();
                record.airDate = item.airDate.has_value()
                    ? QString::fromStdString(*item.airDate) : QString();
                record.runtimeMinutes = item.runtimeMinutes;

                m_mediaRepository.overwriteEpisodeFromTmdb(record);

                const qint64 episodeId = m_mediaRepository.episodeRowId(
                    mediaId, record.season, record.episode);
                if (episodeId > 0) {
                    storeEpisodeCredits(mediaId, episodeId, item);
                }
            }

            m_seasonAsks.answer(key);
            m_mediaRepository.markSeasonFetched(
                mediaId, seasonNumber,
                season->posterPath.has_value()
                    ? QString::fromStdString(*season->posterPath) : QString());
            m_mediaRepository.markSeasonCreditsFetched(mediaId, seasonNumber);

            if (grouped && !m_database.commit()) {
                m_database.rollback();
                MM_LOG_E() << "season" << seasonNumber << "of tmdb" << tmdbId
                           << "could not be committed - it is asked for again"
                           << "next launch";
                return;
            }

            MM_LOG_D() << "season" << seasonNumber << "of tmdb" << tmdbId
                       << "gave" << int(season->episodes.size()) << "episodes";
            noteTitleChanged(mediaId);
        });
    Q_UNUSED(id)
    return true;
}

void MetadataService::runMatch(const QString &fileHandle,
                               const QString &fileName,
                               const QString &folderHandle,
                               bool write,
                               qint64 fileId)
{
    const ParsedFileName parsed = folderHandle.isEmpty()
        ? FileNameParser::parse(fileName)
        : FileNameParser::parsePath(folderHandle + QLatin1Char('/') + fileName);

    runParsedMatch(parsed, fileHandle, fileName, write, fileId);
}

void MetadataService::runParsedMatch(const ParsedFileName &parsed,
                                     const QString &fileHandle,
                                     const QString &fileName,
                                     bool write,
                                     qint64 fileId)
{
    QVariantMap base;
    base.insert(QStringLiteral("fileHandle"), fileHandle);
    base.insert(QStringLiteral("fileName"), fileName);
    base.insert(QStringLiteral("fileId"), fileId);

    ++m_pending;
    announcePending();

    if (!available()) {
        base.insert(QStringLiteral("error"),
                    tr("No TMDB key. Add one in Settings or build with one."));
        finish(base);
        return;
    }

    MM_LOG_D() << "parsed" << fileName << "-> title" << parsed.title
               << "season" << parsed.season << "episode" << parsed.episode
               << (parsed.alternativeTitle.isEmpty()
                       ? QString()
                       : QStringLiteral("aka ") + parsed.alternativeTitle);

    base.insert(QStringLiteral("parsedTitle"), parsed.title);
    base.insert(QStringLiteral("parsedYear"), parsed.year);
    base.insert(QStringLiteral("isEpisode"), parsed.looksLikeEpisode());

    if (parsed.title.isEmpty()) {
        base.insert(QStringLiteral("error"), tr("Nothing usable in the filename."));
        if (write) {
            recordNoMatch(fileId, fileHandle,
                          QStringLiteral("nothing usable in the filename"));
        }
        finish(base);
        return;
    }

    searchAndScore(parsed, fileHandle, base, write, true);
}

void MetadataService::searchAndScore(const ParsedFileName &parsed,
                                     const QString &fileHandle,
                                     const QVariantMap &base,
                                     bool write,
                                     bool useYear)
{
    const bool withYear = useYear && parsed.year > 0;
    const QString key = TmdbAsk::searchKey(parsed, useYear);
    const WaitingMatch waiting{base, parsed, fileHandle, write, withYear,
                               base.value(QStringLiteral("fileId")).toLongLong()};

    const TmdbAsk::Verdict verdict = m_searchAsks.ask(key, waiting);
    if (verdict == TmdbAsk::Verdict::AlreadyAnswered) {
        completeMatch(waiting, m_searchAsks.answerFor(key));
        return;
    }
    if (verdict == TmdbAsk::Verdict::AlreadyAsking) {
        return;
    }

    TmdbSearchRequestDto request;
    request.mediaType = parsed.looksLikeEpisode() ? TmdbMediaType::Tv
                                                  : TmdbMediaType::Movie;
    request.query = parsed.title.toStdString();
    request.page = 1;
    if (withYear) {
        request.year = parsed.year;
    }

    auto handler = [this, key](TmdbClient::DiscoverResult result) {
        if (auto *page = std::get_if<TmdbDiscoverPageDto>(&result)) {
            deliverSearch(key, page->results);
            return;
        }

        const QList<WaitingMatch> waiting = m_searchAsks.fail(key, std::nullopt);
        auto *error = std::get_if<TmdbError>(&result);
        for (const WaitingMatch &item : waiting) {
            QVariantMap failed = item.base;
            failed.insert(QStringLiteral("unreachable"), true);
            failed.insert(QStringLiteral("error"),
                          error ? QString::fromStdString(error->userSafeContext())
                                : tr("No TMDB result."));
            finish(failed);
        }
    };

    const auto id = parsed.looksLikeEpisode()
        ? m_client->searchTv(request, handler)
        : m_client->searchMovie(request, handler);
    Q_UNUSED(id)
}

void MetadataService::deliverSearch(
    const QString &key, const std::vector<TmdbTitleResultDto> &results)
{
    const QList<WaitingMatch> waiting = m_searchAsks.answer(key, results);
    const bool grouped = waiting.size() > 1 && m_database.transaction();

    for (const WaitingMatch &item : waiting) {
        completeMatch(item, results);
    }

    if (grouped && !m_database.commit()) {
        m_database.rollback();
        MM_LOG_E() << "the matches for" << waiting.size()
                   << "files waiting on one search could not be committed";
    }
}

bool MetadataService::askAgainAnotherWay(const WaitingMatch &waiting)
{
    const MatchDecision::Retry retry =
        MatchDecision::askAgainAnotherWay(waiting.parsed, waiting.usedYear);

    switch (retry.kind) {
    case MatchDecision::Retry::Nothing:
        return false;

    case MatchDecision::Retry::WithoutTheYear:
        MM_LOG_I() << "no answer for" << waiting.parsed.title
                   << waiting.parsed.year << "- asking without the year";
        break;

    case MatchDecision::Retry::TheOtherName:
        MM_LOG_I() << "no answer for" << waiting.parsed.title
                   << "- trying the other name" << retry.parsed.title;
        break;
    }

    searchAndScore(retry.parsed, waiting.fileHandle, waiting.base,
                   waiting.write, retry.useYear);
    return true;
}

bool MetadataService::askAgainWithTheOtherName(const WaitingMatch &waiting)
{
    const MatchDecision::Retry retry =
        MatchDecision::askAgainAnotherWay(waiting.parsed, false);
    if (retry.kind != MatchDecision::Retry::TheOtherName) {
        return false;
    }

    MM_LOG_I() << "only a guess for" << waiting.parsed.title
               << "- asking for" << retry.parsed.title << "instead";
    searchAndScore(retry.parsed, waiting.fileHandle, waiting.base,
                   waiting.write, retry.useYear);
    return true;
}

void MetadataService::completeMatch(
    const WaitingMatch &waiting, const std::vector<TmdbTitleResultDto> &results)
{
    QVariantMap base = waiting.base;

    if (results.empty()) {
        if (askAgainAnotherWay(waiting)) {
            return;
        }

        base.insert(QStringLiteral("error"), tr("No TMDB result."));
        if (waiting.write) {
            recordNoMatch(waiting.fileId, waiting.fileHandle,
                          QStringLiteral("TMDB returned nothing"));
        }
        finish(base);
        return;
    }

    const MatchDecision::Choice choice =
        MatchDecision::pickBest(waiting.parsed, results);
    const TmdbTitleResultDto *best = &results[choice.index];
    const MatchScore bestScore = choice.score;

    if (!bestScore.isConfident() && !bestScore.isSuggestion()
        && askAgainAnotherWay(waiting)) {
        return;
    }

    if (!bestScore.isConfident() && !waiting.parsed.alternativeTitle.isEmpty()
        && askAgainWithTheOtherName(waiting)) {
        return;
    }

    base.insert(QStringLiteral("tmdbId"), qint64(best->id));
    base.insert(QStringLiteral("matchedTitle"),
                QString::fromStdString(best->title));
    base.insert(QStringLiteral("matchedYear"),
                best->releaseDate.has_value() && best->releaseDate->size() >= 4
                    ? QString::fromStdString(best->releaseDate->substr(0, 4)).toInt()
                    : 0);
    base.insert(QStringLiteral("score"), bestScore.value);
    base.insert(QStringLiteral("reason"), bestScore.reason);
    base.insert(QStringLiteral("confident"), bestScore.isConfident());
    base.insert(QStringLiteral("suggested"), bestScore.isSuggestion());
    base.insert(QStringLiteral("candidates"), int(results.size()));
    base.insert(QStringLiteral("overview"),
                QString::fromStdString(best->overview));
    base.insert(QStringLiteral("rating"), best->rating);
    base.insert(QStringLiteral("posterPath"),
                best->posterPath.has_value()
                    ? QString::fromStdString(*best->posterPath)
                    : QString());

    if (waiting.write) {
        applyResult(waiting.fileId, waiting.fileHandle, waiting.parsed, base);
    }

    finish(base);
}
