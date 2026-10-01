#pragma once

#include <QList>
#include <QMetaType>

struct LibraryChange
{
    QList<qint64> fileIds;
    QList<qint64> mediaIds;
    int filesMatched = 0;
    int filesUnmatched = 0;

    bool isEmpty() const { return fileIds.isEmpty() && mediaIds.isEmpty(); }
};

Q_DECLARE_METATYPE(LibraryChange)
