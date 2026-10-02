#include "Platform/Android/AndroidMediaSource.h"

#include "Library/SubtitleNaming.h"
#include "MmLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJniObject>
#include <QObject>

#include <unistd.h>

namespace {
constexpr const char *kActivityClass = "com/topicdev/makimedia/org/MakimediaActivity";
constexpr const char *kMediaStoreScheme = "mediastore://";

bool isSubtitleFolderName(const QString &name)
{
    return name.compare(QLatin1String("subs"), Qt::CaseInsensitive) == 0
        || name.compare(QLatin1String("subtitles"), Qt::CaseInsensitive) == 0
        || name.compare(QLatin1String("sub"), Qt::CaseInsensitive) == 0;
}

void appendSubtitleRows(const QDir &folder, const char *place, QString &listing)
{
    const QFileInfoList files = folder.entryInfoList(QDir::Files);
    for (const QFileInfo &file : files) {
        const QString name = file.fileName();
        if (name.contains(QLatin1Char('\t')) || name.contains(QLatin1Char('\n'))
            || !SubtitleNaming::isSubtitleFile(name)) {
            continue;
        }
        listing += QString::fromLatin1(place) + QLatin1Char('\t') + name
                   + QLatin1Char('\t') + file.absoluteFilePath() + QLatin1Char('\n');
    }
}

QString plainSubtitleListing(const QFileInfo &video)
{
    QString listing;
    const QDir folder = video.absoluteDir();
    const QString videoBase = SubtitleNaming::baseName(video.fileName());

    appendSubtitleRows(folder, "beside", listing);

    const QFileInfoList folders =
        folder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : folders) {
        if (!isSubtitleFolderName(entry.fileName())) {
            continue;
        }
        const QDir subs(entry.absoluteFilePath());
        appendSubtitleRows(subs, "subs", listing);

        const QFileInfoList inner = subs.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &named : inner) {
            if (named.fileName().compare(videoBase, Qt::CaseInsensitive) == 0) {
                appendSubtitleRows(QDir(named.absoluteFilePath()), "named", listing);
            }
        }
    }
    return listing;
}
}

bool AndroidMediaSource::isMediaStoreRoot(const QString &rootHandle)
{
    return rootHandle.startsWith(QLatin1String(kMediaStoreScheme));
}

QString AndroidMediaSource::mediaStoreVolume(const QString &rootHandle)
{
    if (!isMediaStoreRoot(rootHandle)) {
        return QString();
    }
    return rootHandle.mid(int(qstrlen(kMediaStoreScheme)));
}

AndroidMediaSource::AndroidMediaSource() = default;

AndroidMediaSource::~AndroidMediaSource() = default;

QString AndroidMediaSource::primaryRoot()
{
    const QString primary = QStringLiteral("/storage/emulated/0");
    return QDir(primary).isReadable() ? primary : QString();
}

QStringList AndroidMediaSource::externalVolumes()
{
    QStringList volumes;

    const QDir storage(QStringLiteral("/storage"));
    const QFileInfoList entries = storage.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        if (name == QLatin1String("emulated") || name == QLatin1String("self")) {
            continue;
        }

        const QString path = entry.absoluteFilePath();
        const QDir dir(path);
        if (!dir.isReadable()) {
            MM_LOG_W() << "storage volume found but not readable, scoped storage"
                       << "probably requires SAF for it:" << path;
            continue;
        }

        MM_LOG_I() << "storage volume readable:" << path;
        volumes.append(path);
    }

    return volumes;
}

QString AndroidMediaSource::volumeLabel(const QString &path)
{
    if (path == QStringLiteral("/storage/emulated/0")) {
        return QObject::tr("Internal storage");
    }

    if (isMediaStoreRoot(path)) {
        const QString volume = mediaStoreVolume(path);
        const QString description = QJniObject::callStaticMethod<jstring>(
            kActivityClass,
            "volumeDescription",
            "(Ljava/lang/String;)Ljava/lang/String;",
            QJniObject::fromString(volume).object<jstring>()).toString();
        if (!description.isEmpty()) {
            return description;
        }
        return QObject::tr("USB or SD card (%1)").arg(volume.toUpper());
    }

    const QString name = QDir(path).dirName();
    return name.isEmpty() ? path : QObject::tr("SD card (%1)").arg(name);
}

