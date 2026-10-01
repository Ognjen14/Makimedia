#include "Library/FolderListModel.h"

#include "Library/ListPatch.h"

#include <QLocale>

#include <utility>

FolderListModel::FolderListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FolderListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_folders.size());
}

QVariant FolderListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_folders.size()) {
        return QVariant();
    }

    const ScanFolder &folder = m_folders.at(index.row());

    switch (role) {
    case IdRole:
        return folder.id;
    case HandleRole:
        return folder.handle;
    case DisplayNameRole:
        return folder.displayName;
    case LastScannedRole:
        return folder.lastScanned.isValid()
            ? QLocale().toString(folder.lastScanned.toLocalTime(), QLocale::ShortFormat)
            : QString();
    case AvailableRole:
        return folder.available;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FolderListModel::roleNames() const
{
    return {
        {IdRole, "folderId"},
        {HandleRole, "handle"},
        {DisplayNameRole, "displayName"},
        {LastScannedRole, "lastScanned"},
        {AvailableRole, "available"}
    };
}

void FolderListModel::setFolders(const QList<ScanFolder> &folders)
{
    if (m_folders == folders) {
        return;
    }

    QList<qint64> before;
    before.reserve(m_folders.size());
    for (const ScanFolder &folder : std::as_const(m_folders)) {
        before.append(folder.id);
    }

    QList<qint64> after;
    after.reserve(folders.size());
    for (const ScanFolder &folder : folders) {
        after.append(folder.id);
    }

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    if (!ListPatch::worthPatching(steps, int(m_folders.size()), int(folders.size()))) {
        beginResetModel();
        m_folders = folders;
        endResetModel();
        emit countChanged();
        return;
    }

    const qsizetype had = m_folders.size();
    int firstChanged = -1;
    int lastChanged = -1;

    const auto flushChanged = [this, &firstChanged, &lastChanged]() {
        if (firstChanged < 0) {
            return;
        }
        emit dataChanged(index(firstChanged, 0), index(lastChanged, 0));
        firstChanged = -1;
        lastChanged = -1;
    };

    for (const ListPatch::Step &step : steps) {
        switch (step.kind) {
        case ListPatch::Kind::Update:
            if (m_folders.at(step.to) != folders.at(step.to)) {
                m_folders[step.to] = folders.at(step.to);
                if (firstChanged < 0) {
                    firstChanged = step.to;
                }
                lastChanged = step.to;
            } else {
                flushChanged();
            }
            break;

        case ListPatch::Kind::Remove:
            flushChanged();
            beginRemoveRows(QModelIndex(), step.from, step.from + step.count - 1);
            m_folders.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            flushChanged();
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_folders.insert(step.to + i, folders.at(step.to + i));
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            flushChanged();
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(), step.to);
            m_folders.move(step.from, step.to);
            endMoveRows();
            if (m_folders.at(step.to) != folders.at(step.to)) {
                m_folders[step.to] = folders.at(step.to);
                const QModelIndex moved = index(step.to, 0);
                emit dataChanged(moved, moved);
            }
            break;
        }
    }

    flushChanged();

    if (m_folders.size() != had) {
        emit countChanged();
    }
}
