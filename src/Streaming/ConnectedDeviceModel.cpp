#include "Streaming/ConnectedDeviceModel.h"

#include <QSet>

ConnectedDeviceModel::ConnectedDeviceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConnectedDeviceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_devices.size());
}

QVariant ConnectedDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size()) {
        return QVariant();
    }
    const StreamDevice &device = m_devices.at(index.row());
    switch (role) {
    case DeviceIdRole: return device.deviceId;
    case NameRole: return device.name;
    case FormRole: return device.form;
    case PeerRole: return device.peer;
    case TitleRole: return device.title;
    case PosterPathRole: return device.posterPath;
    case BackdropPathRole: return device.backdropPath;
    case PositionRole: return device.position;
    case DurationRole: return device.duration;
    case PausedRole: return device.paused;
    case WatchingRole: return device.watching();
    default: return QVariant();
    }
}

QHash<int, QByteArray> ConnectedDeviceModel::roleNames() const
{
    return {
        { DeviceIdRole, "deviceId" },
        { NameRole, "name" },
        { FormRole, "form" },
        { PeerRole, "peer" },
        { TitleRole, "title" },
        { PosterPathRole, "posterPath" },
        { BackdropPathRole, "backdropPath" },
        { PositionRole, "position" },
        { DurationRole, "duration" },
        { PausedRole, "paused" },
        { WatchingRole, "watching" }
    };
}

int ConnectedDeviceModel::rowOf(const QString &deviceId) const
{
    for (int row = 0; row < m_devices.size(); ++row) {
        if (m_devices.at(row).deviceId == deviceId) {
            return row;
        }
    }
    return -1;
}

void ConnectedDeviceModel::apply(const QList<StreamDevice> &devices)
{
    const int countBefore = int(m_devices.size());

    QSet<QString> wanted;
    for (const StreamDevice &device : devices) {
        wanted.insert(device.deviceId);
    }
    for (int row = int(m_devices.size()) - 1; row >= 0; --row) {
        if (!wanted.contains(m_devices.at(row).deviceId)) {
            beginRemoveRows(QModelIndex(), row, row);
            m_devices.removeAt(row);
            endRemoveRows();
        }
    }

    for (const StreamDevice &device : devices) {
        const int row = rowOf(device.deviceId);
        if (row < 0) {
            const int at = int(m_devices.size());
            beginInsertRows(QModelIndex(), at, at);
            m_devices.append(device);
            endInsertRows();
            continue;
        }
        if (!(m_devices.at(row) == device)) {
            m_devices[row] = device;
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed);
        }
    }

    if (countBefore != m_devices.size()) {
        emit countChanged();
    }
}

int ConnectedDeviceModel::watchingCount() const
{
    int watching = 0;
    for (const StreamDevice &device : m_devices) {
        if (device.watching()) {
            ++watching;
        }
    }
    return watching;
}
