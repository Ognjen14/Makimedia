#pragma once

#include "Data/PlaybackState.h"

#include <QList>
#include <QString>

class Database;

class PlaybackStateRepository
{
public:
    explicit PlaybackStateRepository(Database &database);

    PlaybackState forFile(qint64 fileId) const;
    bool save(const PlaybackState &state);
    bool setWatched(qint64 fileId, bool watched);
    bool setWatched(const QList<qint64> &fileIds, bool watched);
    bool clear(qint64 fileId);

    static double finishedThresholdFraction();
    static double resumeAfterSeconds();
    static bool isFinished(double positionSeconds, double durationSeconds);
    static double resumeSeconds(double positionSeconds, double durationSeconds,
                                bool watched);
    static bool resumable(double positionSeconds, double durationSeconds,
                          bool watched);

    QList<qint64> adoptFromMissing(const QList<qint64> &mediaIds);
    QList<qint64> mediaWithMissingHistory() const;

private:
    Database &m_database;
};
