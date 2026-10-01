#pragma once

#include "Data/PlaybackState.h"
#include "Platform/MediaFileInfo.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

class Database;

struct LibraryFile
{
    qint64 id = -1;
    qint64 folderId = -1;
    QString handle;
    QString parentHandle;
    QString displayName;
    qint64 sizeBytes = 0;
    QDateTime modified;
    double durationSeconds = 0.0;
    QString container;
    QString videoCodec;
    QString audioCodec;
    int width = 0;
    int height = 0;
    bool hdr = false;
    int audioTrackCount = 0;
    int subtitleTrackCount = 0;
    bool matchAttempted = false;
    bool missing = false;
    PlaybackState playback;

    QString matchedTitle;
    QString matchedPosterPath;
    QString matchedBackdropPath;
    QString episodeStillPath;
    QString matchedKind;
    int matchedYear = 0;
    bool matchSuggested = false;

    QString episodeTitle;
    int season = 0;
    int episode = 0;

    bool isMatched() const { return !matchedTitle.isEmpty(); }
    bool isEpisode() const { return episode > 0; }
    bool isValid() const { return id >= 0; }

    bool operator==(const LibraryFile &other) const
    {
        return id == other.id
            && folderId == other.folderId
            && handle == other.handle
            && parentHandle == other.parentHandle
            && displayName == other.displayName
            && sizeBytes == other.sizeBytes
            && modified == other.modified
            && durationSeconds == other.durationSeconds
            && container == other.container
            && videoCodec == other.videoCodec
            && audioCodec == other.audioCodec
            && width == other.width
            && height == other.height
            && hdr == other.hdr
            && audioTrackCount == other.audioTrackCount
            && subtitleTrackCount == other.subtitleTrackCount
            && matchAttempted == other.matchAttempted
            && missing == other.missing
            && playback == other.playback
            && matchedTitle == other.matchedTitle
            && matchedPosterPath == other.matchedPosterPath
            && matchedBackdropPath == other.matchedBackdropPath
            && episodeStillPath == other.episodeStillPath
            && matchedKind == other.matchedKind
            && matchedYear == other.matchedYear
            && matchSuggested == other.matchSuggested
            && episodeTitle == other.episodeTitle
            && season == other.season
            && episode == other.episode;
    }

    bool operator!=(const LibraryFile &other) const
    {
        return !(*this == other);
    }
};

struct FolderSummary
{
    int fileCount = 0;
    qint64 totalBytes = 0;
};

struct ScanIndexRow
{
    qint64 sizeBytes = 0;
    qint64 modified = 0;
    bool missing = false;
    bool removed = false;
};

using ScanIndex = QHash<QString, ScanIndexRow>;

struct RemovedFileRecord
{
    QString handle;
    qint64 folderId = 0;
    QString displayName;
    QString folder;
    qint64 removedAt = 0;
};

struct BatchWrite
{
    QStringList inserted;
    QStringList changed;
    QStringList revived;
    int skipped = 0;

    int written() const
    {
        return int(inserted.size() + changed.size() + revived.size());
    }

    QStringList handles() const
    {
        return inserted + changed + revived;
    }
};

class FileRepository
{
public:
    explicit FileRepository(Database &database);

    int upsertBatch(qint64 folderId, const QList<MediaFileInfo> &files);
    int markMissingOutside(qint64 folderId, const QList<MediaFileInfo> &seen);

    ScanIndex scanIndex(qint64 folderId) const;
    BatchWrite upsertBatch(qint64 folderId, const QList<MediaFileInfo> &files,
                           ScanIndex &index);
    QStringList markMissingOutside(qint64 folderId, const QSet<QString> &seen,
                                   ScanIndex &index);
    QList<LibraryFile> byHandles(const QStringList &handles) const;
    QList<QPair<QString, QString>> settleMoves(const QStringList &missingHandles,
                                               const QStringList &writtenHandles);

    enum class MatchScope {
        Unasked,
        Unpinned
    };

    QList<LibraryFile> matchCandidates(MatchScope scope,
                                       const QStringList &handles = QStringList()) const;
    LibraryFile plainById(qint64 id) const;

    LibraryFile probeFieldsFor(const QString &handle) const;
    bool markProbeAttemptedFor(const QString &handle);
    QStringList failedProbeHandles(const QList<qint64> &folderIds) const;

    QList<LibraryFile> all() const;
    QList<LibraryFile> inFolder(qint64 folderId) const;
    QList<LibraryFile> withParentHandle(const QString &parentHandle) const;
    QList<LibraryFile> continueWatching(int limit) const;
    QList<LibraryFile> copiesOfFilm(const QString &handle) const;
    QList<LibraryFile> search(const QString &query, int limit) const;

    QList<LibraryFile> searchUnmatched(const QString &query, int limit) const;

    QList<LibraryFile> searchEpisodes(const QString &query, int limit) const;
    LibraryFile byHandle(const QString &handle) const;
    LibraryFile byId(qint64 id) const;
    QList<LibraryFile> byIds(const QList<qint64> &ids) const;

    bool updateProbeInfo(qint64 id,
                         double durationSeconds,
                         const QString &container,
                         const QString &videoCodec,
                         const QString &audioCodec,
                         int width,
                         int height,
                         bool hdr,
                         int audioTrackCount,
                         int subtitleTrackCount);

    bool markMatchAttempted(qint64 id);
    bool clearMatchAttempts();
    QList<LibraryFile> neverAsked() const;

    QStringList handlesWithPoster() const;

    QList<LibraryFile> neverProbed() const;
    QList<LibraryFile> unmatched() const;
    int unmatchedCount() const;

    QList<RemovedFileRecord> removeFromLibrary(const QList<LibraryFile> &files,
                                               const QHash<QString, QString> &folderPaths);
    QList<RemovedFileRecord> removedFiles() const;
    QList<qint64> restoreRemoved(const QStringList &handles);
    bool markProbeAttempted(qint64 id);
    bool clearProbeAttempts();

    int count() const;
    int continueWatchingCount() const;
    FolderSummary summaryUnder(const QString &prefix) const;
    QHash<QString, FolderSummary> summariesBelow(const QString &folderHandle) const;
    QHash<QString, LibraryFile> filesIn(const QString &parentHandle) const;

private:
    Database &m_database;
};
