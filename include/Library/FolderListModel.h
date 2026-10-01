#pragma once

#include "Data/ScanFolder.h"

#include <QAbstractListModel>
#include <QList>

class FolderListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        HandleRole,
        DisplayNameRole,
        LastScannedRole,
        AvailableRole
    };

    explicit FolderListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFolders(const QList<ScanFolder> &folders);

signals:
    void countChanged();

private:
    QList<ScanFolder> m_folders;
};
