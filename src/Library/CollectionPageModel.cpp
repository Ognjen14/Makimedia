#include "Library/CollectionPageModel.h"

#include "MmLog.h"
#include "Library/ListPatch.h"

#include <QSet>

#include <utility>

CollectionPageModel::CollectionPageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CollectionPageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant CollectionPageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const CollectionPage::Row &row = m_rows.at(index.row());

    switch (role) {
    case HeadingRole:
        return row.heading;
    case ShowRole:
        return row.show;
    case PhaseRole:
        return row.phase;
    case NumberRole:
        return row.number;
    case TmdbIdRole:
        return row.tmdbId;
    case TitleRole:
        return row.title;
    case YearRole:
        return row.year;
    case RuntimeMinutesRole:
        return row.runtimeMinutes;
    case RatingRole:
        return row.rating;
    case PosterPathRole:
        return row.posterPath;
    case OwnedRole:
        return row.owned;
    case AddedRole:
        return row.added;
    case HandleRole:
        return row.handle;
    case WatchedRole:
        return row.watched;
    case ProgressRole:
        return row.progress;
    case LeftMinutesRole:
        return row.leftMinutes();
    case ArtworkStampRole:
        return m_artworkEverything + m_artworkStamps.value(row.posterPath);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> CollectionPageModel::roleNames() const
{
    return {
        {HeadingRole, "heading"},
        {ShowRole, "isShow"},
        {PhaseRole, "phase"},
        {NumberRole, "number"},
        {TmdbIdRole, "tmdbId"},
        {TitleRole, "title"},
        {YearRole, "year"},
        {RuntimeMinutesRole, "runtimeMinutes"},
        {RatingRole, "rating"},
        {PosterPathRole, "posterPath"},
        {OwnedRole, "owned"},
        {AddedRole, "added"},
        {HandleRole, "handle"},
        {WatchedRole, "watched"},
        {ProgressRole, "progress"},
        {LeftMinutesRole, "leftMinutes"},
        {ArtworkStampRole, "artworkStamp"}
    };
}

qint64 CollectionPageModel::collectionId() const
{
    return m_collectionId;
}

void CollectionPageModel::setCollectionId(qint64 collectionId)
{
    if (m_collectionId == collectionId) {
        return;
    }
    m_collectionId = collectionId;
    emit collectionIdChanged();
}

int CollectionPageModel::order() const
{
    return m_order;
}

void CollectionPageModel::setOrder(int order)
{
    if (m_order == order) {
        return;
    }
    m_order = order;
    emit orderChanged();
}

QVariantMap CollectionPageModel::info() const
{
    return m_info;
}

void CollectionPageModel::setPage(const CollectionPage::Page &page)
{
    const CollectionPage::Header &header = page.header;

    QVariantMap info;
    info.insert(QStringLiteral("valid"), header.valid);
    info.insert(QStringLiteral("universe"), header.universe);
    info.insert(QStringLiteral("custom"), header.custom);
    info.insert(QStringLiteral("hasStoryOrder"), header.hasStoryOrder);
    info.insert(QStringLiteral("name"), header.name);
    info.insert(QStringLiteral("tag"), header.tag);
    info.insert(QStringLiteral("description"), header.description);
    info.insert(QStringLiteral("backdropPath"), header.backdropPath);
    info.insert(QStringLiteral("firstYear"), header.firstYear);
    info.insert(QStringLiteral("lastYear"), header.lastYear);
    info.insert(QStringLiteral("total"), header.total);
    info.insert(QStringLiteral("owned"), header.owned);
    info.insert(QStringLiteral("watched"), header.watched);
    info.insert(QStringLiteral("totalMinutes"), header.totalMinutes);
    info.insert(QStringLiteral("leftMinutes"), header.leftMinutes);
    info.insert(QStringLiteral("averageRating"), header.averageRating);
    info.insert(QStringLiteral("posters"), header.posters);
    info.insert(QStringLiteral("nextTitle"), header.nextTitle);
    info.insert(QStringLiteral("nextHandle"), header.nextHandle);
    info.insert(QStringLiteral("nextResumes"), header.nextResumes);
    info.insert(QStringLiteral("nextLeftMinutes"), header.nextLeftMinutes);
    info.insert(QStringLiteral("ownedHandles"), header.ownedHandles);
    info.insert(QStringLiteral("allOwnedWatched"), header.allOwnedWatched);

    if (info != m_info) {
        m_info = info;
        emit infoChanged();
    }

    const QList<CollectionPage::Row> &rows = page.rows;
    if (m_rows == rows) {
        return;
    }

    QList<QString> before;
    before.reserve(m_rows.size());
    for (const CollectionPage::Row &row : std::as_const(m_rows)) {
        before.append(row.key);
    }
    QList<QString> after;
    after.reserve(rows.size());
    QSet<QString> unique;
    for (const CollectionPage::Row &row : rows) {
        if (unique.contains(row.key)) {
            MM_LOG_E() << "collection page rows repeat the key" << row.key
                       << "- the list is left as it was";
            return;
        }
        unique.insert(row.key);
        after.append(row.key);
    }

    const qsizetype had = m_rows.size();
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    for (const ListPatch::Step &step : steps) {
        switch (step.kind) {
        case ListPatch::Kind::Update:
            if (m_rows.at(step.to) != rows.at(step.to)) {
                m_rows[step.to] = rows.at(step.to);
                const QModelIndex changed = index(step.to, 0);
                emit dataChanged(changed, changed);
            }
            break;

        case ListPatch::Kind::Remove:
            beginRemoveRows(QModelIndex(), step.from, step.from + step.count - 1);
            m_rows.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_rows.insert(step.to + i, rows.at(step.to + i));
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(), step.to);
            m_rows.move(step.from, step.to);
            endMoveRows();
            if (m_rows.at(step.to) != rows.at(step.to)) {
                m_rows[step.to] = rows.at(step.to);
                const QModelIndex moved = index(step.to, 0);
                emit dataChanged(moved, moved);
            }
            break;
        }
    }

    if (m_rows.size() != had) {
        emit countChanged();
    }
}

void CollectionPageModel::touchArtwork(const QStringList &paths, bool everything)
{
    if (everything) {
        ++m_artworkEverything;
        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0, 0), index(int(m_rows.size()) - 1, 0),
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
    for (int row = 0; row < m_rows.size(); ++row) {
        if (landed.contains(m_rows.at(row).posterPath)) {
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed, {ArtworkStampRole});
        }
    }
}
