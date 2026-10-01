#include "Platform/Windows/WindowsMediaSource.h"

#include <QDir>

WindowsMediaSource::WindowsMediaSource() = default;

WindowsMediaSource::~WindowsMediaSource() = default;

QString WindowsMediaSource::toExtendedLengthPath(const QString &path)
{
    if (path.startsWith(QLatin1String("\\\\?\\"))) {
        return path;
    }

    const QString native = QDir::toNativeSeparators(path);

    if (native.length() < 260) {
        return native;
    }

    if (native.startsWith(QLatin1String("\\\\"))) {
        return QStringLiteral("\\\\?\\UNC\\") + native.mid(2);
    }

    return QStringLiteral("\\\\?\\") + native;
}

QString WindowsMediaSource::nativePath(const QString &handle) const
{
    return toExtendedLengthPath(handle);
}

QString WindowsMediaSource::displayPath(const QString &handle) const
{
    return QDir::toNativeSeparators(handle);
}
