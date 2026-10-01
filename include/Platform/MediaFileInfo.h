#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

struct MediaFileInfo
{
    QString handle;
    QString parentHandle;
    QString displayName;
    qint64 sizeBytes = 0;
    QDateTime modified;
    bool isFolder = false;

    bool isValid() const { return !handle.isEmpty(); }
};

Q_DECLARE_METATYPE(MediaFileInfo)

namespace MediaFormats {

const QStringList &videoSuffixes();
bool isVideoFile(const QString &pathOrName);
bool looksLikeVideoFile(const QString &path);
bool isInSystemFolder(const QString &path);

}
