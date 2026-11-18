#pragma once

#include "Data/MediaRepository.h"

#include <QHash>
#include <QList>
#include <QStringList>

namespace SimilarTitles {

struct People
{
    QStringList directors;
    QStringList cast;
};

struct Known
{
    QHash<qint64, People> people;
    QHash<qint64, qint64> collections;
};

double score(const MediaRecord &seed, const MediaRecord &other, const Known &known);

QList<MediaRecord> similarTo(const MediaRecord &seed,
                             const QList<MediaRecord> &titles,
                             const Known &known,
                             int limit);

}
