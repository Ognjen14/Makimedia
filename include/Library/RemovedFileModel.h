#pragma once

#include "Data/FileRepository.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class RemovedFileModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        HandleRole = Qt::UserRole + 1,
        DisplayNameRole,
        FolderRole,
        RemovedAtRole
    };

    explicit RemovedFileModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFiles(const QList<RemovedFileRecord> &files);
    void prependFiles(const QList<RemovedFileRecord> &files);
    void removeHandles(const QStringList &handles);
    QStringList handles() const;

signals:
    void countChanged();

private:
    void reindex(int first);

    QList<RemovedFileRecord> m_files;
    QHash<QString, int> m_rowsByHandle;
};
