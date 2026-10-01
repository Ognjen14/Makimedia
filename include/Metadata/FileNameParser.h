#pragma once

#include <QList>
#include <QString>

struct ParsedFileName
{
    QString title;
    QString episodeTitle;
    QString releaseGroup;
    int year = 0;
    int season = 0;
    int episode = 0;

    QString alternativeTitle;
    QString titleIfEpisode;
    bool titleFromFallback = false;
    bool episodeWithoutSeason = false;
    bool specials = false;

    bool looksLikeEpisode() const { return episode > 0 && (season > 0 || specials); }
    bool isEmpty() const { return title.isEmpty(); }
};

namespace FileNameParser {

struct Ancestor
{
    QString name;
    QString handle;
};

ParsedFileName parse(const QString &fileNameOrPath);

ParsedFileName parsePath(const QString &filePath);

QList<Ancestor> ancestorsOf(const QString &filePath);

bool isJunkToken(const QString &token);

int seasonFromFolder(const QString &folderName);

bool isSeasonOnlyFolder(const QString &folderName);

}
