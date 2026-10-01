#pragma once

#include "Data/MediaRepository.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace GenreShelf {

struct Row
{
    QString genre;
    QList<MediaRecord> titles;
};

QStringList genresOf(const QString &stored);

QList<Row> build(const QList<MediaRecord> &titles,
                 const QHash<qint64, qint64> &lastAdded,
                 int smallestRow);

}
