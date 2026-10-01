#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

struct ScanFolder
{
    qint64 id = -1;
    QString handle;
    QString displayName;
    QDateTime lastScanned;
    bool available = true;
    QDateTime added;

    bool isValid() const { return id >= 0 && !handle.isEmpty(); }

    bool operator==(const ScanFolder &other) const
    {
        return id == other.id
            && handle == other.handle
            && displayName == other.displayName
            && lastScanned == other.lastScanned
            && available == other.available
            && added == other.added;
    }

    bool operator!=(const ScanFolder &other) const
    {
        return !(*this == other);
    }
};

Q_DECLARE_METATYPE(ScanFolder)
