#pragma once

#include "Streaming/SessionTable.h"

#include <QAbstractListModel>
#include <QList>

class ConnectedDeviceModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        NameRole,
        FormRole,
        PeerRole,
        TitleRole,
        PosterPathRole,
        BackdropPathRole,
        PositionRole,
        DurationRole,
        PausedRole,
        WatchingRole
    };

    explicit ConnectedDeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void apply(const QList<StreamDevice> &devices);
    int watchingCount() const;

signals:
    void countChanged();

private:
    int rowOf(const QString &deviceId) const;

    QList<StreamDevice> m_devices;
};
