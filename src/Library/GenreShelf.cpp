#include "Library/GenreShelf.h"

#include "TextFold.h"

#include <QSet>

#include <algorithm>
#include <cmath>

namespace GenreShelf {

namespace {

constexpr int kYesterdaysFront = 8;
constexpr double kYesterdaysFrontPenalty = 0.25;
constexpr double kYesterdaysGenreWeight = 0.35;

quint64 mixed(quint64 value)
{
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

quint64 titleSalt(quint64 salt, qint64 id)
{
    return salt ^ (quint64(id) * 0x9E3779B97F4A7C15ULL);
}

QHash<qint64, double> freshnessOf(const QList<MediaRecord> &titles,
                                  const QHash<qint64, qint64> &lastAdded)
{
    QList<qint64> ids;
    ids.reserve(titles.size());
    for (const MediaRecord &title : titles) {
        ids.append(title.id);
    }
    std::sort(ids.begin(), ids.end(), [&lastAdded](qint64 a, qint64 b) {
        const qint64 addedA = lastAdded.value(a);
        const qint64 addedB = lastAdded.value(b);
        return addedA != addedB ? addedA > addedB : a > b;
    });

    QHash<qint64, double> freshness;
    const int last = int(ids.size()) - 1;
    for (int rank = 0; rank <= last; ++rank) {
        freshness.insert(ids.at(rank), last > 0 ? 1.0 - double(rank) / double(last) : 1.0);
    }
    return freshness;
}

QList<MediaRecord> arrangedForDay(const QList<MediaRecord> &titles,
                                  const QString &genre,
                                  const QHash<qint64, double> &freshness,
                                  const QDate &day,
                                  const QSet<qint64> &frontYesterday)
{
    const quint64 salt = saltOf(genre);

    QHash<qint64, double> score;
    score.reserve(titles.size());
    for (const MediaRecord &title : titles) {
        double value = 0.40 * std::clamp(title.rating, 0.0, 10.0) / 10.0
                       + 0.25 * freshness.value(title.id)
                       + 0.35 * dayChance(day, titleSalt(salt, title.id));
        if (frontYesterday.contains(title.id)) {
            value -= kYesterdaysFrontPenalty;
        }
        score.insert(title.id, value);
    }

    QList<MediaRecord> arranged = titles;
    std::sort(arranged.begin(), arranged.end(),
              [&score](const MediaRecord &a, const MediaRecord &b) {
        const bool doneA = finished(a);
        const bool doneB = finished(b);
        if (doneA != doneB) {
            return !doneA;
        }
        const double scoreA = score.value(a.id);
        const double scoreB = score.value(b.id);
        if (scoreA != scoreB) {
            return scoreA > scoreB;
        }
        return a.id < b.id;
    });
    return arranged;
}

QList<int> pickedGenres(const QList<Row> &rows, int count, const QDate &day,
                        const QSet<QString> &pickedYesterday)
{
    QList<std::pair<double, int>> keyed;
    keyed.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        double weight = std::sqrt(double(rows.at(i).titles.size()));
        if (pickedYesterday.contains(rows.at(i).genre)) {
            weight *= kYesterdaysGenreWeight;
        }
        const double chance = std::max(dayChance(day, saltOf(rows.at(i).genre)), 1e-12);
        keyed.append({std::pow(chance, 1.0 / weight), i});
    }

    std::sort(keyed.begin(), keyed.end(), [](const auto &a, const auto &b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });

    QList<int> picked;
    for (int i = 0; i < keyed.size() && i < count; ++i) {
        picked.append(keyed.at(i).second);
    }
    return picked;
}

QStringList unified(const QString &name)
{
    static const QHash<QString, QStringList> mapped = {
        {QStringLiteral("Action & Adventure"),
         {QStringLiteral("Action"), QStringLiteral("Adventure")}},
        {QStringLiteral("Sci-Fi & Fantasy"),
         {QStringLiteral("Science Fiction"), QStringLiteral("Fantasy")}},
        {QStringLiteral("War & Politics"), {QStringLiteral("War")}},
        {QStringLiteral("Kids"), {QStringLiteral("Family")}},
        {QStringLiteral("TV Movie"), {}},
    };

    const auto found = mapped.constFind(name);
    if (found != mapped.constEnd()) {
        return found.value();
    }
    return {name};
}

}

QStringList genresOf(const QString &stored)
{
    QStringList result;
    const QStringList parts = stored.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString name = part.trimmed();
        if (name.isEmpty()) {
            continue;
        }
        for (const QString &genre : unified(name)) {
            if (!result.contains(genre)) {
                result.append(genre);
            }
        }
    }
    return result;
}

