#include "Library/FileListModel.h"

#include "Library/ListPatch.h"
#include "Library/ListText.h"
#include "Library/RowPlacement.h"
#include "Library/ThumbnailService.h"

#include <QLocale>
#include <QSet>

#include <utility>

FileListModel::FileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_files.size());
}

LibraryFile FileListModel::at(int row) const
{
    if (row < 0 || row >= m_files.size()) {
        return LibraryFile();
    }
    return m_files.at(row);
}

LibraryFile FileListModel::fileWithHandle(const QString &handle) const
{
    const int row = m_rowsByHandle.value(handle, -1);
    return row < 0 ? LibraryFile() : m_files.at(row);
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size()) {
        return QVariant();
    }

    const LibraryFile &file = m_files.at(index.row());

    switch (role) {
    case IdRole:
        return file.id;
    case HandleRole:
        return file.handle;
    case ParentHandleRole:
        return file.parentHandle;
    case DisplayNameRole:
        return file.isMatched() ? file.matchedTitle : file.displayName;
    case MatchedYearRole:
        return file.matchedYear;
    case SuggestedRole:
        return file.matchSuggested;
    case SeasonRole:
        return file.season;
    case EpisodeRole:
        return file.episode;
    case EpisodeTitleRole:
        return file.episodeTitle;
    case EpisodeLabelRole: {
        if (!file.isEpisode()) {
            return QString();
        }
        const QString code = QStringLiteral("S%1E%2")
                                 .arg(file.season, 2, 10, QLatin1Char('0'))
                                 .arg(file.episode, 2, 10, QLatin1Char('0'));
        return file.episodeTitle.isEmpty()
            ? code
            : code + QStringLiteral(" · ") + file.episodeTitle;
    }
    case SizeTextRole:
        return file.sizeBytes > 0
            ? QLocale().formattedDataSize(file.sizeBytes, 1, QLocale::DataSizeIecFormat)
            : QString();
    case DurationTextRole:
        return ListText::duration(file.durationSeconds);
    case ProgressRole:
        return file.playback.progress();
    case WatchedRole:
        return file.playback.watched;
    case MissingRole:
        return file.missing;
    case ResolutionRole:
        return (file.width > 0 && file.height > 0)
            ? QStringLiteral("%1x%2").arg(file.width).arg(file.height)
            : QString();
    case RemainingTextRole:
        return ListText::remaining(file);
    case UnmatchedRole:
        return !file.isMatched();
    case LocationTextRole:
        return ListText::location(file.parentHandle);
    case PosterRole:
        return file.matchedPosterPath;
    case BackdropRole:
        return file.matchedBackdropPath;
    case StillRole:
        return file.episodeStillPath;
    case TitleTextRole:
        return titleTextFor(file);
    case ArtworkStampRole:
        return m_artworkEverything
            + m_artworkStamps.value(file.matchedPosterPath)
            + m_artworkStamps.value(file.matchedBackdropPath)
            + m_artworkStamps.value(file.episodeStillPath);
    case ThumbnailRole: {
        if (!m_thumbnails || file.missing) {
            return QString();
        }

        if (!file.matchedPosterPath.isEmpty()) {
            return QString();
        }

        return m_thumbnails->cachedUrl(file.handle);
    }
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    return {
        {IdRole, "fileId"},
        {HandleRole, "handle"},
        {ParentHandleRole, "parentHandle"},
        {DisplayNameRole, "displayName"},
        {SizeTextRole, "sizeText"},
        {DurationTextRole, "durationText"},
        {ProgressRole, "watchProgress"},
        {WatchedRole, "watched"},
        {MissingRole, "missing"},
        {ResolutionRole, "resolution"},
        {RemainingTextRole, "remainingText"},
        {UnmatchedRole, "isUnmatched"},
        {LocationTextRole, "locationText"},
        {ThumbnailRole, "thumbnail"},
        {PosterRole, "posterPath"},
        {BackdropRole, "backdropPath"},
        {StillRole, "stillPath"},
        {MatchedYearRole, "matchedYear"},
        {SuggestedRole, "matchSuggested"},
        {SeasonRole, "season"},
        {EpisodeRole, "episode"},
        {EpisodeTitleRole, "episodeTitle"},
        {EpisodeLabelRole, "episodeLabel"},
        {TitleTextRole, "titleText"},
        {ArtworkStampRole, "artworkStamp"}
    };
}

QString FileListModel::titleTextFor(const LibraryFile &file) const
{
    if (file.isMatched()) {
        return file.matchedTitle;
    }

    const auto cached = m_prettyTitles.constFind(file.displayName);
    if (cached != m_prettyTitles.constEnd()) {
        return cached.value();
    }

    const QString title = ListText::prettyTitle(file.displayName);
    m_prettyTitles.insert(file.displayName, title);
    return title;
}

