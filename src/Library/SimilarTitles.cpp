#include "Library/SimilarTitles.h"

#include "Library/GenreShelf.h"

#include <algorithm>
#include <cstdlib>

namespace SimilarTitles {

namespace {

constexpr double kGenreWeight = 6.0;
constexpr double kSameCollection = 5.0;
constexpr double kPerDirector = 3.0;
constexpr double kDirectorCap = 6.0;
constexpr double kPerActor = 1.5;
constexpr double kActorCap = 4.5;
constexpr double kNearYear = 1.5;
constexpr int kYearSpan = 20;
constexpr double kSameKind = 1.0;
constexpr double kRatingWeight = 0.5;

constexpr double kEnough = 3.5;

int sharedNames(const QStringList &a, const QStringList &b)
{
    int shared = 0;
    for (const QString &name : a) {
        if (!name.isEmpty() && b.contains(name)) {
            ++shared;
        }
    }
    return shared;
}

}

double score(const MediaRecord &seed, const MediaRecord &other, const Known &known)
{
    const QStringList seedGenres = GenreShelf::genresOf(seed.genres);
    const QStringList otherGenres = GenreShelf::genresOf(other.genres);
    const int sharedGenres = sharedNames(seedGenres, otherGenres);
    const int allGenres = int(seedGenres.size() + otherGenres.size()) - sharedGenres;

    const qint64 seedCollection = known.collections.value(seed.id);
    const bool sameCollection =
        seedCollection > 0 && seedCollection == known.collections.value(other.id);

    const People seedPeople = known.people.value(seed.id);
    const People otherPeople = known.people.value(other.id);
    const int sharedDirectors = sharedNames(seedPeople.directors, otherPeople.directors);
    const int sharedActors = sharedNames(seedPeople.cast, otherPeople.cast);

    if (sharedGenres == 0 && !sameCollection && sharedDirectors == 0) {
        return 0.0;
    }

    double total = 0.0;
    if (allGenres > 0) {
        total += kGenreWeight * double(sharedGenres) / double(allGenres);
    }
    if (sameCollection) {
        total += kSameCollection;
    }
    total += std::min(kDirectorCap, kPerDirector * sharedDirectors);
    total += std::min(kActorCap, kPerActor * sharedActors);
    if (seed.year > 0 && other.year > 0) {
        const int apart = std::abs(seed.year - other.year);
        total += kNearYear * std::max(0.0, 1.0 - double(apart) / double(kYearSpan));
    }
    if (seed.kind == other.kind) {
        total += kSameKind;
    }
    total += kRatingWeight * std::clamp(other.rating, 0.0, 10.0) / 10.0;
    return total;
}

QList<MediaRecord> similarTo(const MediaRecord &seed,
                             const QList<MediaRecord> &titles,
                             const Known &known,
                             int limit)
{
    QList<std::pair<double, MediaRecord>> scored;
    for (const MediaRecord &title : titles) {
        if (title.id == seed.id || GenreShelf::finished(title) || GenreShelf::started(title)) {
            continue;
        }
        const double value = score(seed, title, known);
        if (value >= kEnough) {
            scored.append({value, title});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
        return a.first != b.first ? a.first > b.first : a.second.id < b.second.id;
    });

    QList<MediaRecord> result;
    for (int i = 0; i < scored.size() && i < limit; ++i) {
        result.append(scored.at(i).second);
    }
    return result;
}

}
