#include "Library/BrowseOptions.h"

#include "Data/PlaybackStateRepository.h"
#include "TextFold.h"

#include <algorithm>
#include <utility>

namespace {

QString shownTitleOf(const LibraryFile &file)
{
    return file.isMatched() ? file.matchedTitle : file.displayName;
}

}

namespace BrowseOptions {

bool isFinished(const LibraryFile &file)
{
    return file.playback.watched
        || PlaybackStateRepository::isFinished(file.playback.positionSeconds,
                                               file.playback.durationSeconds);
}

bool isStarted(const LibraryFile &file)
{
    return file.playback.isValid()
        && PlaybackStateRepository::resumable(file.playback.positionSeconds,
                                              file.playback.durationSeconds,
                                              file.playback.watched);
}

bool keeps(int filter, const LibraryFile &file)
{
    const bool finished = isFinished(file);
    const bool started = isStarted(file);

    switch (filter) {
    case Unwatched:
        return !finished && !started;
    case InProgress:
        return !finished && started;
    case Watched:
        return finished;
    case Unmatched:
        return !file.isMatched();
    case Suggested:
        return file.matchSuggested;
    case Everything:
    default:
        return true;
    }
}

bool isBefore(int sort, const LibraryFile &a, const LibraryFile &b)
{
    switch (sort) {
    case Title: {
        const int order = TextFold::compare(shownTitleOf(a), shownTitleOf(b));
        if (order == 0) {
            return a.id > b.id;
        }
        return order < 0;
    }
    case LastPlayed:
        if (a.playback.lastPlayed == b.playback.lastPlayed) {
            return a.id > b.id;
        }
        return a.playback.lastPlayed > b.playback.lastPlayed;
    case FileSize:
        if (a.sizeBytes == b.sizeBytes) {
            return a.id > b.id;
        }
        return a.sizeBytes > b.sizeBytes;
    case RecentlyAdded:
    default:
        return a.id > b.id;
    }
}

QList<LibraryFile> apply(const QList<LibraryFile> &files, int sort, int filter)
{
    QList<LibraryFile> result;
    result.reserve(files.size());

    for (const LibraryFile &file : files) {
        if (keeps(filter, file)) {
            result.append(file);
        }
    }

    if (sort == Title) {
        QList<std::pair<QString, LibraryFile>> keyed;
        keyed.reserve(result.size());
        for (const LibraryFile &file : std::as_const(result)) {
            keyed.append({TextFold::key(shownTitleOf(file)), file});
        }

        std::sort(keyed.begin(), keyed.end(),
                  [](const std::pair<QString, LibraryFile> &a,
                     const std::pair<QString, LibraryFile> &b) {
            if (a.first != b.first) {
                return a.first < b.first;
            }
            const int exact = shownTitleOf(a.second).compare(shownTitleOf(b.second));
            if (exact != 0) {
                return exact < 0;
            }
            return a.second.id > b.second.id;
        });

        for (qsizetype i = 0; i < keyed.size(); ++i) {
            result[i] = keyed.at(i).second;
        }
        return result;
    }

    std::sort(result.begin(), result.end(),
              [sort](const LibraryFile &a, const LibraryFile &b) {
        return isBefore(sort, a, b);
    });

    return result;
}

}
