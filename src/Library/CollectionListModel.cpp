#include "Library/CollectionListModel.h"

#include "Library/ListPatch.h"

#include <QObject>
#include <QSet>

#include <utility>

CollectionListModel::CollectionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CollectionListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QList<CollectionShelf::Summary> CollectionListModel::items() const
{
    return m_items;
}

int CollectionListModel::stampFor(const CollectionShelf::Summary &item) const
{
    int stamp = m_artworkEverything + m_artworkStamps.value(item.posterPath);
    for (const QString &path : item.behindPosters) {
        stamp += m_artworkStamps.value(path);
    }
    return stamp;
}

QVariant CollectionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }

    const CollectionShelf::Summary &item = m_items.at(index.row());

    switch (role) {
    case CollectionIdRole:
        return item.id;
    case NameRole:
        return item.name;
    case PosterRole:
        return item.posterPath;
    case BackdropRole:
        return item.backdropPath;
    case BehindPostersRole:
        return item.behindPosters;
    case TotalRole:
        return item.total;
    case OwnedRole:
        return item.owned;
    case MissingRole:
        return item.missing;
    case WatchedCountRole:
        return item.watched;
    case SummaryRole: {
        QStringList parts;
        parts << (item.total == 1 ? QObject::tr("1 film")
                                  : QObject::tr("%1 films").arg(item.total));
        if (item.missing > 0) {
            parts << QObject::tr("%1 missing").arg(item.missing);
        }
        const QString years = CollectionShelf::yearsText(item.firstYear, item.lastYear);
        if (!years.isEmpty()) {
            parts << years;
        }
        return parts.join(QStringLiteral(" · "));
    }
    case ProgressRole:
        return item.progress();
    case CompletedRole:
        return item.state == CollectionShelf::State::Completed;
    case CustomRole:
        return item.custom;
    case ArtworkStampRole:
        return stampFor(item);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> CollectionListModel::roleNames() const
{
    return {
        {CollectionIdRole, "collectionId"},
        {NameRole, "name"},
        {PosterRole, "posterPath"},
        {BackdropRole, "backdropPath"},
        {BehindPostersRole, "behindPosters"},
        {TotalRole, "totalCount"},
        {OwnedRole, "ownedCount"},
        {MissingRole, "missingCount"},
        {WatchedCountRole, "watchedCount"},
        {SummaryRole, "summary"},
        {ProgressRole, "watchProgress"},
        {CompletedRole, "completed"},
        {CustomRole, "custom"},
        {ArtworkStampRole, "artworkStamp"}
    };
}

void CollectionListModel::touchArtwork(const QStringList &paths, bool everything)
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
        const CollectionShelf::Summary &item = m_items.at(row);
        bool touched = landed.contains(item.posterPath);
        for (const QString &path : item.behindPosters) {
            touched = touched || landed.contains(path);
        }
        if (touched) {
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed, {ArtworkStampRole});
        }
    }
}

void CollectionListModel::setItems(const QList<CollectionShelf::Summary> &items)
{
    if (m_items == items) {
        return;
    }

    QList<qint64> before;
    before.reserve(m_items.size());
    for (const CollectionShelf::Summary &item : std::as_const(m_items)) {
        before.append(item.id);
    }

    QList<qint64> after;
    after.reserve(items.size());
    for (const CollectionShelf::Summary &item : items) {
        after.append(item.id);
    }

    const qsizetype had = m_items.size();
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    for (const ListPatch::Step &step : steps) {
        switch (step.kind) {
        case ListPatch::Kind::Update:
            if (m_items.at(step.to) != items.at(step.to)) {
                m_items[step.to] = items.at(step.to);
                const QModelIndex changed = index(step.to, 0);
                emit dataChanged(changed, changed);
            }
            break;

        case ListPatch::Kind::Remove:
            beginRemoveRows(QModelIndex(), step.from, step.from + step.count - 1);
            m_items.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_items.insert(step.to + i, items.at(step.to + i));
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(),
                          step.to > step.from ? step.to + 1 : step.to);
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

    if (m_items.size() != had) {
        emit countChanged();
    }
}
