#include "Library/BrowseListModel.h"

#include "Library/ListPatch.h"
#include "Library/ThumbnailService.h"

#include <QLocale>

#include <utility>

namespace {

QString sizeText(qint64 bytes)
{
    if (bytes <= 0) {
        return QString();
    }
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeIecFormat);
}

QString clockText(double seconds)
{
    if (seconds <= 0.0) {
        return QString();
    }
    const int total = static_cast<int>(seconds);
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int secs = total % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours)
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

}

BrowseListModel::BrowseListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BrowseListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant BrowseListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const BrowseEntry &entry = m_entries.at(index.row());

    switch (role) {
    case HandleRole:
        return entry.handle;
    case DisplayNameRole:
        return entry.displayName;
    case IsFolderRole:
        return entry.isFolder;
    case SummaryRole: {
        QStringList parts;
        if (entry.isFolder) {
            if (entry.childFileCount > 0) {
                parts << QObject::tr("%n file(s)", "", entry.childFileCount);
            }
            const QString bytes = sizeText(entry.childBytes);
            if (!bytes.isEmpty()) {
                parts << bytes;
            }
            if (parts.isEmpty()) {
                parts << QObject::tr("Not indexed");
            }
        } else {
            const QString bytes = sizeText(entry.sizeBytes);
            if (!bytes.isEmpty()) {
                parts << bytes;
            }
            if (!entry.indexed) {
                parts << QObject::tr("not in library");
            }
        }
        return parts.join(QStringLiteral("  ·  "));
    }
    case DurationTextRole:
        return clockText(entry.durationSeconds);
    case WatchProgressRole:
        return entry.progress;
    case WatchedRole:
        return entry.watched;
    case IndexedRole:
        return entry.indexed;
    case ThumbnailRole: {
        if (!m_thumbnails || entry.isFolder) {
            return QString();
        }

        if (!entry.matchedPosterPath.isEmpty()) {
            return QString();
        }

        return m_thumbnails->cachedUrl(entry.handle);
    }
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> BrowseListModel::roleNames() const
{
    return {
        {HandleRole, "handle"},
        {DisplayNameRole, "displayName"},
        {IsFolderRole, "isFolder"},
        {SummaryRole, "summary"},
        {DurationTextRole, "durationText"},
        {WatchProgressRole, "watchProgress"},
        {WatchedRole, "watched"},
        {IndexedRole, "indexed"},
        {ThumbnailRole, "thumbnail"}
    };
}

void BrowseListModel::setThumbnailService(ThumbnailService *service)
{
    m_thumbnails = service;
}

QString BrowseListModel::handleAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return QString();
    }
    return m_entries.at(row).handle;
}

bool BrowseListModel::wantsThumbnailAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return false;
    }
    const BrowseEntry &entry = m_entries.at(row);
    return !entry.isFolder && entry.matchedPosterPath.isEmpty();
}

void BrowseListModel::onThumbnailReady(const QString &handle)
{
    const QList<int> rows = m_rowsByHandle.values(handle);
    for (const int row : rows) {
        const QModelIndex changed = index(row, 0);
        emit dataChanged(changed, changed, {ThumbnailRole});
    }
}

void BrowseListModel::setEntries(const QList<BrowseEntry> &entries)
{
    if (m_entries == entries) {
        return;
    }

    QStringList before;
    before.reserve(m_entries.size());
    for (const BrowseEntry &entry : std::as_const(m_entries)) {
        before.append(entry.handle);
    }

    QStringList after;
    after.reserve(entries.size());
    for (const BrowseEntry &entry : entries) {
        after.append(entry.handle);
    }

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    if (!ListPatch::worthPatching(steps, int(m_entries.size()), int(entries.size()))) {
        beginResetModel();
        m_entries = entries;
        reindex();
        endResetModel();
        emit countChanged();
        return;
    }

    const qsizetype had = m_entries.size();
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
            if (m_entries.at(step.to) != entries.at(step.to)) {
                m_entries[step.to] = entries.at(step.to);
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
            m_entries.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            flushChanged();
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_entries.insert(step.to + i, entries.at(step.to + i));
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            flushChanged();
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(), step.to);
            m_entries.move(step.from, step.to);
            endMoveRows();
            if (m_entries.at(step.to) != entries.at(step.to)) {
                m_entries[step.to] = entries.at(step.to);
                const QModelIndex moved = index(step.to, 0);
                emit dataChanged(moved, moved);
            }
            break;
        }
    }

    flushChanged();
    reindex();

    if (m_entries.size() != had) {
        emit countChanged();
    }
}

void BrowseListModel::reindex()
{
    m_rowsByHandle.clear();
    m_rowsByHandle.reserve(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        m_rowsByHandle.insert(m_entries.at(row).handle, row);
    }
}
