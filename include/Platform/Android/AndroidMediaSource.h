#pragma once

#include "Platform/FilesystemMediaSource.h"

class AndroidMediaSource : public FilesystemMediaSource
{
public:
    AndroidMediaSource();
    ~AndroidMediaSource() override;

    QString mpvUrl(const QString &handle) override;
    QString displayPath(const QString &handle) const override;
    bool exists(const QString &handle) const override;
    QStringList siblingSubtitles(const QString &handle) const override;
    bool isRootAvailable(const QString &rootHandle) const override;

    static QByteArray readHead(const QString &handle, qint64 maxBytes);
    static bool hasSubtitleFolderAccess(const QString &folderPath);
    static void requestSubtitleFolderAccess(const QString &folderPath);

    static bool isContentUri(const QString &handle);
    static bool persistAccess(const QString &handle);

    static QStringList storageRoots();
    static QString primaryRoot();
    static QStringList externalVolumes();
    static QString volumeLabel(const QString &path);

    static QStringList removableVolumeNames();
    static QList<MediaFileInfo> videosOnVolume(const QString &volumeName);

    static bool isMediaStoreRoot(const QString &rootHandle);
    static QString mediaStoreVolume(const QString &rootHandle);

    static bool writeSystemThumbnail(const QString &handle,
                                     const QString &outputPath,
                                     int width);
};
