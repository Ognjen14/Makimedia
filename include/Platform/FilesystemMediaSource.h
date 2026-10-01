#pragma once

#include "Platform/IMediaSource.h"

class FilesystemMediaSource : public IMediaSource
{
public:
    FilesystemMediaSource();
    ~FilesystemMediaSource() override;

    QString mpvUrl(const QString &handle) override;
    bool exists(const QString &handle) const override;
    MediaFileInfo info(const QString &handle) const override;
    QString displayPath(const QString &handle) const override;
    QStringList siblingSubtitles(const QString &handle) const override;
    QList<MediaFileInfo> listChildren(const QString &handle) const override;
    QString parentOf(const QString &handle) const override;
    bool isRootAvailable(const QString &rootHandle) const override;

protected:
    virtual QString nativePath(const QString &handle) const;

    static const QStringList &subtitleSuffixes();
};
