#include "Metadata/ShowGrouping.h"

#include "Metadata/FileNameParser.h"
#include "Metadata/MatchScorer.h"

#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace {

struct Resolved
{
    QString title;
    QString folderHandle;
    int season = 0;
    bool showShaped = false;

    bool titleFromFolder = false;
    QString titleFolder;
    bool episodeMarker = false;
    bool numberedEpisode = false;
    bool seasonFolder = false;

    bool isValid() const { return !title.isEmpty(); }
};

bool namesAFilmSet(const QString &folderName)
{
    static const QStringList words = {
        QStringLiteral("trilogy"),   QStringLiteral("duology"),
        QStringLiteral("quadrilogy"), QStringLiteral("pentalogy"),
        QStringLiteral("saga"),      QStringLiteral("anthology"),
        QStringLiteral("collection"), QStringLiteral("boxset"),
        QStringLiteral("box set")
    };

    const QString lowered = folderName.toLower();
    for (const QString &word : words) {
        if (lowered.contains(word)) {
            return true;
        }
    }
    return false;
}

Resolved resolve(const QString &fileHandle)
{
    Resolved resolved;

    if (!fileHandle.contains(QLatin1Char('/'))
        && !fileHandle.contains(QLatin1Char('\\'))) {
        return resolved;
    }

    const ParsedFileName file = FileNameParser::parse(fileHandle);
    int season = file.season;

    const bool nameSpeaksForItself = file.looksLikeEpisode()
        && !file.title.isEmpty() && !file.titleFromFallback;
    if (nameSpeaksForItself) {
        resolved.title = file.title;
    }

    resolved.episodeMarker = file.looksLikeEpisode();
    resolved.numberedEpisode = file.episode > 0 && file.season <= 0 && !file.specials;

    const QList<FileNameParser::Ancestor> ancestors =
        FileNameParser::ancestorsOf(fileHandle);
    for (const FileNameParser::Ancestor &ancestor : ancestors) {
        if (FileNameParser::isSeasonOnlyFolder(ancestor.name)) {
            resolved.seasonFolder = true;
            if (season <= 0 && !file.specials) {
                season = FileNameParser::seasonFromFolder(ancestor.name);
            }
            continue;
        }

        const ParsedFileName folder = FileNameParser::parse(ancestor.name);
        if (folder.title.isEmpty() || folder.titleFromFallback) {
            continue;
        }

        if (resolved.title.isEmpty()) {
            resolved.title = folder.title;
            resolved.titleFromFolder = true;
            resolved.titleFolder = ancestor.name;
        }
        resolved.folderHandle = ancestor.handle;
        if (season <= 0 && !file.specials) {
            season = folder.season;
        }
        break;
    }

    resolved.season = season;
    resolved.showShaped = file.episode > 0 || season > 0;
    return resolved;
}

}

namespace ShowGrouping {

int episodeFromBareName(const QString &fileName, int season)
{
    if (season <= 0) {
        return 0;
    }

    const QString stem = QFileInfo(fileName).completeBaseName().trimmed();

    static const QRegularExpression digitsOnly(QStringLiteral("^[0-9]{1,3}$"));
    if (!digitsOnly.match(stem).hasMatch()) {
        return 0;
    }

    if (stem.size() < 3) {
        return stem.toInt();
    }

    const QString marker = QString::number(season);
    if (stem.size() == marker.size() + 2 && stem.startsWith(marker)) {
        return stem.mid(marker.size()).toInt();
    }

    return 0;
}

QList<Show> group(const QStringList &fileHandles)
{
    QList<File> files;
    files.reserve(fileHandles.size());
    for (const QString &handle : fileHandles) {
        files.append({handle, handle, QString()});
    }
    return group(files);
}

QList<Show> group(const QList<File> &files)
{
    QList<Show> shows;
    QHash<QString, int> byTitle;

    for (const File &file : files) {
        const QString &handle = file.handle;
        const Resolved resolved = resolve(file.path);
        if (!resolved.isValid() || !resolved.showShaped) {
            continue;
        }

        const QString key = MatchScorer::normaliseTitle(resolved.title);
        if (key.isEmpty()) {
            continue;
        }

        int index = byTitle.value(key, -1);
        if (index < 0) {
            Show show;
            show.title = resolved.title;
            shows.append(show);
            index = int(shows.size()) - 1;
            byTitle.insert(key, index);
        }

        Show &show = shows[index];
        show.fileHandles.append(handle);
        show.fileNames.append(file.name.isEmpty()
                                  ? QFileInfo(file.path).fileName()
                                  : file.name);
        if (!resolved.folderHandle.isEmpty()
            && !show.folderHandles.contains(resolved.folderHandle)) {
            show.folderHandles.append(resolved.folderHandle);
        }
        if (resolved.season > 0 && !show.seasons.contains(resolved.season)) {
            show.seasons.append(resolved.season);
        }

        show.episodeMarkers = show.episodeMarkers || resolved.episodeMarker;
        show.numberedEpisodes = show.numberedEpisodes || resolved.numberedEpisode;
        show.seasonFolders = show.seasonFolders || resolved.seasonFolder;
        if (resolved.titleFromFolder) {
            show.titleFromFolder = true;
            if (show.titleFolder.isEmpty()) {
                show.titleFolder = resolved.titleFolder;
            }
        }
    }

    shows.removeIf([](const Show &show) {
        if (!show.seasons.isEmpty()) {
            return false;
        }
        return show.fileHandles.size() < 2
            || (show.titleFromFolder && namesAFilmSet(show.titleFolder));
    });

    for (Show &show : shows) {
        std::sort(show.seasons.begin(), show.seasons.end());
    }

    std::sort(shows.begin(), shows.end(), [](const Show &a, const Show &b) {
        return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
    });

    return shows;
}

}
