#include "Library/UnmatchedShowModel.h"

#include "MmLog.h"
#include "Library/ListPatch.h"

#include <QSet>

#include <algorithm>
#include <functional>
#include <utility>

UnmatchedShowModel::UnmatchedShowModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int UnmatchedShowModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_shows.size());
}

QVariant UnmatchedShowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_shows.size()) {
        return QVariant();
    }

    const UnmatchedShow &show = m_shows.at(index.row());

    switch (role) {
    case TitleRole:
        return show.title;
    case FileCountRole:
        return show.fileCount;
    case SeasonsRole:
        return show.seasons;
    case FileHandlesRole:
        return show.fileHandles;
    case FolderHandlesRole:
        return show.folderHandles;
    case FileNamesRole:
        return show.fileNames;
    case FolderPathRole:
        return show.folderPath;
    case TitleFromFolderRole:
        return show.titleFromFolder;
    case TitleFolderRole:
        return show.titleFolder;
    case EpisodeMarkersRole:
        return show.episodeMarkers;
    case NumberedEpisodesRole:
        return show.numberedEpisodes;
    case SeasonFoldersRole:
        return show.seasonFolders;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> UnmatchedShowModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {FileCountRole, "fileCount"},
        {SeasonsRole, "seasons"},
        {FileHandlesRole, "fileHandles"},
        {FolderHandlesRole, "folderHandles"},
        {FileNamesRole, "fileNames"},
        {FolderPathRole, "folderPath"},
        {TitleFromFolderRole, "titleFromFolder"},
        {TitleFolderRole, "titleFolder"},
        {EpisodeMarkersRole, "episodeMarkers"},
        {NumberedEpisodesRole, "numberedEpisodes"},
        {SeasonFoldersRole, "seasonFolders"}
    };
}

void UnmatchedShowModel::setShows(const QList<ShowGrouping::Show> &shows,
                                  const QHash<QString, QString> &folderPaths)
{
    QList<UnmatchedShow> next;
    next.reserve(shows.size());
    for (const ShowGrouping::Show &show : shows) {
        UnmatchedShow row;
        row.title = show.title;
        row.fileCount = int(show.fileHandles.size());
        for (const int season : show.seasons) {
            row.seasons.append(season);
        }
        row.fileHandles = show.fileHandles;
        row.folderHandles = show.folderHandles;
        row.fileNames = show.fileNames;
        row.titleFromFolder = show.titleFromFolder;
        row.titleFolder = show.titleFolder;
        row.episodeMarkers = show.episodeMarkers;
        row.numberedEpisodes = show.numberedEpisodes;
        row.seasonFolders = show.seasonFolders;
        for (const QString &folder : show.folderHandles) {
            const QString path = folderPaths.value(folder, folder);
            if (!path.isEmpty()) {
                row.folderPath = path;
                break;
            }
        }
        next.append(row);
    }

    if (m_shows == next) {
        return;
    }

    QStringList before;
    before.reserve(m_shows.size());
    for (const UnmatchedShow &show : std::as_const(m_shows)) {
        before.append(show.title);
    }

    QStringList after;
    after.reserve(next.size());
    for (const UnmatchedShow &show : std::as_const(next)) {
        after.append(show.title);
    }

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    if (!ListPatch::worthPatching(steps, int(m_shows.size()), int(next.size()))) {
        beginResetModel();
        m_shows = next;
        reindex();
        endResetModel();
        emit countChanged();
        return;
    }

    const qsizetype had = m_shows.size();
    int firstChanged = -1;
    int lastChanged = -1;

    const auto flushChanged = [this, &firstChanged, &lastChanged]() {
        if (firstChanged < 0) {
            return;
        }
        emit dataChanged(index(firstChanged, 0), index(lastChanged, 0));
        firstChanged = -1;
        lastChanged = -1;
    };

    for (const ListPatch::Step &step : steps) {
        switch (step.kind) {
        case ListPatch::Kind::Update:
            if (m_shows.at(step.to) != next.at(step.to)) {
                m_shows[step.to] = next.at(step.to);
                if (firstChanged < 0) {
                    firstChanged = step.to;
                }
                lastChanged = step.to;
            } else {
                flushChanged();
            }
            break;

        case ListPatch::Kind::Remove:
            flushChanged();
            beginRemoveRows(QModelIndex(), step.from, step.from + step.count - 1);
            m_shows.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            flushChanged();
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_shows.insert(step.to + i, next.at(step.to + i));
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            flushChanged();
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(), step.to);
            m_shows.move(step.from, step.to);
            endMoveRows();
            if (m_shows.at(step.to) != next.at(step.to)) {
                m_shows[step.to] = next.at(step.to);
                const QModelIndex moved = index(step.to, 0);
                emit dataChanged(moved, moved);
            }
            break;
        }
    }

    flushChanged();
    reindex();

    if (m_shows.size() != had) {
        emit countChanged();
    }
}

void UnmatchedShowModel::removeFiles(const QStringList &fileHandles)
{
    if (fileHandles.isEmpty() || m_shows.isEmpty()) {
        return;
    }

    QHash<int, QStringList> byRow;
    for (const QString &handle : fileHandles) {
        const QString title = m_titleByHandle.take(handle);
        if (title.isEmpty()) {
            continue;
        }
        const int row = m_rowsByTitle.value(title, -1);
        if (row >= 0) {
            byRow[row].append(handle);
        }
    }

    if (byRow.isEmpty()) {
        return;
    }

    QList<int> rows = byRow.keys();
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    int emptied = 0;
    for (const int row : std::as_const(rows)) {
        const QStringList &gone = byRow[row];
        const QSet<QString> drop(gone.cbegin(), gone.cend());

        UnmatchedShow show = m_shows.at(row);
        QStringList kept;
        kept.reserve(show.fileHandles.size());
        for (const QString &handle : std::as_const(show.fileHandles)) {
            if (!drop.contains(handle)) {
                kept.append(handle);
            }
        }

        if (kept.isEmpty()) {
            beginRemoveRows(QModelIndex(), row, row);
            m_shows.removeAt(row);
            endRemoveRows();
            ++emptied;
            continue;
        }

        show.fileHandles = kept;
        show.fileCount = int(kept.size());
        m_shows[row] = show;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed);
    }

    if (emptied > 0) {
        reindexRows();
    }

    MM_LOG_D() << "matched files left" << byRow.size() << "unmatched shows,"
               << emptied << "of them now empty";

    if (emptied > 0) {
        emit countChanged();
    }
}

void UnmatchedShowModel::reindex()
{
    m_titleByHandle.clear();
    for (const UnmatchedShow &show : std::as_const(m_shows)) {
        for (const QString &handle : show.fileHandles) {
            m_titleByHandle.insert(handle, show.title);
        }
    }

    reindexRows();
}

void UnmatchedShowModel::reindexRows()
{
    m_rowsByTitle.clear();
    m_rowsByTitle.reserve(m_shows.size());
    for (int row = 0; row < m_shows.size(); ++row) {
        m_rowsByTitle.insert(m_shows.at(row).title, row);
    }
}
