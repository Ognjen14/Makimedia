#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

class Database;

struct MediaRecord
{
    qint64 id = 0;
    qint64 tmdbId = 0;
    QString kind;
    QString title;
    QString originalTitle;
    int year = 0;
    QString overview;
    double rating = 0.0;
    int runtimeMinutes = 0;
    QString genres;
    QString certification;
    QString posterPath;
    QString backdropPath;

    int fileCount = 0;
    int suggestedFileCount = 0;
    int seasonCount = 0;
    QString firstFileHandle;
    int watchedCount = 0;
    double partialProgress = 0.0;
    int newFileCount = 0;
    qint64 lastAdded = 0;

    bool isValid() const { return id > 0; }

    bool operator==(const MediaRecord &other) const
    {
        return id == other.id
            && tmdbId == other.tmdbId
            && kind == other.kind
            && title == other.title
            && originalTitle == other.originalTitle
            && year == other.year
            && overview == other.overview
            && rating == other.rating
            && runtimeMinutes == other.runtimeMinutes
            && genres == other.genres
            && certification == other.certification
            && posterPath == other.posterPath
            && backdropPath == other.backdropPath
            && fileCount == other.fileCount
            && suggestedFileCount == other.suggestedFileCount
            && seasonCount == other.seasonCount
            && firstFileHandle == other.firstFileHandle
            && watchedCount == other.watchedCount
            && partialProgress == other.partialProgress
            && newFileCount == other.newFileCount
            && lastAdded == other.lastAdded;
    }

    bool operator!=(const MediaRecord &other) const
    {
        return !(*this == other);
    }
};

struct EpisodeRecord
{
    qint64 mediaId = 0;
    int season = 0;
    int episode = 0;
    QString title;
    QString overview;
    QString stillPath;
    QString airDate;
    int runtimeMinutes = 0;

    qint64 fileId = -1;
    QString fileHandle;
    QString fileName;
    double positionSeconds = 0.0;
    double durationSeconds = 0.0;
    qint64 lastPlayed = 0;
    bool watched = false;
    bool missing = false;

    bool hasFile() const { return fileId >= 0; }

    bool operator==(const EpisodeRecord &other) const
    {
        return mediaId == other.mediaId
            && season == other.season
            && episode == other.episode
            && title == other.title
            && overview == other.overview
            && stillPath == other.stillPath
            && airDate == other.airDate
            && runtimeMinutes == other.runtimeMinutes
            && fileId == other.fileId
            && fileHandle == other.fileHandle
            && fileName == other.fileName
            && positionSeconds == other.positionSeconds
            && durationSeconds == other.durationSeconds
            && lastPlayed == other.lastPlayed
            && watched == other.watched
            && missing == other.missing;
    }

    bool operator!=(const EpisodeRecord &other) const
    {
        return !(*this == other);
    }
};

struct CreditRecord
{
    enum Kind
    {
        Cast = 0,
        Director = 1,
        Writer = 2,
        Creator = 3
    };

    int kind = Cast;
    QString name;
    QString role;
    QString profilePath;

    bool operator==(const CreditRecord &other) const
    {
        return kind == other.kind
            && name == other.name
            && role == other.role
            && profilePath == other.profilePath;
    }
};

struct ArtworkPaths
{
    QStringList posters;
    QStringList backdrops;
    QStringList stills;
    QStringList profiles;
    QStringList seasonPosters;
    QStringList seasonBackdrops;
    QStringList collectionPosters;
};

struct CollectionPartRecord
{
    qint64 tmdbId = 0;
    int position = 0;
    QString title;
    QString releaseDate;
    QString posterPath;
    QString backdropPath;
};

struct CollectionRecord
{
    qint64 tmdbId = 0;
    QString name;
    QString overview;
    QString posterPath;
    QString backdropPath;
    qint64 fetchedAt = 0;
    QList<CollectionPartRecord> parts;

    bool isValid() const { return tmdbId > 0; }
};

struct FilmDetailsRecord
{
    qint64 tmdbId = 0;
    QString title;
    QString releaseDate;
    QString posterPath;
    QString backdropPath;
    int runtimeMinutes = 0;
    double rating = 0.0;
    qint64 fetchedAt = 0;
};

struct CustomCollectionRecord
{
    qint64 id = 0;
    QString name;
    QString description;
    QString coverMode;
    qint64 createdAt = 0;
    QList<qint64> mediaIds;

    bool isValid() const { return id > 0; }
};

struct HiddenCollectionRecord
{
    qint64 collectionId = 0;
    QString name;
    qint64 hiddenAt = 0;
};

struct HiddenCollectionFilmRecord
{
    QString scope;
    qint64 tmdbId = 0;
    QString title;
    QString collectionName;
    qint64 hiddenAt = 0;
};

struct DiscardedShowRecord
{
    QString title;
    QString folder;
    int fileCount = 0;
    qint64 discardedAt = 0;
};

struct FileMediaLink
{
    qint64 fileId = 0;
    qint64 mediaId = 0;
    double confidence = 0.0;
    bool suggested = false;

    bool isValid() const { return mediaId > 0; }
};

struct LinkedFile
{
    FileMediaLink link;
    QString fileHandle;
};

struct MatchOverride
{
    QString fileHandle;
    qint64 tmdbId = 0;
    QString kind;
    int season = 0;
    int episode = 0;

    bool isValid() const { return tmdbId > 0; }
};

struct PinRecord
{
    qint64 fileId = 0;
    QString fileHandle;
    MediaRecord media;
    int season = 0;
    int episode = 0;
    QString episodeTitle;
};

