#include "Platform/FilesystemMediaSource.h"

#include "Library/SubtitleFinder.h"
#include "MmLog.h"
#include "Platform/MediaFileInfo.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

FilesystemMediaSource::FilesystemMediaSource() = default;

FilesystemMediaSource::~FilesystemMediaSource() = default;

const QStringList &FilesystemMediaSource::subtitleSuffixes()
{
    static const QStringList suffixes = {
        QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"),
        QStringLiteral("sub"), QStringLiteral("vtt"), QStringLiteral("idx")
    };
    return suffixes;
}

QString FilesystemMediaSource::nativePath(const QString &handle) const
{
    return handle;
}

QString FilesystemMediaSource::mpvUrl(const QString &handle)
{
    return nativePath(handle);
}

bool FilesystemMediaSource::exists(const QString &handle) const
{
    return QFileInfo::exists(nativePath(handle));
}

MediaFileInfo FilesystemMediaSource::info(const QString &handle) const
{
    const QFileInfo fileInfo(nativePath(handle));

    MediaFileInfo result;
    if (!fileInfo.exists()) {
        MM_LOG_W() << "media source info requested for missing path" << handle;
        return result;
    }

    result.handle = handle;
    result.parentHandle = fileInfo.absolutePath();
    result.displayName = fileInfo.fileName();
    result.sizeBytes = fileInfo.size();
    result.modified = fileInfo.lastModified();
    result.isFolder = fileInfo.isDir();
    return result;
}

QString FilesystemMediaSource::displayPath(const QString &handle) const
{
    return handle;
}

QStringList FilesystemMediaSource::siblingSubtitles(const QString &handle) const
{
    const QFileInfo fileInfo(nativePath(handle));
    if (!fileInfo.exists()) {
        return QStringList();
    }

    const QStringList matches = SubtitleFinder::onDisk(
        fileInfo.absoluteFilePath(),
        [](const QString &name) { return MediaFormats::isVideoFile(name); });

    MM_LOG_I() << "found" << matches.size() << "subtitles belonging to"
               << fileInfo.fileName();
    return matches;
}

QList<MediaFileInfo> FilesystemMediaSource::listChildren(const QString &handle) const
{
    QList<MediaFileInfo> entries;

    QDir dir(nativePath(handle));
    if (!dir.exists()) {
        MM_LOG_W() << "cannot browse a folder that is not there" << handle;
        return entries;
    }

    const QFileInfoList children = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &child : children) {
        if (!child.isDir() && !MediaFormats::isVideoFile(child.fileName())) {
            continue;
        }

        MediaFileInfo info;
        info.handle = child.absoluteFilePath();
        info.parentHandle = child.absolutePath();
        info.displayName = child.fileName();
        info.sizeBytes = child.isDir() ? 0 : child.size();
        info.modified = child.lastModified();
        info.isFolder = child.isDir();
        entries.append(info);
    }

    MM_LOG_D() << "browsed" << handle << "->" << entries.size() << "entries";
    return entries;
}

QString FilesystemMediaSource::parentOf(const QString &handle) const
{
    const QFileInfo info(nativePath(handle));
    const QDir parent = info.isDir() ? QDir(info.absoluteFilePath()) : info.absoluteDir();
    QDir walk = parent;
    if (info.isDir() && !walk.cdUp()) {
        return QString();
    }
    return walk.absolutePath();
}

bool FilesystemMediaSource::isRootAvailable(const QString &rootHandle) const
{
    const QString native = nativePath(rootHandle);

    if (!QFileInfo::exists(native)) {
        MM_LOG_W() << "scan root unavailable" << rootHandle;
        return false;
    }

    const QStorageInfo storage(native);
    if (!storage.isValid() || !storage.isReady()) {
        MM_LOG_W() << "scan root volume not ready" << rootHandle;
        return false;
    }

    return true;
}
