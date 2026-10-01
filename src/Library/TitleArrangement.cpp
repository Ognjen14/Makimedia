#include "Library/TitleArrangement.h"

#include "Library/GenreShelf.h"
#include "TextFold.h"

#include <algorithm>

namespace TitleArrangement {

namespace {

int lengthOf(const MediaRecord &title)
{
    return title.kind == QLatin1String("tv") ? title.fileCount : title.runtimeMinutes;
}

bool byTitle(const MediaRecord &a, const MediaRecord &b)
{
    const int order = TextFold::compare(a.title, b.title);
    return order != 0 ? order < 0 : a.id < b.id;
}

bool knownFirst(bool knownA, bool knownB, bool &decided)
{
    decided = knownA != knownB;
    return knownA;
}

}

bool isWatched(const MediaRecord &title)
{
    return title.fileCount > 0 && title.watchedCount >= title.fileCount;
}

bool isStarted(const MediaRecord &title)
{
    return title.watchedCount > 0 || title.partialProgress > 0.0;
}

bool keeps(int filter, const MediaRecord &title)
{
    switch (filter) {
    case Unwatched:
        return !isStarted(title);
    case Watched:
        return isWatched(title);
    case InProgress:
        return isStarted(title) && !isWatched(title);
    case Everything:
    default:
        return true;
    }
}

QStringList genresIn(const QList<MediaRecord> &titles)
{
    QStringList genres;
    for (const MediaRecord &title : titles) {
        for (const QString &genre : GenreShelf::genresOf(title.genres)) {
            if (!genres.contains(genre)) {
                genres.append(genre);
            }
        }
    }
    std::sort(genres.begin(), genres.end(), [](const QString &a, const QString &b) {
        return TextFold::compare(a, b) < 0;
    });
    return genres;
}

QList<MediaRecord> arrange(const QList<MediaRecord> &titles,
                           int sort,
                           int filter,
                           const QString &genre,
                           const QHash<qint64, qint64> &lastAdded)
{
    QList<MediaRecord> result;
    result.reserve(titles.size());
    for (const MediaRecord &title : titles) {
        if (!keeps(filter, title)) {
            continue;
        }
        if (!genre.isEmpty() && !GenreShelf::genresOf(title.genres).contains(genre)) {
            continue;
        }
        result.append(title);
    }

    const auto before = [sort, &lastAdded](const MediaRecord &a, const MediaRecord &b) {
        bool decided = false;
        switch (sort) {
        case RecentlyAdded: {
            const qint64 addedA = lastAdded.value(a.id);
            const qint64 addedB = lastAdded.value(b.id);
            if (addedA != addedB) {
                return addedA > addedB;
            }
            break;
        }
        case YearNewest:
        case YearOldest: {
            const bool first = knownFirst(a.year > 0, b.year > 0, decided);
            if (decided) {
                return first;
            }
            if (a.year != b.year) {
                return sort == YearNewest ? a.year > b.year : a.year < b.year;
            }
            break;
        }
        case Rating:
            if (a.rating != b.rating) {
                return a.rating > b.rating;
            }
            break;
        case Shortest:
        case Longest: {
            const int lengthA = lengthOf(a);
            const int lengthB = lengthOf(b);
            const bool first = knownFirst(lengthA > 0, lengthB > 0, decided);
            if (decided) {
                return first;
            }
            if (lengthA != lengthB) {
                return sort == Shortest ? lengthA < lengthB : lengthA > lengthB;
            }
            break;
        }
        case TitleAscending:
        default:
            break;
        }
        return byTitle(a, b);
    };

    std::stable_sort(result.begin(), result.end(), before);
    return result;
}

}
