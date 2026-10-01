#include "Library/GenreRowModel.h"

#include "Library/ListPatch.h"
#include "Library/MediaListModel.h"

GenreRowModel::GenreRowModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GenreRowModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant GenreRowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Entry &entry = m_rows.at(index.row());
    switch (role) {
    case GenreRole:
        return entry.genre;
    case TitleCountRole:
        return entry.titles ? entry.titles->rowCount() : 0;
    case TitlesRole:
        return QVariant::fromValue<QObject *>(entry.titles);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> GenreRowModel::roleNames() const
{
    return {
        {GenreRole, "genre"},
        {TitleCountRole, "titleCount"},
        {TitlesRole, "titles"}
    };
}

qint64 GenreRowModel::keyOf(const QString &genre)
{
    auto found = m_keys.constFind(genre);
    if (found == m_keys.constEnd()) {
        found = m_keys.insert(genre, m_nextKey++);
    }
    return found.value();
}

void GenreRowModel::setRows(const QList<GenreShelf::Row> &rows)
{
    QList<qint64> before;
    before.reserve(m_rows.size());
    for (const Entry &entry : std::as_const(m_rows)) {
        before.append(keyOf(entry.genre));
    }

    QList<qint64> after;
    after.reserve(rows.size());
    for (const GenreShelf::Row &row : rows) {
        after.append(keyOf(row.genre));
    }

    const qsizetype had = m_rows.size();
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    for (const ListPatch::Step &step : steps) {
        switch (step.kind) {
        case ListPatch::Kind::Remove:
            beginRemoveRows(QModelIndex(), step.from, step.from + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_rows.at(step.from + i).titles->deleteLater();
            }
            m_rows.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                const GenreShelf::Row &row = rows.at(step.to + i);
                auto *titles = new MediaListModel(this);
                titles->setItems(row.titles);
                m_rows.insert(step.to + i, {row.genre, titles});
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(),
                          step.to > step.from ? step.to + 1 : step.to);
            m_rows.move(step.from, step.to);
            endMoveRows();
            break;

        case ListPatch::Kind::Update:
            break;
        }
    }

    for (int row = 0; row < m_rows.size(); ++row) {
        const int countBefore = m_rows.at(row).titles->rowCount();
        m_rows.at(row).titles->setItems(rows.at(row).titles);
        if (m_rows.at(row).titles->rowCount() != countBefore) {
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed, {TitleCountRole});
        }
    }

    if (m_rows.size() != had) {
        emit countChanged();
    }
}

void GenreRowModel::touchArtwork(const QStringList &paths, bool everything)
{
    for (const Entry &entry : std::as_const(m_rows)) {
        entry.titles->touchArtwork(paths, everything);
    }
}
