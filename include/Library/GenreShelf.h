#pragma once

#include "Data/MediaRepository.h"

#include <QDate>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace GenreShelf {

struct Row
{
    QString genre;
    QList<MediaRecord> titles;
    QString label;
};

QStringList genresOf(const QString &stored);

bool finished(const MediaRecord &title);
bool started(const MediaRecord &title);

double dayChance(const QDate &day, quint64 salt);
quint64 saltOf(const QString &text);

QList<Row> build(const QList<MediaRecord> &titles,
                 const QHash<qint64, qint64> &lastAdded,
                 int smallestRow);

QList<Row> daily(const QList<MediaRecord> &titles,
                 const QHash<qint64, qint64> &lastAdded,
                 int smallestRow,
                 int rowsPerDay,
                 const QDate &day);

QList<MediaRecord> shortOnTime(const QList<MediaRecord> &titles,
                               int longestMinutes,
                               int limit,
                               const QDate &day);

}
