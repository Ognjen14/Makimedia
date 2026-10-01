#include "Library/CollectionPage.h"

#include <QSet>

#include <algorithm>
#include <cmath>

namespace CollectionPage {

namespace {

struct Entry
{
    qint64 tmdbId = 0;
    QString title;
    QDate released;
    QString posterPath;
    QString phase;
};

int yearOf(const Entry &entry, const MediaRecord *film)
{
    if (film && film->year > 0) {
        return film->year;
    }
    return entry.released.isValid() ? entry.released.year() : 0;
}

QString tagFor(bool universe, int total)
{
    if (universe) {
        return QStringLiteral("universe");
    }
    if (total == 2) {
        return QStringLiteral("duology");
    }
    if (total == 3) {
        return QStringLiteral("trilogy");
    }
    return QStringLiteral("collection");
}

}

int Row::leftMinutes() const
{
    if (heading || watched || runtimeMinutes <= 0) {
        return 0;
    }
    return int(std::lround(runtimeMinutes * (1.0 - qBound(0.0, progress, 1.0))));
}

Page build(qint64 collectionId,
           int order,
           const QList<CollectionRecord> &collections,
           const QHash<qint64, qint64> &collectionByMedia,
           const QList<MediaRecord> &films,
           const QList<UniverseCatalog::Universe> &universes,
           const QDate &today,
           const QHash<qint64, FilmDetailsRecord> &details,
           const QList<CustomCollectionRecord> &custom,
           const QHash<QString, QSet<qint64>> &hiddenFilms,
           const QHash<QString, QSet<qint64>> &addedFilms)
{
    Page page;
    if (collectionId == 0) {
        return page;
    }

    if (CollectionShelf::isCustom(collectionId)) {
        return buildCustom(collectionId, films, custom);
    }

    const UniverseCatalog::Universe *universe = nullptr;
    const CollectionRecord *collection = nullptr;
    if (collectionId < 0) {
        const qint64 index = -collectionId - 1;
        if (index >= universes.size()) {
            return page;
        }
        universe = &universes.at(int(index));
    } else {
        for (const CollectionRecord &candidate : collections) {
            if (candidate.tmdbId == collectionId) {
                collection = &candidate;
                break;
            }
        }
        if (!collection) {
            return page;
        }
    }

    const QString scope = universe ? CollectionShelf::universeScope(universe->key)
                                   : CollectionShelf::collectionScope(collectionId);
    const QSet<qint64> putIn = addedFilms.value(scope);

    QHash<qint64, const MediaRecord *> ownedByTmdb;
    for (const MediaRecord &film : films) {
        if (film.kind != QLatin1String("movie")) {
            continue;
        }
        const qint64 filmCollection = collectionByMedia.value(film.id, 0);
        const bool belongs = putIn.contains(film.tmdbId)
            || (universe ? universe->contains(film.tmdbId, filmCollection,
                                              film.title, film.originalTitle,
                                              film.genres)
                         : filmCollection == collectionId);
        if (belongs && !ownedByTmdb.contains(film.tmdbId)) {
            ownedByTmdb.insert(film.tmdbId, &film);
        }
    }

    QList<Entry> entries;
    Header &header = page.header;
    header.valid = true;

    if (universe) {
        header.universe = true;
        header.hasStoryOrder = true;
        header.name = universe->name;
        header.description = universe->description;
        header.backdropPath = universe->backdropPath;

        QHash<qint64, QString> partPosters;
        for (const CollectionRecord &candidate : collections) {
            if (!universe->collections.contains(candidate.tmdbId)) {
                continue;
            }
            if (header.backdropPath.isEmpty()) {
                header.backdropPath = candidate.backdropPath;
            }
            for (const CollectionPartRecord &part : candidate.parts) {
                partPosters.insert(part.tmdbId, part.posterPath);
            }
        }

        for (const UniverseCatalog::Film &film : universe->storyOrder) {
            entries.append({film.tmdbId, film.title, film.released,
                            partPosters.value(film.tmdbId), film.phase});
        }
    } else {
        header.name = collection->name;
        header.description = collection->overview;
        header.backdropPath = collection->backdropPath;
        for (const CollectionPartRecord &part : collection->parts) {
            entries.append({part.tmdbId, part.title,
                            QDate::fromString(part.releaseDate, Qt::ISODate),
                            part.posterPath, QString()});
        }
    }

    const QSet<qint64> takenOut = hiddenFilms.value(scope);

    QSet<qint64> listed;
    QList<Entry> kept;
    for (const Entry &entry : std::as_const(entries)) {
        const bool out = entry.released.isValid() && entry.released <= today;
        if (listed.contains(entry.tmdbId) || (!out && !ownedByTmdb.contains(entry.tmdbId))
            || (takenOut.contains(entry.tmdbId) && !ownedByTmdb.contains(entry.tmdbId))) {
            continue;
        }
        listed.insert(entry.tmdbId);
        kept.append(entry);
    }
    for (const MediaRecord &film : films) {
        if (ownedByTmdb.value(film.tmdbId) == &film && !listed.contains(film.tmdbId)) {
            listed.insert(film.tmdbId);
            kept.append({film.tmdbId, film.title,
                         film.year > 0 ? QDate(film.year, 1, 1) : QDate(), film.posterPath,
                         QString()});
        }
    }

    const bool story = universe && order == Story && !universe->storyOrder.isEmpty();
    if (!story) {
        std::stable_sort(kept.begin(), kept.end(), [](const Entry &a, const Entry &b) {
            if (a.released.isValid() != b.released.isValid()) {
                return a.released.isValid();
            }
            return a.released.isValid() && a.released < b.released;
        });
    }

    QString lastPhase;
    int headings = 0;
    int number = 0;
    double ratingSum = 0.0;
    int rated = 0;
    const MediaRecord *firstUnwatched = nullptr;
    const MediaRecord *firstStarted = nullptr;

    for (const Entry &entry : std::as_const(kept)) {
        if (story && !entry.phase.isEmpty() && entry.phase != lastPhase) {
            Row heading;
            heading.heading = true;
            heading.key = QStringLiteral("phase:%1:%2").arg(++headings).arg(entry.phase);
            heading.phase = entry.phase;
            page.rows.append(heading);
            lastPhase = entry.phase;
        }

        const MediaRecord *film = ownedByTmdb.value(entry.tmdbId, nullptr);

        Row row;
        row.key = QStringLiteral("film:%1").arg(entry.tmdbId);
        row.number = ++number;
        row.tmdbId = entry.tmdbId;
        row.year = yearOf(entry, film);
        row.added = putIn.contains(entry.tmdbId);
        if (film) {
            row.owned = true;
            row.title = film->title.isEmpty() ? entry.title : film->title;
            row.runtimeMinutes = film->runtimeMinutes;
            row.rating = film->rating;
            row.posterPath = film->posterPath.isEmpty() ? entry.posterPath : film->posterPath;
            row.handle = film->firstFileHandle;
            row.watched = film->fileCount > 0 && film->watchedCount >= film->fileCount;
            row.progress = row.watched ? 1.0 : qBound(0.0, film->partialProgress, 1.0);

            ++header.owned;
            header.ownedHandles.append(row.handle);
            if (row.watched) {
                ++header.watched;
            } else {
                if (!firstUnwatched) {
                    firstUnwatched = film;
                }
                if (!firstStarted && row.progress > 0.0) {
                    firstStarted = film;
                }
            }
            if (header.backdropPath.isEmpty()) {
                header.backdropPath = film->backdropPath;
            }
        } else {
            const auto known = details.constFind(entry.tmdbId);
            const bool hasDetails = known != details.constEnd();
            row.title = hasDetails && !known->title.isEmpty() ? known->title : entry.title;
            row.posterPath = hasDetails && !known->posterPath.isEmpty() ? known->posterPath
                                                                         : entry.posterPath;
            if (hasDetails) {
                row.runtimeMinutes = known->runtimeMinutes;
                row.rating = known->rating;
            }
        }

        header.totalMinutes += row.runtimeMinutes;
        header.leftMinutes += row.leftMinutes();
        if (row.rating > 0.0) {
            ratingSum += row.rating;
            ++rated;
        }
        if (row.year > 0) {
            if (header.firstYear <= 0 || row.year < header.firstYear) {
                header.firstYear = row.year;
            }
            header.lastYear = std::max(header.lastYear, row.year);
        }
        if (header.posters.size() < 6 && !row.posterPath.isEmpty()) {
            header.posters.append(row.posterPath);
        }

        ++header.total;
        page.rows.append(row);
    }

    header.averageRating = rated > 0 ? ratingSum / rated : 0.0;
    header.tag = tagFor(header.universe, header.total);
    header.allOwnedWatched = header.owned > 0 && header.watched >= header.owned;

    const MediaRecord *next = firstStarted ? firstStarted : firstUnwatched;
    if (next) {
        header.nextTitle = next->title;
        header.nextHandle = next->firstFileHandle;
        header.nextResumes = next == firstStarted;
        for (const Row &row : std::as_const(page.rows)) {
            if (!row.heading && row.tmdbId == next->tmdbId) {
                header.nextLeftMinutes = row.leftMinutes();
            }
        }
    }

    return page;
}

Page buildCustom(qint64 collectionId,
                 const QList<MediaRecord> &films,
                 const QList<CustomCollectionRecord> &custom)
{
    Page page;

    const qint64 wanted = CollectionShelf::customCollectionIdOf(collectionId);
    const CustomCollectionRecord *made = nullptr;
    for (const CustomCollectionRecord &candidate : custom) {
        if (candidate.id == wanted) {
            made = &candidate;
            break;
        }
    }
    if (!made) {
        return page;
    }

    QHash<qint64, const MediaRecord *> filmsById;
    for (const MediaRecord &film : films) {
        filmsById.insert(film.id, &film);
    }

    Header &header = page.header;
    header.valid = true;
    header.custom = true;
    header.name = made->name;
    header.description = made->description;
    header.tag = QStringLiteral("custom");

    QList<const MediaRecord *> owned;
    double ratingSum = 0.0;
    int rated = 0;
    const MediaRecord *firstUnwatched = nullptr;
    const MediaRecord *firstStarted = nullptr;
    int number = 0;

    for (const qint64 mediaId : made->mediaIds) {
        const MediaRecord *film = filmsById.value(mediaId, nullptr);
        if (!film) {
            continue;
        }
        owned.append(film);

        Row row;
        row.key = QStringLiteral("film:%1").arg(film->id);
        row.show = film->kind == QLatin1String("tv");
        row.number = ++number;
        row.tmdbId = film->tmdbId;
        row.title = film->title;
        row.year = film->year;
        row.runtimeMinutes = row.show ? 0 : film->runtimeMinutes;
        row.rating = film->rating;
        row.posterPath = film->posterPath;
        row.owned = true;
        row.handle = film->firstFileHandle;
        row.watched = film->fileCount > 0 && film->watchedCount >= film->fileCount;
        row.progress = row.watched ? 1.0 : qBound(0.0, film->partialProgress, 1.0);

        ++header.owned;
        ++header.total;
        header.ownedHandles.append(row.handle);
        if (row.watched) {
            ++header.watched;
        } else {
            if (!firstUnwatched) {
                firstUnwatched = film;
            }
            if (!firstStarted && row.progress > 0.0) {
                firstStarted = film;
            }
        }
        if (header.backdropPath.isEmpty()) {
            header.backdropPath = film->backdropPath;
        }
        header.totalMinutes += row.runtimeMinutes;
        header.leftMinutes += row.leftMinutes();
        if (row.rating > 0.0) {
            ratingSum += row.rating;
            ++rated;
        }
        if (row.year > 0) {
            if (header.firstYear <= 0 || row.year < header.firstYear) {
                header.firstYear = row.year;
            }
            header.lastYear = std::max(header.lastYear, row.year);
        }
        if (header.posters.size() < 6 && !row.posterPath.isEmpty()) {
            header.posters.append(row.posterPath);
        }

        page.rows.append(row);
    }

    header.averageRating = rated > 0 ? ratingSum / rated : 0.0;
    header.allOwnedWatched = header.owned > 0 && header.watched >= header.owned;

    const MediaRecord *next = firstStarted ? firstStarted : firstUnwatched;
    if (next) {
        header.nextTitle = next->title;
        header.nextHandle = next->firstFileHandle;
        header.nextResumes = next == firstStarted;
        for (const Row &row : std::as_const(page.rows)) {
            if (row.tmdbId == next->tmdbId) {
                header.nextLeftMinutes = row.leftMinutes();
            }
        }
    }

    return page;
}

QList<qint64> filmsNotInLibrary(const QList<CollectionRecord> &ownedCollections,
                                const QHash<qint64, qint64> &collectionByOwnedFilm,
                                const QList<UniverseCatalog::Universe> &universes,
                                const QDate &today)
{
    QList<qint64> result;
    QSet<qint64> seen;

    const auto consider = [&](qint64 tmdbId, const QDate &released) {
        if (tmdbId <= 0 || seen.contains(tmdbId) || collectionByOwnedFilm.contains(tmdbId)) {
            return;
        }
        if (!released.isValid() || released > today) {
            return;
        }
        seen.insert(tmdbId);
        result.append(tmdbId);
    };

    for (const CollectionRecord &collection : ownedCollections) {
        for (const CollectionPartRecord &part : collection.parts) {
            consider(part.tmdbId, QDate::fromString(part.releaseDate, Qt::ISODate));
        }
    }

    for (const UniverseCatalog::Universe &universe : universes) {
        bool owned = false;
        for (auto film = collectionByOwnedFilm.cbegin(); film != collectionByOwnedFilm.cend(); ++film) {
            if (universe.contains(film.key(), film.value())) {
                owned = true;
                break;
            }
        }
        if (!owned) {
            continue;
        }
        for (const UniverseCatalog::Film &film : universe.storyOrder) {
            consider(film.tmdbId, film.released);
        }
    }

    return result;
}

}
