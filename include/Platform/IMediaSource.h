#pragma once

#include "Platform/MediaFileInfo.h"

#include <QList>
#include <QString>
#include <QStringList>

class IMediaSource
{
public:
    virtual ~IMediaSource() = default;

    virtual QString mpvUrl(const QString &handle) = 0;
    virtual bool exists(const QString &handle) const = 0;
    virtual MediaFileInfo info(const QString &handle) const = 0;
    virtual QString displayPath(const QString &handle) const = 0;
    virtual QStringList siblingSubtitles(const QString &handle) const = 0;
    virtual QList<MediaFileInfo> listChildren(const QString &handle) const = 0;
    virtual QString parentOf(const QString &handle) const = 0;
    virtual bool isRootAvailable(const QString &rootHandle) const = 0;
};
