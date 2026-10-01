#pragma once

#include "Data/MediaRepository.h"
#include "Library/CollectionShelf.h"
#include "Library/UniverseCatalog.h"

#include <QDate>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace CollectionPage {

enum Order {
    Release = 0,
    Story = 1
};

struct Row
{
    bool heading = false;
    bool show = false;
    QString key;
    QString phase;
    int number = 0;
    qint64 tmdbId = 0;
    QString title;
    int year = 0;
    int runtimeMinutes = 0;
    double rating = 0.0;
    QString posterPath;
    bool owned = false;
    bool added = false;
    QString handle;
    bool watched = false;
    double progress = 0.0;

    int leftMinutes() const;

    bool operator==(const Row &other) const
    {
        return heading == other.heading
            && show == other.show
            && key == other.key
            && phase == other.phase
            && number == other.number
            && tmdbId == other.tmdbId
            && title == other.title
            && year == other.year
            && runtimeMinutes == other.runtimeMinutes
            && rating == other.rating
            && posterPath == other.posterPath
            && owned == other.owned
            && added == other.added
            && handle == other.handle
            && watched == other.watched
            && progress == other.progress;
    }

    bool operator!=(const Row &other) const { return !(*this == other); }
};

struct Header
{
    bool valid = false;
    bool universe = false;
    bool custom = false;
    bool hasStoryOrder = false;
    QString name;
    QString tag;
    QString description;
    QString backdropPath;
    int firstYear = 0;
    int lastYear = 0;
    int total = 0;
    int owned = 0;
    int watched = 0;
    int totalMinutes = 0;
    int leftMinutes = 0;
    double averageRating = 0.0;
    QStringList posters;
    QString nextTitle;
    QString nextHandle;
    bool nextResumes = false;
    int nextLeftMinutes = 0;
    QStringList ownedHandles;
    bool allOwnedWatched = false;
};

struct Page
{
    Header header;
    QList<Row> rows;
};

Page build(qint64 collectionId,
           int order,
           const QList<CollectionRecord> &collections,
           const QHash<qint64, qint64> &collectionByMedia,
           const QList<MediaRecord> &films,
           const QList<UniverseCatalog::Universe> &universes,
           const QDate &today,
           const QHash<qint64, FilmDetailsRecord> &details = {},
           const QList<CustomCollectionRecord> &custom = {},
           const QHash<QString, QSet<qint64>> &hiddenFilms = {},
           const QHash<QString, QSet<qint64>> &addedFilms = {});

Page buildCustom(qint64 collectionId,
                 const QList<MediaRecord> &films,
                 const QList<CustomCollectionRecord> &custom);

QList<qint64> filmsNotInLibrary(const QList<CollectionRecord> &ownedCollections,
                                const QHash<qint64, qint64> &collectionByOwnedFilm,
                                const QList<UniverseCatalog::Universe> &universes,
                                const QDate &today);

}
