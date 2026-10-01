#pragma once

#include <QHash>
#include <QList>
#include <QString>

struct AttachedSubtitle
{
    QString path;
    QString displayName;
};

struct LibrarySnapshotResult
{
    bool ok = false;
    QString error;
    QString path;
    QString revision;
    qint64 bytes = 0;
    int schemaVersion = 0;
    int titles = 0;
    int files = 0;
    QHash<qint64, QString> servedFiles;
    QHash<qint64, QList<AttachedSubtitle>> attachedSubtitles;
};

namespace LibrarySnapshot {

inline constexpr qint64 ServerFolderId = 1;

QString serverFolderHandle(const QString &serverId);

LibrarySnapshotResult build(const QString &sourcePath, const QString &outputPath,
                            const QString &serverId, const QString &serverName);

QString revisionOf(const QString &path);

}
