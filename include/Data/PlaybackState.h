#pragma once

#include <QDateTime>
#include <QMetaType>

struct PlaybackState
{
    qint64 fileId = -1;
    double positionSeconds = 0.0;
    double durationSeconds = 0.0;
    double watchedSeconds = 0.0;
    bool watched = false;
    QDateTime lastPlayed;

    bool isValid() const { return fileId >= 0; }

    bool isPartial() const
    {
        return !watched && positionSeconds > 0.0 && durationSeconds > 0.0;
    }

    double progress() const
    {
        if (durationSeconds <= 0.0) {
            return 0.0;
        }
        const double value = positionSeconds / durationSeconds;
        return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
    }

    bool operator==(const PlaybackState &other) const
    {
        return fileId == other.fileId
            && positionSeconds == other.positionSeconds
            && durationSeconds == other.durationSeconds
            && watchedSeconds == other.watchedSeconds
            && watched == other.watched
            && lastPlayed == other.lastPlayed;
    }

    bool operator!=(const PlaybackState &other) const
    {
        return !(*this == other);
    }
};

Q_DECLARE_METATYPE(PlaybackState)
