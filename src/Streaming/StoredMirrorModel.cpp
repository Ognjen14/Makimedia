#include "Streaming/StoredMirrorModel.h"

StoredMirrorModel::StoredMirrorModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int StoredMirrorModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return int(m_mirrors.size());
}

QVariant StoredMirrorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_mirrors.size()) {
        return QVariant();
    }

    const Mirror::Stored &mirror = m_mirrors.at(index.row());
    switch (role) {
    case ServerIdRole:
        return mirror.serverId;
    case NameRole:
        return mirror.name.isEmpty() ? mirror.serverId : mirror.name;
    case BytesRole:
        return double(mirror.bytes);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> StoredMirrorModel::roleNames() const
{
    return {
        { ServerIdRole, "serverId" },
        { NameRole, "name" },
        { BytesRole, "bytes" }
    };
}

int StoredMirrorModel::rowOf(const QString &serverId) const
{
    for (int row = 0; row < m_mirrors.size(); ++row) {
        if (m_mirrors.at(row).serverId == serverId) {
            return row;
        }
    }
    return -1;
}

void StoredMirrorModel::setMirrors(const QList<Mirror::Stored> &mirrors)
{
    beginResetModel();
    m_mirrors = mirrors;
    endResetModel();
    emit countChanged();
}

void StoredMirrorModel::upsert(const Mirror::Stored &mirror)
{
    const int row = rowOf(mirror.serverId);
    if (row < 0) {
        const int at = int(m_mirrors.size());
        beginInsertRows(QModelIndex(), at, at);
        m_mirrors.append(mirror);
        endInsertRows();
        emit countChanged();
        return;
    }
    m_mirrors[row] = mirror;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed);
}

void StoredMirrorModel::remove(const QString &serverId)
{
    const int row = rowOf(serverId);
    if (row < 0) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_mirrors.removeAt(row);
    endRemoveRows();
    emit countChanged();
}
