#include "Library/RemovedFileModel.h"

#include <algorithm>
#include <functional>

RemovedFileModel::RemovedFileModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RemovedFileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_files.size());
}

QVariant RemovedFileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size()) {
        return QVariant();
    }

    const RemovedFileRecord &file = m_files.at(index.row());

    switch (role) {
    case HandleRole:
        return file.handle;
    case DisplayNameRole:
        return file.displayName;
    case FolderRole:
        return file.folder;
    case RemovedAtRole:
        return file.removedAt;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> RemovedFileModel::roleNames() const
{
    return {
        { HandleRole, "handle" },
        { DisplayNameRole, "displayName" },
        { FolderRole, "folder" },
        { RemovedAtRole, "removedAt" }
    };
}

void RemovedFileModel::setFiles(const QList<RemovedFileRecord> &files)
{
    beginResetModel();
    m_files = files;
    m_rowsByHandle.clear();
    reindex(0);
    endResetModel();
    emit countChanged();
}

void RemovedFileModel::prependFiles(const QList<RemovedFileRecord> &files)
{
    if (files.isEmpty()) {
        return;
    }

    QStringList replaced;
    for (const RemovedFileRecord &file : files) {
        if (m_rowsByHandle.contains(file.handle)) {
            replaced.append(file.handle);
        }
    }
    if (!replaced.isEmpty()) {
        removeHandles(replaced);
    }

    beginInsertRows(QModelIndex(), 0, int(files.size()) - 1);
    m_files = files + m_files;
    reindex(0);
    endInsertRows();
    emit countChanged();
}

void RemovedFileModel::removeHandles(const QStringList &handles)
{
    QList<int> rows;
    rows.reserve(handles.size());
    for (const QString &handle : handles) {
        const int row = m_rowsByHandle.value(handle, -1);
        if (row >= 0) {
            rows.append(row);
        }
    }
    if (rows.isEmpty()) {
        return;
    }

    std::sort(rows.begin(), rows.end(), std::greater<int>());

    const int lowest = rows.last();
    int at = 0;
    while (at < rows.size()) {
        int last = rows.at(at);
        int first = last;
        while (at + 1 < rows.size() && rows.at(at + 1) == first - 1) {
            ++at;
            first = rows.at(at);
        }
        beginRemoveRows(QModelIndex(), first, last);
        for (int row = last; row >= first; --row) {
            m_rowsByHandle.remove(m_files.at(row).handle);
        }
        m_files.remove(first, last - first + 1);
        endRemoveRows();
        ++at;
    }

    reindex(lowest);
    emit countChanged();
}

QStringList RemovedFileModel::handles() const
{
    QStringList result;
    result.reserve(m_files.size());
    for (const RemovedFileRecord &file : m_files) {
        result.append(file.handle);
    }
    return result;
}

void RemovedFileModel::reindex(int first)
{
    for (int row = first; row < m_files.size(); ++row) {
        m_rowsByHandle.insert(m_files.at(row).handle, row);
    }
}
