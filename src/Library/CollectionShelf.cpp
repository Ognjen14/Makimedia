#include "Library/CollectionShelf.h"

#include "TextFold.h"

#include <QSet>

#include <algorithm>

namespace CollectionShelf {

namespace {

struct Entry
{
    qint64 tmdbId = 0;
    QDate released;
    QString posterPath;
};

void widenYears(Summary &summary, int year)
{
    if (year <= 0) {
        return;
    }
    if (summary.firstYear <= 0 || year < summary.firstYear) {
        summary.firstYear = year;
    }
    if (year > summary.lastYear) {
        summary.lastYear = year;
    }
}

void addBehind(Summary &summary, const QString &posterPath)
{
    constexpr int kBehind = 2;
    if (summary.behindPosters.size() >= kBehind || posterPath.isEmpty()
        || posterPath == summary.posterPath || summary.behindPosters.contains(posterPath)) {
        return;
    }
    summary.behindPosters.append(posterPath);
}

void fill(Summary &summary,
          const QList<const MediaRecord *> &owned,
          const QList<Entry> &entries,
          const QDate &today,
          bool ownedPostersFirst)
{
    summary.owned = int(owned.size());

    QSet<qint64> ownedTmdb;
    bool anyProgress = false;
    for (const MediaRecord *film : owned) {
        ownedTmdb.insert(film->tmdbId);
        if (film->fileCount > 0 && film->watchedCount >= film->fileCount) {
            ++summary.watched;
        } else if (film->partialProgress > 0.0) {
            anyProgress = true;
        }
    }

    if (ownedPostersFirst) {
        for (const Entry &entry : entries) {
            if (!ownedTmdb.contains(entry.tmdbId)) {
                continue;
            }
            for (const MediaRecord *film : owned) {
                if (film->tmdbId == entry.tmdbId) {
                    addBehind(summary, film->posterPath);
                }
            }
        }
    }

    QSet<qint64> counted;
    for (const Entry &entry : entries) {
        const bool out = entry.released.isValid() && entry.released <= today;
        if (!out && !ownedTmdb.contains(entry.tmdbId)) {
            continue;
        }
        counted.insert(entry.tmdbId);
        if (entry.released.isValid()) {
            widenYears(summary, entry.released.year());
        }
        addBehind(summary, entry.posterPath);
    }

    for (const MediaRecord *film : owned) {
        if (!counted.contains(film->tmdbId)) {
            counted.insert(film->tmdbId);
            widenYears(summary, film->year);
        }
        addBehind(summary, film->posterPath);
    }

    summary.total = int(counted.size());
    summary.missing = std::max(0, summary.total - summary.owned);

    if (summary.total > 0 && summary.watched >= summary.total) {
        summary.state = State::Completed;
    } else if (summary.watched > 0 || anyProgress) {
        summary.state = State::InProgress;
    } else {
        summary.state = State::NotStarted;
    }
}

}

QList<Summary> build(const QList<CollectionRecord> &collections,
                     const QHash<qint64, qint64> &collectionByMedia,
                     const QList<MediaRecord> &films,
                     const QDate &today,
                     const QList<UniverseCatalog::Universe> &universes,
                     const QList<CustomCollectionRecord> &custom,
                     const QSet<qint64> &hidden,
                     const QHash<QString, QSet<qint64>> &hiddenFilms,
                     const QHash<QString, QSet<qint64>> &addedFilms)
{
    QHash<qint64, QList<const MediaRecord *>> filmsByCollection;
    for (const MediaRecord &film : films) {
        if (film.kind != QLatin1String("movie")) {
            continue;
        }
        const qint64 collectionId = collectionByMedia.value(film.id, 0);
        if (collectionId > 0) {
            filmsByCollection[collectionId].append(&film);
        }
    }

    QHash<qint64, const CollectionRecord *> collectionsById;
    for (const CollectionRecord &collection : collections) {
        collectionsById.insert(collection.tmdbId, &collection);
    }

    QList<Summary> result;
    result.reserve(collections.size() + universes.size());

    QSet<qint64> claimed;
    for (int index = 0; index < universes.size(); ++index) {
        const UniverseCatalog::Universe &universe = universes.at(index);
        claimed.unite(universe.collections);

        const QSet<qint64> putIn = addedFilms.value(universeScope(universe.key));
        QList<const MediaRecord *> owned;
        for (const MediaRecord &film : films) {
            if (film.kind != QLatin1String("movie")) {
                continue;
            }
            if (putIn.contains(film.tmdbId)
                || universe.contains(film.tmdbId, collectionByMedia.value(film.id, 0),
                                     film.title, film.originalTitle, film.genres)) {
                owned.append(&film);
            }
        }
        if (owned.isEmpty()) {
            continue;
        }

        QHash<qint64, QString> partPosters;
        for (const qint64 collectionId : universe.collections) {
            const CollectionRecord *collection = collectionsById.value(collectionId, nullptr);
            if (!collection) {
                continue;
            }
            for (const CollectionPartRecord &part : collection->parts) {
                partPosters.insert(part.tmdbId, part.posterPath);
            }
        }

        const QSet<qint64> takenOut = hiddenFilms.value(universeScope(universe.key));
        QList<Entry> entries;
        entries.reserve(universe.storyOrder.size());
        for (const UniverseCatalog::Film &film : universe.storyOrder) {
            if (takenOut.contains(film.tmdbId)) {
                continue;
            }
            entries.append({film.tmdbId, film.released, partPosters.value(film.tmdbId)});
        }

        Summary summary;
        summary.id = -(index + 1);
        summary.universe = true;
        summary.name = universe.name;
        summary.posterPath = universe.posterPath;
        fill(summary, owned, entries, today, true);
        result.append(summary);
    }

    for (const CollectionRecord &collection : collections) {
        if (claimed.contains(collection.tmdbId) || collection.parts.isEmpty()) {
            continue;
        }
        QList<const MediaRecord *> owned = filmsByCollection.value(collection.tmdbId);

        const QSet<qint64> putIn = addedFilms.value(collectionScope(collection.tmdbId));
        if (!putIn.isEmpty()) {
            for (const MediaRecord &film : films) {
                if (film.kind == QLatin1String("movie") && putIn.contains(film.tmdbId)
                    && !owned.contains(&film)) {
                    owned.append(&film);
                }
            }
        }

        if (owned.isEmpty()) {
            continue;
        }

        const QSet<qint64> takenOut = hiddenFilms.value(collectionScope(collection.tmdbId));
        QList<Entry> entries;
        entries.reserve(collection.parts.size());
        for (const CollectionPartRecord &part : collection.parts) {
            if (takenOut.contains(part.tmdbId)) {
                continue;
            }
            entries.append({part.tmdbId, QDate::fromString(part.releaseDate, Qt::ISODate),
                            part.posterPath});
        }

        Summary summary;
        summary.id = collection.tmdbId;
        summary.name = collection.name;
        summary.posterPath = collection.posterPath;
        summary.backdropPath = collection.backdropPath;
        fill(summary, owned, entries, today, false);
        if (summary.total < 2) {
            continue;
        }
        result.append(summary);
    }

    QHash<qint64, const MediaRecord *> filmsById;
    for (const MediaRecord &film : films) {
        filmsById.insert(film.id, &film);
    }

    for (const CustomCollectionRecord &made : custom) {
        QList<const MediaRecord *> owned;
        for (const qint64 mediaId : made.mediaIds) {
            const MediaRecord *film = filmsById.value(mediaId, nullptr);
            if (film) {
                owned.append(film);
            }
        }
        if (owned.isEmpty()) {
            continue;
        }

        Summary summary;
        summary.id = customId(made.id);
        summary.custom = true;
        summary.name = made.name;
        if (made.coverMode == QLatin1String("poster") && !owned.first()->posterPath.isEmpty()) {
            summary.posterPath = owned.first()->posterPath;
        }
        fill(summary, owned, {}, today, true);
        result.append(summary);
    }

    if (!hidden.isEmpty()) {
        result.removeIf([&hidden](const Summary &summary) {
            return hidden.contains(summary.id);
        });
    }

    std::sort(result.begin(), result.end(), [](const Summary &a, const Summary &b) {
        const int order = TextFold::compare(a.name, b.name);
        if (order != 0) {
            return order < 0;
        }
        return a.id < b.id;
    });

    return result;
}

QList<Summary> filtered(const QList<Summary> &summaries, int filter)
{
    if (filter == All) {
        return summaries;
    }

    const State wanted = filter == InProgress ? State::InProgress
                       : filter == Completed ? State::Completed
                                             : State::NotStarted;

    QList<Summary> result;
    for (const Summary &summary : summaries) {
        if (summary.state == wanted) {
            result.append(summary);
        }
    }
    return result;
}

QString yearsText(int firstYear, int lastYear)
{
    if (firstYear <= 0) {
        return QString();
    }
    if (lastYear <= firstYear) {
        return QString::number(firstYear);
    }
    const QString last = firstYear / 100 == lastYear / 100
        ? QStringLiteral("%1").arg(lastYear % 100, 2, 10, QLatin1Char('0'))
        : QString::number(lastYear);
    return QString::number(firstYear) + QChar(0x2013) + last;
}

}