bool AndroidMediaSource::writeSystemThumbnail(const QString &handle,
                                              const QString &outputPath,
                                              int width)
{
    if (!isContentUri(handle) || outputPath.isEmpty()) {
        return false;
    }

    const QJniObject uri = QJniObject::fromString(handle);
    const QJniObject out = QJniObject::fromString(outputPath);

    const bool ok = QJniObject::callStaticMethod<jboolean>(
        kActivityClass,
        "writeMediaThumbnail",
        "(Ljava/lang/String;Ljava/lang/String;II)Z",
        uri.object<jstring>(),
        out.object<jstring>(),
        jint(width),
        jint(width * 9 / 16));

    if (ok) {
        MM_LOG_D() << "system thumbnail written for" << handle;
    }
    return ok;
}

QStringList AndroidMediaSource::removableVolumeNames()
{
    const QString joined = QJniObject::callStaticMethod<jstring>(
        kActivityClass,
        "removableVolumeNames",
        "()Ljava/lang/String;").toString();

    if (joined.isEmpty()) {
        return {};
    }

    const QStringList names = joined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    MM_LOG_I() << "removable volumes indexed by MediaStore:" << names;
    return names;
}

QList<MediaFileInfo> AndroidMediaSource::videosOnVolume(const QString &volumeName)
{
    QList<MediaFileInfo> files;
    if (volumeName.isEmpty()) {
        return files;
    }

    const QJniObject name = QJniObject::fromString(volumeName);
    const QString listing = QJniObject::callStaticMethod<jstring>(
        kActivityClass,
        "videosOnVolume",
        "(Ljava/lang/String;)Ljava/lang/String;",
        name.object<jstring>()).toString();

    if (listing.isEmpty()) {
        MM_LOG_I() << "no video indexed on volume" << volumeName;
        return files;
    }

    int skippedSystem = 0;
    const QStringList rows = listing.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &row : rows) {
        const QStringList parts = row.split(QLatin1Char('\t'));
        if (parts.size() < 5) {
            MM_LOG_W() << "malformed media row, skipping:" << row;
            continue;
        }

        MediaFileInfo info;
        info.handle = parts.at(0);
        info.displayName = parts.at(1);
        info.sizeBytes = parts.at(2).toLongLong();
        info.modified = QDateTime::fromSecsSinceEpoch(parts.at(3).toLongLong());

        QString relative = parts.at(4);
        while (relative.endsWith(QLatin1Char('/'))) {
            relative.chop(1);
        }
        info.parentHandle = volumeName + QLatin1Char(':') + relative;

        if (!MediaFormats::isVideoFile(info.displayName)) {
            continue;
        }

        if (MediaFormats::isInSystemFolder(relative)) {
            ++skippedSystem;
            continue;
        }

        files.append(info);
    }

    MM_LOG_I() << "MediaStore listed" << files.size() << "videos on"
               << volumeName << "- skipped" << skippedSystem
               << "in recycle bins and system folders";
    return files;
}

QStringList AndroidMediaSource::storageRoots()
{
    QStringList roots;

    const QString primary = primaryRoot();
    if (!primary.isEmpty()) {
        roots.append(primary);
    }

    const QStringList readable = externalVolumes();
    for (const QString &volume : readable) {
        if (!roots.contains(volume)) {
            roots.append(volume);
        }
    }

    QStringList readableNames;
    for (const QString &path : readable) {
        readableNames.append(QDir(path).dirName().toLower());
    }

    for (const QString &name : removableVolumeNames()) {
        if (readableNames.contains(name.toLower())) {
            MM_LOG_I() << "volume" << name
                       << "is readable directly, not using MediaStore for it";
            continue;
        }
        roots.append(QLatin1String(kMediaStoreScheme) + name);
    }

    MM_LOG_I() << "android storage roots:" << roots;
    return roots;
}

bool AndroidMediaSource::isContentUri(const QString &handle)
{
    return handle.startsWith(QLatin1String("content://"));
}

