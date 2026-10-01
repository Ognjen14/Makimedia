#pragma once

#include <QString>
#include <QStringList>

namespace SubtitleNaming {

const QStringList &subtitleSuffixes();

enum class Place {
    BesideVideo,
    SubtitleFolder,
    FolderNamedAfterVideo
};

struct Description
{
    QString language;
    QString code;
    QString title;
};

QString baseName(const QString &pathOrName);
QString languageOf(const QString &baseName);
QString titleFor(const QString &baseName, const QString &language);
Description describe(const QString &videoPathOrName, const QString &subtitlePathOrName);

bool isSubtitleFile(const QString &pathOrName);
bool belongsToVideo(const QString &videoPathOrName, const QString &subtitlePathOrName);
bool belongsToVideo(const QString &videoPathOrName, const QString &subtitlePathOrName,
                    Place place, bool onlyVideoInFolder);

}
