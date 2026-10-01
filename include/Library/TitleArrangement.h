#pragma once

#include "Data/MediaRepository.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace TitleArrangement {

enum Sort {
    TitleAscending = 0,
    RecentlyAdded = 1,
    YearNewest = 2,
    YearOldest = 3,
    Rating = 4,
    Shortest = 5,
    Longest = 6
};

enum Filter {
    Everything = 0,
    Unwatched = 1,
    Watched = 2,
    InProgress = 3
};

bool isWatched(const MediaRecord &title);
bool isStarted(const MediaRecord &title);
bool keeps(int filter, const MediaRecord &title);

QList<MediaRecord> arrange(const QList<MediaRecord> &titles,
                           int sort,
                           int filter,
                           const QString &genre,
                           const QHash<qint64, qint64> &lastAdded);

QStringList genresIn(const QList<MediaRecord> &titles);

}