bool AndroidMediaSource::persistAccess(const QString &handle)
{
    if (!isContentUri(handle)) {
        return true;
    }

    const bool ok = QJniObject::callStaticMethod<jboolean>(
        kActivityClass,
        "persistContentPermission",
        "(Ljava/lang/String;)Z",
        QJniObject::fromString(handle).object<jstring>());

    if (!ok) {
        MM_LOG_W() << "content uri access is not persistable, the attachment will"
                   << "only last for this run:" << handle;
    }
    return ok;
}

QString AndroidMediaSource::mpvUrl(const QString &handle)
{
    if (!isContentUri(handle)) {
        return FilesystemMediaSource::mpvUrl(handle);
    }

    const int fd = QJniObject::callStaticMethod<jint>(
        kActivityClass,
        "openContentFd",
        "(Ljava/lang/String;)I",
        QJniObject::fromString(handle).object<jstring>());

    if (fd < 0) {
        MM_LOG_E() << "could not open a file descriptor for" << handle;
        return QString();
    }

    MM_LOG_I() << "opened content uri as fd" << fd << handle;
    return QStringLiteral("fdclose://%1").arg(fd);
}

QString AndroidMediaSource::displayPath(const QString &handle) const
{
    if (!isContentUri(handle)) {
        return FilesystemMediaSource::displayPath(handle);
    }

    const QString name = QJniObject::callStaticMethod<jstring>(
        kActivityClass,
        "contentDisplayName",
        "(Ljava/lang/String;)Ljava/lang/String;",
        QJniObject::fromString(handle).object<jstring>()).toString();

    return name.isEmpty() ? handle : name;
}

QByteArray AndroidMediaSource::readHead(const QString &handle, qint64 maxBytes)
{
    if (handle.isEmpty() || maxBytes <= 0) {
        return QByteArray();
    }

    if (!isContentUri(handle)) {
        QFile file(handle);
        if (!file.open(QIODevice::ReadOnly)) {
            MM_LOG_W() << "could not read the start of" << handle << file.errorString();
            return QByteArray();
        }
        return file.read(maxBytes);
    }

    const int fd = QJniObject::callStaticMethod<jint>(
        kActivityClass,
        "openContentFd",
        "(Ljava/lang/String;)I",
        QJniObject::fromString(handle).object<jstring>());
    if (fd < 0) {
        MM_LOG_W() << "could not open" << handle << "to read its start";
        return QByteArray();
    }

    QByteArray data(qsizetype(maxBytes), Qt::Uninitialized);
    qint64 total = 0;
    while (total < maxBytes) {
        const ssize_t got = ::read(fd, data.data() + total, size_t(maxBytes - total));
        if (got <= 0) {
            break;
        }
        total += got;
    }
    ::close(fd);

    data.truncate(qsizetype(total));
    return data;
}

bool AndroidMediaSource::hasSubtitleFolderAccess(const QString &folderPath)
{
    if (folderPath.isEmpty()) {
        return false;
    }
    return QJniObject::callStaticMethod<jboolean>(
        kActivityClass,
        "hasSubtitleFolderAccess",
        "(Ljava/lang/String;)Z",
        QJniObject::fromString(folderPath).object<jstring>());
}

void AndroidMediaSource::requestSubtitleFolderAccess(const QString &folderPath)
{
    MM_LOG_I() << "asking for access to the subtitles in" << folderPath;
    QJniObject::callStaticMethod<void>(
        kActivityClass,
        "requestSubtitleFolderAccess",
        "(Ljava/lang/String;)V",
        QJniObject::fromString(folderPath).object<jstring>());
}