class MediaRepository
{
public:
    explicit MediaRepository(Database &database);

    qint64 upsertMedia(const MediaRecord &record);
    MediaRecord mediaByTmdbId(qint64 tmdbId, const QString &kind) const;
    MediaRecord mediaForFile(qint64 fileId) const;
    MediaRecord mediaById(qint64 mediaId) const;

    QList<MediaRecord> allOfKind(const QString &kind) const;

    QList<MediaRecord> searchOfKind(const QString &kind, const QString &query,
                                    int limit) const;
    QList<MediaRecord> withSuggestions() const;
    QList<MediaRecord> listedByIds(const QList<qint64> &ids) const;
    QList<MediaRecord> recentlyAddedTitles(int limit, qint64 freshSeconds) const;
    QHash<qint64, qint64> lastAddedByTitle() const;
    QList<qint64> fileIdsFor(qint64 mediaId) const;
    static bool listsBefore(const MediaRecord &a, const MediaRecord &b);
    bool hasSuggestions(qint64 mediaId) const;

    int settleSuggestions(qint64 mediaId);
    bool detachFileFromOtherEpisodes(qint64 fileId, qint64 keepMediaId);
    QList<EpisodeRecord> episodesFor(qint64 mediaId) const;

    bool linkFile(qint64 fileId, qint64 mediaId, double confidence, bool suggested);
    FileMediaLink linkForFile(qint64 fileId) const;
    bool unlinkFile(qint64 fileId);
    QList<LinkedFile> unpinnedLinks() const;
    bool detachUnlinkedEpisodes();

    bool upsertEpisode(const EpisodeRecord &record, qint64 fileId);

    bool overwriteEpisodeFromTmdb(const EpisodeRecord &record);
    EpisodeRecord episodeForFile(qint64 fileId) const;
    EpisodeRecord nextEpisodeWithFile(qint64 mediaId,
                                      int season,
                                      int episode) const;

    bool markSeasonFetched(qint64 mediaId, int season, const QString &posterPath);
    bool seasonFetched(qint64 mediaId, int season) const;
    QHash<int, QString> seasonPosters(qint64 mediaId) const;

    bool replaceCredits(qint64 mediaId, qint64 episodeId,
                        const QList<CreditRecord> &credits);
    QList<CreditRecord> creditsFor(qint64 mediaId, qint64 episodeId = 0) const;

    qint64 episodeRowId(qint64 mediaId, int season, int episode) const;

    bool markCreditsFetched(qint64 mediaId);
    bool markSeasonCreditsFetched(qint64 mediaId, int season);

    QList<qint64> titlesWithoutCredits(int limit) const;
    QList<QPair<qint64, int>> seasonsWithoutCredits(int limit) const;
    int titlesWithoutCreditsCount() const;
    int seasonsWithoutCreditsCount() const;
    QStringList seasonBackdropPaths(const QString &mediaFilter) const;
    bool clearFetchedSeasons();

    ArtworkPaths artworkPaths() const;
    ArtworkPaths artworkPathsFor(const QList<qint64> &mediaIds) const;

    bool setMediaCollection(qint64 mediaId, qint64 collectionId);
    qint64 collectionIdFor(qint64 mediaId) const;
    QList<qint64> filmsWithoutCollectionCheck() const;
    bool noteCollection(const CollectionRecord &reference);
    bool saveCollection(const CollectionRecord &collection);
    CollectionRecord collectionById(qint64 collectionId) const;
    QList<qint64> collectionsToFetch(qint64 fetchedBefore) const;
    QHash<qint64, qint64> collectionsByMedia() const;
    QHash<qint64, qint64> collectionsByOwnedFilm() const;
    QList<CollectionRecord> ownedCollections() const;
    qint64 createCustomCollection(const CustomCollectionRecord &collection);
    bool hideCollection(qint64 collectionId, const QString &name);
    bool restoreCollection(qint64 collectionId);
    QList<HiddenCollectionRecord> hiddenCollections() const;
    QSet<qint64> hiddenCollectionIds() const;
    bool hideCollectionFilm(const QString &scope,
                            qint64 tmdbId,
                            const QString &title,
                            const QString &collectionName);
    bool restoreCollectionFilm(const QString &scope, qint64 tmdbId);
    QList<HiddenCollectionFilmRecord> hiddenCollectionFilms() const;
    QHash<QString, QSet<qint64>> hiddenCollectionFilmsByScope() const;
    bool addCollectionFilm(const QString &scope,
                           qint64 tmdbId,
                           const QString &title,
                           const QString &collectionName);
    bool removeAddedCollectionFilm(const QString &scope, qint64 tmdbId);
    QHash<QString, QSet<qint64>> addedCollectionFilmsByScope() const;

    bool discardShow(const QStringList &fileHandles, const QString &title,
                     const QString &folder);
    bool restoreShow(const QString &title);
    QList<DiscardedShowRecord> discardedShows() const;
    QSet<QString> discardedShowFiles() const;
    QList<CustomCollectionRecord> customCollections() const;
    bool saveFilmDetails(const FilmDetailsRecord &details);
    QHash<qint64, FilmDetailsRecord> filmDetails() const;

    MatchOverride overrideFor(const QString &fileHandle) const;
    bool setOverride(const MatchOverride &value);
    bool clearOverride(const QString &fileHandle);

    qint64 pin(const PinRecord &request);

    int matchedCount() const;
    int suggestedCount() const;

private:
    Database &m_database;
};
