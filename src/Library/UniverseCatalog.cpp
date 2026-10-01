#include "Library/UniverseCatalog.h"

#include "MmLog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace UniverseCatalog {

namespace {

QString fold(const QString &text)
{
    QString folded;
    folded.reserve(text.size());
    for (const QChar character : text) {
        if (character.isLetterOrNumber()) {
            folded.append(character.toLower());
        }
    }
    return folded;
}

bool anyContains(const QStringList &needles, const QString &first, const QString &second)
{
    for (const QString &needle : needles) {
        if (first.contains(needle) || second.contains(needle)) {
            return true;
        }
    }
    return false;
}

}

bool Universe::contains(qint64 filmTmdbId,
                        qint64 collectionId,
                        const QString &title,
                        const QString &originalTitle,
                        const QString &genres) const
{
    const QString folded = fold(title);
    const QString foldedOriginal = fold(originalTitle);

    if (!titleExcludes.isEmpty() && anyContains(titleExcludes, folded, foldedOriginal)) {
        return false;
    }
    if (!excludeGenres.isEmpty()) {
        const QString foldedGenres = fold(genres);
        for (const QString &genre : excludeGenres) {
            if (foldedGenres.contains(genre)) {
                return false;
            }
        }
    }
    if (collectionId > 0 && collections.contains(collectionId)) {
        return true;
    }
    for (const Film &film : storyOrder) {
        if (film.tmdbId == filmTmdbId) {
            return true;
        }
    }
    return !titleKeywords.isEmpty() && anyContains(titleKeywords, folded, foldedOriginal);
}

QList<Universe> parse(const QByteArray &json, QString *error)
{
    QList<Universe> result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return result;
    }

    const QJsonArray universes = document.object().value(QStringLiteral("universes")).toArray();
    QSet<QString> keys;
    for (const QJsonValue &value : universes) {
        const QJsonObject object = value.toObject();

        Universe universe;
        universe.key = object.value(QStringLiteral("key")).toString();
        universe.name = object.value(QStringLiteral("name")).toString();
        universe.posterPath = object.value(QStringLiteral("poster")).toString();
        universe.backdropPath = object.value(QStringLiteral("backdrop")).toString();
        universe.description = object.value(QStringLiteral("description")).toString();
        if (universe.key.isEmpty() || universe.name.isEmpty() || keys.contains(universe.key)) {
            continue;
        }

        for (const QJsonValue &collection : object.value(QStringLiteral("collections")).toArray()) {
            const qint64 id = collection.toInteger();
            if (id > 0) {
                universe.collections.insert(id);
            }
        }

        for (const QJsonValue &keyword : object.value(QStringLiteral("titleKeywords")).toArray()) {
            const QString folded = fold(keyword.toString());
            if (!folded.isEmpty() && !universe.titleKeywords.contains(folded)) {
                universe.titleKeywords.append(folded);
            }
        }

        for (const QJsonValue &keyword : object.value(QStringLiteral("titleExcludes")).toArray()) {
            const QString folded = fold(keyword.toString());
            if (!folded.isEmpty() && !universe.titleExcludes.contains(folded)) {
                universe.titleExcludes.append(folded);
            }
        }

        for (const QJsonValue &genre : object.value(QStringLiteral("excludeGenres")).toArray()) {
            const QString folded = fold(genre.toString());
            if (!folded.isEmpty() && !universe.excludeGenres.contains(folded)) {
                universe.excludeGenres.append(folded);
            }
        }

        QSet<qint64> seen;
        for (const QJsonValue &entry : object.value(QStringLiteral("storyOrder")).toArray()) {
            const QJsonObject filmObject = entry.toObject();
            Film film;
            film.tmdbId = filmObject.value(QStringLiteral("film")).toInteger();
            film.title = filmObject.value(QStringLiteral("title")).toString();
            film.released = QDate::fromString(
                filmObject.value(QStringLiteral("released")).toString(), Qt::ISODate);
            film.phase = filmObject.value(QStringLiteral("phase")).toString();
            if (film.tmdbId <= 0 || seen.contains(film.tmdbId)) {
                continue;
            }
            seen.insert(film.tmdbId);
            universe.storyOrder.append(film);
        }

        keys.insert(universe.key);
        result.append(universe);
    }

    return result;
}

namespace {

QStringList ownedPaths(const QList<Universe> &universes,
                       const QHash<qint64, qint64> &collectionByOwnedFilm,
                       QString Universe::*path)
{
    QStringList paths;
    for (const Universe &universe : universes) {
        const QString &value = universe.*path;
        if (value.isEmpty() || paths.contains(value)) {
            continue;
        }
        for (auto film = collectionByOwnedFilm.cbegin(); film != collectionByOwnedFilm.cend(); ++film) {
            if (universe.contains(film.key(), film.value())) {
                paths.append(value);
                break;
            }
        }
    }
    return paths;
}

}

QStringList ownedPosters(const QList<Universe> &universes,
                         const QHash<qint64, qint64> &collectionByOwnedFilm)
{
    return ownedPaths(universes, collectionByOwnedFilm, &Universe::posterPath);
}

QStringList ownedBackdrops(const QList<Universe> &universes,
                           const QHash<qint64, qint64> &collectionByOwnedFilm)
{
    return ownedPaths(universes, collectionByOwnedFilm, &Universe::backdropPath);
}

QList<Universe> load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        MM_LOG_W() << "could not open the universes file" << path << file.errorString();
        return {};
    }

    QString error;
    const QList<Universe> universes = parse(file.readAll(), &error);
    if (!error.isEmpty()) {
        MM_LOG_E() << "the universes file" << path << "is not valid JSON:" << error;
        return {};
    }

    int films = 0;
    for (const Universe &universe : universes) {
        films += int(universe.storyOrder.size());
    }
    MM_LOG_I() << "universes loaded:" << universes.size() << "with" << films << "films";
    return universes;
}

}