bool finished(const MediaRecord &title)
{
    return title.fileCount > 0 && title.watchedCount >= title.fileCount;
}

bool started(const MediaRecord &title)
{
    return title.watchedCount > 0 || title.partialProgress >= 0.05;
}

quint64 saltOf(const QString &text)
{
    quint64 hash = 0xCBF29CE484222325ULL;
    for (const QChar character : text) {
        hash ^= character.unicode();
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

double dayChance(const QDate &day, quint64 salt)
{
    const quint64 value = mixed(mixed(quint64(day.toJulianDay())) ^ salt);
    return double(value >> 11) * (1.0 / 9007199254740992.0);
}

QList<Row> daily(const QList<MediaRecord> &titles,
                 const QHash<qint64, qint64> &lastAdded,
                 int smallestRow,
                 int rowsPerDay,
                 const QDate &day)
{
    const QList<Row> all = build(titles, lastAdded, smallestRow);
    if (all.isEmpty() || rowsPerDay <= 0) {
        return {};
    }

    const QDate yesterday = day.addDays(-1);
    QSet<QString> genresYesterday;
    for (const int i : pickedGenres(all, rowsPerDay, yesterday, {})) {
        genresYesterday.insert(all.at(i).genre);
    }

    const QHash<qint64, double> freshness = freshnessOf(titles, lastAdded);

    QList<Row> rows;
    for (const int i : pickedGenres(all, rowsPerDay, day, genresYesterday)) {
        const Row &row = all.at(i);

        QSet<qint64> frontYesterday;
        const QList<MediaRecord> before =
            arrangedForDay(row.titles, row.genre, freshness, yesterday, {});
        for (int at = 0; at < before.size() && at < kYesterdaysFront; ++at) {
            frontYesterday.insert(before.at(at).id);
        }

        rows.append({row.genre,
                     arrangedForDay(row.titles, row.genre, freshness, day, frontYesterday),
                     QString()});
    }
    return rows;
}

QList<MediaRecord> shortOnTime(const QList<MediaRecord> &titles,
                               int longestMinutes,
                               int limit,
                               const QDate &day)
{
    const quint64 salt = saltOf(QStringLiteral("short on time"));

    QList<std::pair<double, MediaRecord>> scored;
    for (const MediaRecord &title : titles) {
        if (title.kind != QLatin1String("movie") || title.runtimeMinutes <= 0
            || title.runtimeMinutes > longestMinutes || finished(title) || started(title)) {
            continue;
        }
        const double score = 0.5 * std::clamp(title.rating, 0.0, 10.0) / 10.0
                             + 0.5 * dayChance(day, titleSalt(salt, title.id));
        scored.append({score, title});
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

QList<Row> build(const QList<MediaRecord> &titles,
                 const QHash<qint64, qint64> &lastAdded,
                 int smallestRow)
{
    QHash<QString, int> rowOf;
    QList<Row> rows;

    for (const MediaRecord &title : titles) {
        for (const QString &genre : genresOf(title.genres)) {
            auto found = rowOf.constFind(genre);
            if (found == rowOf.constEnd()) {
                found = rowOf.insert(genre, int(rows.size()));
                rows.append({genre, {}});
            }
            rows[found.value()].titles.append(title);
        }
    }

    const auto newerFirst = [&lastAdded](const MediaRecord &a, const MediaRecord &b) {
        const qint64 addedA = lastAdded.value(a.id);
        const qint64 addedB = lastAdded.value(b.id);
        if (addedA != addedB) {
            return addedA > addedB;
        }
        const int order = TextFold::compare(a.title, b.title);
        return order != 0 ? order < 0 : a.id < b.id;
    };

    QList<Row> kept;
    for (Row &row : rows) {
        if (row.titles.size() < smallestRow) {
            continue;
        }
        std::sort(row.titles.begin(), row.titles.end(), newerFirst);
        kept.append(row);
    }

    std::sort(kept.begin(), kept.end(), [](const Row &a, const Row &b) {
        if (a.titles.size() != b.titles.size()) {
            return a.titles.size() > b.titles.size();
        }
        return TextFold::compare(a.genre, b.genre) < 0;
    });

    return kept;
}

}
