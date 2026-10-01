#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace ShowGrouping {

struct Show
{
    QString title;
    QStringList folderHandles;
    QStringList fileHandles;
    QStringList fileNames;
    QList<int> seasons;

    bool titleFromFolder = false;
    QString titleFolder;
    bool episodeMarkers = false;
    bool numberedEpisodes = false;
    bool seasonFolders = false;

    bool isValid() const { return !title.isEmpty() && !fileHandles.isEmpty(); }
};

struct File
{
    QString handle;
    QString path;
    QString name;
};

QList<Show> group(const QStringList &fileHandles);
QList<Show> group(const QList<File> &files);

int episodeFromBareName(const QString &fileName, int season);

}
