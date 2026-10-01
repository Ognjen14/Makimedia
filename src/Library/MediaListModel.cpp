#include "Library/MediaListModel.h"

#include "Library/ListPatch.h"
#include "Library/RowPlacement.h"

#include <QObject>
#include <QSet>
#include <QStringList>

#include <utility>

MediaListModel::MediaListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MediaListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

MediaRecord MediaListModel::at(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return MediaRecord();
    }
    return m_items.at(row);
}

QVariant MediaListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }

    const MediaRecord &item = m_items.at(index.row());

    switch (role) {
    case MediaIdRole:
        return item.id;
    case TmdbIdRole:
        return item.tmdbId;
    case KindRole:
        return item.kind;
    case TitleRole:
        return item.title;
    case YearRole:
        return item.year;
    case PosterRole:
        return item.posterPath;
    case OverviewRole:
        return item.overview;
    case RatingRole:
        return item.rating;
    case GenresRole:
        return item.genres;
    case RuntimeRole:
        return item.runtimeMinutes;
    case FileCountRole:
        return item.fileCount;
    case SeasonCountRole:
        return item.seasonCount;
    case HandleRole:
        return item.firstFileHandle;
    case SummaryRole: {
        QStringList parts;
        if (item.year > 0) {
            parts << QString::number(item.year);
        }

        if (item.kind == QLatin1String("tv")) {
            if (item.seasonCount > 0) {
                parts << QObject::tr("%n season(s)", "", item.seasonCount);
            }
            parts << QObject::tr("%n episode(s) here", "", item.fileCount);
        } else if (item.runtimeMinutes > 0) {
            parts << QObject::tr("%1 min").arg(item.runtimeMinutes);
        }

        return parts.join(QStringLiteral("  ·  "));
    }
    case WatchedRole:
        return item.fileCount > 0 && item.watchedCount >= item.fileCount;
    case ProgressRole:
        if (item.kind == QLatin1String("tv")) {
            return item.fileCount > 0
                       ? double(item.watchedCount) / double(item.fileCount)
                       : 0.0;
        }
        return item.partialProgress;
    case ArtworkStampRole:
        return m_artworkEverything + m_artworkStamps.value(item.posterPath);
    case NewCountRole:
        return item.newFileCount;
    default:
        return QVariant();
    }
}

void MediaListModel::touchArtwork(const QStringList &paths, bool everything)
{
    if (everything) {
        ++m_artworkEverything;
        if (!m_items.isEmpty()) {
            emit dataChanged(index(0, 0), index(int(m_items.size()) - 1, 0),
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

    for (int row = 0; row < m_items.size(); ++row) {
        if (landed.contains(m_items.at(row).posterPath)) {
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed, {ArtworkStampRole});
        }
    }
}

QHash<int, QByteArray> MediaListModel::roleNames() const
{
    return {
        {MediaIdRole, "mediaId"},
        {TmdbIdRole, "tmdbId"},
        {KindRole, "kind"},
        {TitleRole, "title"},
        {YearRole, "year"},
        {PosterRole, "posterPath"},
        {OverviewRole, "overview"},
        {RatingRole, "rating"},
        {GenresRole, "genres"},
        {RuntimeRole, "runtimeMinutes"},
        {FileCountRole, "fileCount"},
        {SeasonCountRole, "seasonCount"},
        {HandleRole, "handle"},
        {SummaryRole, "summary"},
        {WatchedRole, "watched"},
        {ProgressRole, "watchProgress"},
        {ArtworkStampRole, "artworkStamp"},
        {NewCountRole, "newCount"}
    };
}

void MediaListModel::setItems(const QList<MediaRecord> &items)
{
    if (m_items == items) {
        return;
    }

    QList<qint64> before;
    before.reserve(m_items.size());
    for (const MediaRecord &item : std::as_const(m_items)) {
        before.append(item.id);
    }

    QList<qint64> after;
    after.reserve(items.size());
    for (const MediaRecord &item : items) {
        after.append(item.id);
    }

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    if (!ListPatch::worthPatching(steps, int(m_items.size()), int(items.size()))) {
        beginResetModel();
        m_items = items;
        m_rowsById.clear();
        m_rowsById.reserve(m_items.size());
        reindex(0, int(m_items.size()) - 1);
        endResetModel();
        emit countChanged();
        return;
    }

    const qsizetype had = m_items.size();
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
            if (m_items.at(step.to) != items.at(step.to)) {
                m_items[step.to] = items.at(step.to);
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
            m_items.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            flushChanged();
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_items.insert(step.to + i, items.at(step.to + i));
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            flushChanged();
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(), step.to);
            m_items.move(step.from, step.to);
            endMoveRows();
            if (m_items.at(step.to) != items.at(step.to)) {
                m_items[step.to] = items.at(step.to);
                const QModelIndex moved = index(step.to, 0);
                emit dataChanged(moved, moved);
            }
            break;
        }
    }

    flushChanged();

    m_rowsById.clear();
    m_rowsById.reserve(m_items.size());
    reindex(0, int(m_items.size()) - 1);

    if (m_items.size() != had) {
        emit countChanged();
    }
}

void MediaListModel::placeItem(const MediaRecord &item, bool keep)
{
    const int current = m_rowsById.value(item.id, -1);
    const RowPlacement::Step step = RowPlacement::plan(
        m_items, current, item, keep, &MediaRepository::listsBefore);

    switch (step.kind) {
    case RowPlacement::Kind::Nothing:
        return;

    case RowPlacement::Kind::Update:
        if (m_items.at(step.from) != item) {
            m_items[step.from] = item;
            const QModelIndex changed = index(step.from, 0);
            emit dataChanged(changed, changed);
        }
        return;

    case RowPlacement::Kind::Insert:
        beginInsertRows(QModelIndex(), step.to, step.to);
        m_items.insert(step.to, item);
        reindex(step.to, int(m_items.size()) - 1);
        endInsertRows();
        emit countChanged();
        return;

    case RowPlacement::Kind::Remove:
        beginRemoveRows(QModelIndex(), step.from, step.from);
        m_rowsById.remove(m_items.at(step.from).id);
        m_items.removeAt(step.from);
        reindex(step.from, int(m_items.size()) - 1);
        endRemoveRows();
        emit countChanged();
        return;

    case RowPlacement::Kind::Move: {
        beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(),
                      step.to > step.from ? step.to + 1 : step.to);
        m_items.move(step.from, step.to);
        m_items[step.to] = item;
        reindex(qMin(step.from, step.to), qMax(step.from, step.to));
        endMoveRows();
        const QModelIndex changed = index(step.to, 0);
        emit dataChanged(changed, changed);
        return;
    }
    }
}

void MediaListModel::reindex(int first, int last)
{
    for (int row = first; row <= last; ++row) {
        m_rowsById.insert(m_items.at(row).id, row);
    }
}
