#pragma once

#include "Streaming/Mirror.h"

#include <QAbstractListModel>
#include <QList>

class StoredMirrorModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        ServerIdRole = Qt::UserRole + 1,
        NameRole,
        BytesRole
    };

    explicit StoredMirrorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMirrors(const QList<Mirror::Stored> &mirrors);
    void upsert(const Mirror::Stored &mirror);
    void remove(const QString &serverId);

signals:
    void countChanged();

private:
    int rowOf(const QString &serverId) const;

    QList<Mirror::Stored> m_mirrors;
};
