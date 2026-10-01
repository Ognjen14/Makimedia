#pragma once

#include "Platform/FilesystemMediaSource.h"

class WindowsMediaSource : public FilesystemMediaSource
{
public:
    WindowsMediaSource();
    ~WindowsMediaSource() override;

    QString displayPath(const QString &handle) const override;

    static QString toExtendedLengthPath(const QString &path);

protected:
    QString nativePath(const QString &handle) const override;
};
