#pragma once

#include "Data/FileRepository.h"

#include <QList>

namespace BrowseOptions {

enum Sort {
    RecentlyAdded = 0,
    Title,
    LastPlayed,
    FileSize
};

enum Filter {
    Everything = 0,
    Unwatched,
    InProgress,
    Watched,
    Unmatched,
    Suggested
};

bool isFinished(const LibraryFile &file);
bool isStarted(const LibraryFile &file);

bool keeps(int filter, const LibraryFile &file);
bool isBefore(int sort, const LibraryFile &a, const LibraryFile &b);

QList<LibraryFile> apply(const QList<LibraryFile> &files, int sort, int filter);

}
