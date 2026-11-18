#include "Library/PreviewPick.h"

#include <algorithm>

namespace PreviewPick {

double startSeconds(double durationSeconds, double chance)
{
    if (durationSeconds <= 0.0) {
        return kUnknownLengthStart;
    }
    const double fraction = kEarliest + (kLatest - kEarliest) * std::clamp(chance, 0.0, 1.0);
    return durationSeconds * fraction;
}

int episodeIndex(const QList<EpisodeRecord> &episodes, double chance)
{
    QList<int> playable;
    bool started = false;
    for (int i = 0; i < episodes.size(); ++i) {
        const EpisodeRecord &episode = episodes.at(i);
        if (episode.watched || episode.positionSeconds > 0.0) {
            started = true;
        }
        if (episode.hasFile() && !episode.missing && !episode.fileHandle.isEmpty()) {
            playable.append(i);
        }
    }
    if (playable.isEmpty()) {
        return -1;
    }

    if (started) {
        for (const int i : std::as_const(playable)) {
            if (!episodes.at(i).watched) {
                return i;
            }
        }
    } else {
        int firstSeason = -1;
        for (const int i : std::as_const(playable)) {
            const int season = episodes.at(i).season;
            if (season > 0 && (firstSeason < 0 || season < firstSeason)) {
                firstSeason = season;
            }
        }
        if (firstSeason > 0) {
            QList<int> inSeason;
            for (const int i : std::as_const(playable)) {
                if (episodes.at(i).season == firstSeason) {
                    inSeason.append(i);
                }
            }
            playable = inSeason;
        }
    }

    const int at = std::min(int(std::clamp(chance, 0.0, 1.0) * playable.size()),
                            int(playable.size()) - 1);
    return playable.at(at);
}

}