void FileListModel::touchArtwork(const QStringList &paths, bool everything)
{
    if (everything) {
        ++m_artworkEverything;
        if (!m_files.isEmpty()) {
            emit dataChanged(index(0, 0), index(int(m_files.size()) - 1, 0),
                             {ArtworkStampRole});
        }
        return;
    }

    if (paths.isEmpty()) {
        return;
    }

    const QSet<QString> landed(paths.cbegin(), paths.cend());
    for (const QString &path : landed) {
        ++m_artworkStamps[path];
    }

    int first = -1;
    int last = -1;
    for (int row = 0; row < m_files.size(); ++row) {
        const LibraryFile &file = m_files.at(row);
        const bool hit = landed.contains(file.matchedPosterPath)
            || landed.contains(file.matchedBackdropPath)
            || landed.contains(file.episodeStillPath);
        if (hit) {
            if (first < 0) {
                first = row;
            }
            last = row;
            continue;
        }
        if (first >= 0) {
            emit dataChanged(index(first, 0), index(last, 0), {ArtworkStampRole});
            first = -1;
        }
    }
    if (first >= 0) {
        emit dataChanged(index(first, 0), index(last, 0), {ArtworkStampRole});
    }
}

void FileListModel::setThumbnailService(ThumbnailService *service)
{
    m_thumbnails = service;
}

QString FileListModel::handleAt(int row) const
{
    if (row < 0 || row >= m_files.size()) {
        return QString();
    }
    return m_files.at(row).handle;
}

bool FileListModel::wantsThumbnailAt(int row) const
{
    if (row < 0 || row >= m_files.size()) {
        return false;
    }
    const LibraryFile &file = m_files.at(row);
    return !file.missing && file.matchedPosterPath.isEmpty();
}

void FileListModel::onThumbnailReady(const QString &handle)
{
    const int row = m_rowsByHandle.value(handle, -1);
    if (row < 0) {
        return;
    }
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {ThumbnailRole});
}

bool FileListModel::updateFile(const LibraryFile &file)
{
    const int row = m_rowsByHandle.value(file.handle, -1);
    if (row < 0) {
        return false;
    }

    if (m_files.at(row) != file) {
        m_files[row] = file;
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed);
    }
    return true;
}

bool FileListModel::removeFile(const QString &handle)
{
    const int row = m_rowsByHandle.value(handle, -1);
    if (row < 0) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_rowsByHandle.remove(handle);
    m_files.removeAt(row);
    reindex(row, int(m_files.size()) - 1);
    endRemoveRows();
    emit countChanged();
    return true;
}

void FileListModel::placeFile(const LibraryFile &file, bool keep, const Order &isBefore)
{
    const int current = m_rowsByHandle.value(file.handle, -1);
    const RowPlacement::Step step =
        RowPlacement::plan(m_files, current, file, keep, isBefore);

    switch (step.kind) {
    case RowPlacement::Kind::Nothing:
        return;

    case RowPlacement::Kind::Update:
        updateFile(file);
        return;

    case RowPlacement::Kind::Insert:
        beginInsertRows(QModelIndex(), step.to, step.to);
        m_files.insert(step.to, file);
        reindex(step.to, int(m_files.size()) - 1);
        endInsertRows();
        emit countChanged();
        return;

    case RowPlacement::Kind::Remove:
        beginRemoveRows(QModelIndex(), step.from, step.from);
        m_rowsByHandle.remove(m_files.at(step.from).handle);
        m_files.removeAt(step.from);
        reindex(step.from, int(m_files.size()) - 1);
        endRemoveRows();
        emit countChanged();
        return;

    case RowPlacement::Kind::Move: {
        beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(),
                      step.to > step.from ? step.to + 1 : step.to);
        m_files.move(step.from, step.to);
        m_files[step.to] = file;
        reindex(qMin(step.from, step.to), qMax(step.from, step.to));
        endMoveRows();
        const QModelIndex changed = index(step.to, 0);
        emit dataChanged(changed, changed);
        return;
    }
    }
}

void FileListModel::reindex(int first, int last)
{
    for (int row = first; row <= last; ++row) {
        m_rowsByHandle.insert(m_files.at(row).handle, row);
    }
}

void FileListModel::setFiles(const QList<LibraryFile> &files)
{
    if (m_files == files) {
        return;
    }

    QStringList before;
    before.reserve(m_files.size());
    for (const LibraryFile &file : std::as_const(m_files)) {
        before.append(file.handle);
    }

    QStringList after;
    after.reserve(files.size());
    for (const LibraryFile &file : files) {
        after.append(file.handle);
    }

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    if (!ListPatch::worthPatching(steps, int(m_files.size()), int(files.size()))) {
        beginResetModel();
        m_files = files;
        m_rowsByHandle.clear();
        m_rowsByHandle.reserve(m_files.size());
        reindex(0, int(m_files.size()) - 1);
        endResetModel();
        emit countChanged();
        return;
    }

    const qsizetype had = m_files.size();
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
            if (m_files.at(step.to) != files.at(step.to)) {
                m_files[step.to] = files.at(step.to);
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
            m_files.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            flushChanged();
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_files.insert(step.to + i, files.at(step.to + i));
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            flushChanged();
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(), step.to);
            m_files.move(step.from, step.to);
            endMoveRows();
            if (m_files.at(step.to) != files.at(step.to)) {
                m_files[step.to] = files.at(step.to);
                const QModelIndex moved = index(step.to, 0);
                emit dataChanged(moved, moved);
            }
            break;
        }
    }

    flushChanged();

    m_rowsByHandle.clear();
    m_rowsByHandle.reserve(m_files.size());
    reindex(0, int(m_files.size()) - 1);

    if (m_files.size() != had) {
        emit countChanged();
    }
}
