#include "Streaming/FoundServerModel.h"

FoundServerModel::FoundServerModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FoundServerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return int(m_servers.size());
}

QVariant FoundServerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_servers.size()) {
        return QVariant();
    }

    const FoundServer &server = m_servers.at(index.row());
    switch (role) {
    case ServerIdRole:
        return server.serverId;
    case NameRole:
        return server.name;
    case HostRole:
        return server.host;
    case PortRole:
        return int(server.port);
    case SelectedRole:
        return server.serverId == m_selectedId;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FoundServerModel::roleNames() const
{
    return {
        { ServerIdRole, "serverId" },
        { NameRole, "name" },
        { HostRole, "host" },
        { PortRole, "port" },
        { SelectedRole, "selected" }
    };
}

int FoundServerModel::rowOf(const QString &serverId) const
{
    for (int row = 0; row < m_servers.size(); ++row) {
        if (m_servers.at(row).serverId == serverId) {
            return row;
        }
    }
    return -1;
}

bool FoundServerModel::upsert(const FoundServer &server)
{
    const int row = rowOf(server.serverId);
    if (row < 0) {
        const int at = int(m_servers.size());
        beginInsertRows(QModelIndex(), at, at);
        m_servers.append(server);
        endInsertRows();
        emit countChanged();
        return true;
    }

    FoundServer &known = m_servers[row];
    known.lastSeenMs = qMax(known.lastSeenMs, server.lastSeenMs);
    if (known.name == server.name && known.host == server.host
        && known.port == server.port) {
        return false;
    }
    known.name = server.name;
    known.host = server.host;
    known.port = server.port;
    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed);
    return false;
}

const FoundServer *FoundServerModel::find(const QString &serverId) const
{
    const int row = rowOf(serverId);
    return row < 0 ? nullptr : &m_servers.at(row);
}

QStringList FoundServerModel::expire(qint64 nowMs, qint64 maxAgeMs, const QString &keepId)
{
    QStringList gone;
    for (int row = int(m_servers.size()) - 1; row >= 0; --row) {
        const FoundServer &server = m_servers.at(row);
        if (server.serverId == keepId || nowMs - server.lastSeenMs <= maxAgeMs) {
            continue;
        }
        gone.append(server.serverId);
        beginRemoveRows(QModelIndex(), row, row);
        m_servers.removeAt(row);
        endRemoveRows();
    }
    if (!gone.isEmpty()) {
        if (gone.contains(m_selectedId)) {
            m_selectedId.clear();
        }
        emit countChanged();
    }
    return gone;
}

QString FoundServerModel::firstId() const
{
    return m_servers.isEmpty() ? QString() : m_servers.first().serverId;
}

void FoundServerModel::setSelected(const QString &serverId)
{
    if (m_selectedId == serverId) {
        return;
    }
    const int before = rowOf(m_selectedId);
    m_selectedId = serverId;
    const int after = rowOf(m_selectedId);
    for (const int row : { before, after }) {
        if (row >= 0) {
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed, { SelectedRole });
        }
    }
}