QStringList AndroidMediaSource::siblingSubtitles(const QString &handle) const
{
    if (handle.isEmpty()) {
        return QStringList();
    }

    QString videoName;
    QString listing;
    int videosInFolder = 0;

    if (isContentUri(handle)) {
        listing = QJniObject::callStaticMethod<jstring>(
            kActivityClass,
            "listVolumeSubtitleCandidates",
            "(Ljava/lang/String;)Ljava/lang/String;",
            QJniObject::fromString(handle).object<jstring>()).toString();

        const int headerEnd = listing.indexOf(QLatin1Char('\n'));
        const QString header = headerEnd >= 0 ? listing.left(headerEnd) : listing;
        if (!header.startsWith(QLatin1String("video\t"))) {
            MM_LOG_I() << "MediaStore could not say what is beside" << handle;
            return QStringList();
        }
        videoName = header.mid(6);
        listing = headerEnd >= 0 ? listing.mid(headerEnd + 1) : QString();

        const QStringList rows = listing.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &row : rows) {
            const QStringList parts = row.split(QLatin1Char('\t'));
            if (parts.size() >= 3 && parts.at(0) == QLatin1String("beside")
                && MediaFormats::isVideoFile(parts.at(1))) {
                ++videosInFolder;
            }
        }
        MM_LOG_I() << "MediaStore lists" << rows.size() << "files around" << videoName
                   << "on its drive";
    } else {
        const QFileInfo video(handle);
        videoName = video.fileName();
        listing = QJniObject::callStaticMethod<jstring>(
            kActivityClass,
            "listSubtitleCandidates",
            "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
            QJniObject::fromString(video.absolutePath()).object<jstring>(),
            QJniObject::fromString(SubtitleNaming::baseName(video.fileName())).object<jstring>())
            .toString();

        if (listing.isEmpty()) {
            listing = plainSubtitleListing(video);
            MM_LOG_I() << "no granted folder for" << video.fileName()
                       << "- read its folder directly and found"
                       << listing.count(QLatin1Char('\n')) << "subtitle files";
        }

        const QFileInfoList entries = QDir(video.absolutePath()).entryInfoList(QDir::Files);
        for (const QFileInfo &entry : entries) {
            if (MediaFormats::isVideoFile(entry.fileName())) {
                ++videosInFolder;
            }
        }
    }

    if (listing.isEmpty()) {
        return QStringList();
    }
    const bool onlyVideo = videosInFolder <= 1;

    QStringList matches;
    const QStringList rows = listing.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &row : rows) {
        const QStringList parts = row.split(QLatin1Char('\t'));
        if (parts.size() < 3) {
            continue;
        }

        if (!SubtitleNaming::isSubtitleFile(parts.at(1))) {
            continue;
        }

        SubtitleNaming::Place place = SubtitleNaming::Place::BesideVideo;
        if (parts.at(0) == QLatin1String("subs")) {
            place = SubtitleNaming::Place::SubtitleFolder;
        } else if (parts.at(0) == QLatin1String("named")) {
            place = SubtitleNaming::Place::FolderNamedAfterVideo;
        }

        if (SubtitleNaming::belongsToVideo(videoName, parts.at(1), place, onlyVideo)) {
            matches.append(parts.at(2));
        }
    }

    MM_LOG_I() << "subtitle folders of" << videoName << "list" << rows.size()
               << "files," << videosInFolder << "videos beside it,"
               << matches.size() << "subtitles belong to it";
    return matches;
}

bool AndroidMediaSource::exists(const QString &handle) const
{
    if (!isContentUri(handle)) {
        return FilesystemMediaSource::exists(handle);
    }

    const QString name = QJniObject::callStaticMethod<jstring>(
        kActivityClass,
        "contentDisplayName",
        "(Ljava/lang/String;)Ljava/lang/String;",
        QJniObject::fromString(handle).object<jstring>()).toString();
    if (name.isEmpty()) {
        MM_LOG_W() << "content uri no longer resolves" << handle;
    }
    return !name.isEmpty();
}

bool AndroidMediaSource::isRootAvailable(const QString &rootHandle) const
{
    if (isMediaStoreRoot(rootHandle)) {
        const QString volume = mediaStoreVolume(rootHandle);
        const bool mounted = removableVolumeNames().contains(volume);
        if (!mounted) {
            MM_LOG_W() << "MediaStore volume no longer mounted:" << volume;
        }
        return mounted;
    }

    if (!QFileInfo::exists(rootHandle)) {
        MM_LOG_W() << "scan root unavailable" << rootHandle;
        return false;
    }

    const QDir dir(rootHandle);
    if (!dir.isReadable()) {
        MM_LOG_W() << "scan root exists but is not readable, the storage"
                   << "permission is probably missing:" << rootHandle;
        return false;
    }

    return true;
}
