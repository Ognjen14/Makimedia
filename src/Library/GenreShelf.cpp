#include "Library/GenreShelf.h"

#include "TextFold.h"

#include <algorithm>

namespace GenreShelf {

namespace {

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
