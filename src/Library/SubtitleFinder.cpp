#include "Library/SubtitleFinder.h"

#include "Library/SubtitleNaming.h"

#include <QDir>
#include <QFileInfo>

namespace SubtitleFinder {

const QStringList &subtitleFolderNames()
{
    static const QStringList names = {
        QStringLiteral("Subs"), QStringLiteral("Subtitles"), QStringLiteral("Sub")
    };
    return names;
}

bool isSubtitleFolderName(const QString &name)
{
    for (const QString &known : subtitleFolderNames()) {
        if (name.compare(known, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QStringList onDisk(const QString &videoPath, const IsVideo &isVideo)
{
    const QFileInfo video(videoPath);
    if (!video.exists()) {
        return QStringList();
    }

    const QDir folder = video.absoluteDir();
    const QString videoName = video.fileName();
    const QString videoBase = SubtitleNaming::baseName(videoName);

    const QFileInfoList beside = folder.entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
    int videosInFolder = 0;
    for (const QFileInfo &entry : beside) {
        if (isVideo && isVideo(entry.fileName())) {
            ++videosInFolder;
        }
    }
    const bool onlyVideo = videosInFolder <= 1;

    QStringList matches;
    const auto consider = [&](const QFileInfoList &files, SubtitleNaming::Place place) {
        for (const QFileInfo &file : files) {
            const QString name = file.fileName();
            if (!SubtitleNaming::isSubtitleFile(name)) {
                continue;
            }
            if (SubtitleNaming::belongsToVideo(videoName, name, place, onlyVideo)) {
                matches.append(file.absoluteFilePath());
            }
        }
    };

    consider(beside, SubtitleNaming::Place::BesideVideo);

    const QFileInfoList folders = folder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                       QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : folders) {
        if (!isSubtitleFolderName(entry.fileName())) {
            continue;
        }
        const QDir subs(entry.absoluteFilePath());
        consider(subs.entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase),
                 SubtitleNaming::Place::SubtitleFolder);

        const QFileInfoList inner = subs.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                       QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &named : inner) {
            if (named.fileName().compare(videoBase, Qt::CaseInsensitive) == 0) {
                consider(QDir(named.absoluteFilePath())
                             .entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase),
                         SubtitleNaming::Place::FolderNamedAfterVideo);
            }
        }
    }

    return matches;
}

}
