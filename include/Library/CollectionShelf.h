#pragma once

#include "Data/MediaRepository.h"
#include "Library/UniverseCatalog.h"

#include <QDate>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace CollectionShelf {

enum class State {
    NotStarted,
    InProgress,
    Completed
};

enum Filter {
    All = 0,
    InProgress = 1,
    Completed = 2,
    NotStarted = 3
};

constexpr qint64 kCustomIdBase = -1000000000;

inline qint64 customId(qint64 customCollectionId)
{
    return kCustomIdBase - customCollectionId;
}

inline bool isCustom(qint64 id)
{
    return id <= kCustomIdBase;
}

inline QString universeScope(const QString &key)
{
    return QStringLiteral("universe:") + key;
}

inline QString collectionScope(qint64 tmdbId)
{
    return QStringLiteral("collection:") + QString::number(tmdbId);
}

inline qint64 customCollectionIdOf(qint64 id)
{
    return kCustomIdBase - id;
}

struct Summary
{
    qint64 id = 0;
    bool universe = false;
    bool custom = false;
    QString name;
    QString posterPath;
    QString backdropPath;
    QStringList behindPosters;
    int total = 0;
    int owned = 0;
    int missing = 0;
    int watched = 0;
    int firstYear = 0;
    int lastYear = 0;
    State state = State::NotStarted;

    double progress() const { return total > 0 ? double(watched) / double(total) : 0.0; }

    bool operator==(const Summary &other) const
    {
        return id == other.id
            && universe == other.universe
            && custom == other.custom
            && name == other.name
            && posterPath == other.posterPath
            && backdropPath == other.backdropPath
            && behindPosters == other.behindPosters
            && total == other.total
            && owned == other.owned
            && missing == other.missing
            && watched == other.watched
            && firstYear == other.firstYear
            && lastYear == other.lastYear
            && state == other.state;
    }

    bool operator!=(const Summary &other) const { return !(*this == other); }
};

QList<Summary> build(const QList<CollectionRecord> &collections,
                     const QHash<qint64, qint64> &collectionByMedia,
                     const QList<MediaRecord> &films,
                     const QDate &today,
                     const QList<UniverseCatalog::Universe> &universes = {},
                     const QList<CustomCollectionRecord> &custom = {},
                     const QSet<qint64> &hidden = {},
                     const QHash<QString, QSet<qint64>> &hiddenFilms = {},
                     const QHash<QString, QSet<qint64>> &addedFilms = {});

QList<Summary> filtered(const QList<Summary> &summaries, int filter);

QString yearsText(int firstYear, int lastYear);

}
