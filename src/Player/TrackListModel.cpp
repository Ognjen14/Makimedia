#include "Player/TrackListModel.h"

#include "Library/ListPatch.h"

#include <utility>

namespace {

qint64 idOf(const QVariantMap &track)
{
    return track.value(QStringLiteral("id")).toLongLong();
}

}

TrackListModel::TrackListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TrackListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_tracks.size());
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size()) {
        return QVariant();
    }

    const QVariantMap &track = m_tracks.at(index.row());

    switch (role) {
    case IdRole:
        return track.value(QStringLiteral("id"));
    case TitleRole:
        return track.value(QStringLiteral("title"));
    case LangRole:
        return track.value(QStringLiteral("lang"));
    case CodecRole:
        return track.value(QStringLiteral("codec"));
    case SelectedRole:
        return track.value(QStringLiteral("selected"));
    case ExternalRole:
        return track.value(QStringLiteral("external"));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {LangRole, "lang"},
        {CodecRole, "codec"},
        {SelectedRole, "selected"},
        {ExternalRole, "external"}
    };
}

int TrackListModel::revision() const
{
    return m_revision;
}

void TrackListModel::setTracks(const QVariantList &tracks)
{
    QList<QVariantMap> next;
    next.reserve(tracks.size());
    for (const QVariant &track : tracks) {
        next.append(track.toMap());
    }

    if (m_tracks == next) {
        return;
    }

    QList<qint64> before;
    before.reserve(m_tracks.size());
    for (const QVariantMap &track : std::as_const(m_tracks)) {
        before.append(idOf(track));
    }

    QList<qint64> after;
    after.reserve(next.size());
    for (const QVariantMap &track : std::as_const(next)) {
        after.append(idOf(track));
    }

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    const qsizetype had = m_tracks.size();

    if (!ListPatch::worthPatching(steps, int(m_tracks.size()), int(next.size()))) {
        beginResetModel();
        m_tracks = next;
        endResetModel();
    } else {
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
                if (m_tracks.at(step.to) != next.at(step.to)) {
                    m_tracks[step.to] = next.at(step.to);
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
                m_tracks.remove(step.from, step.count);
                endRemoveRows();
                break;

            case ListPatch::Kind::Insert:
                flushChanged();
                beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
                for (int i = 0; i < step.count; ++i) {
                    m_tracks.insert(step.to + i, next.at(step.to + i));
                }
                endInsertRows();
                break;

            case ListPatch::Kind::Move:
                flushChanged();
                beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(), step.to);
                m_tracks.move(step.from, step.to);
                endMoveRows();
                if (m_tracks.at(step.to) != next.at(step.to)) {
                    m_tracks[step.to] = next.at(step.to);
                    const QModelIndex moved = index(step.to, 0);
                    emit dataChanged(moved, moved);
                }
                break;
            }
        }

        flushChanged();
    }

    if (m_tracks.size() != had) {
        emit countChanged();
    }

    ++m_revision;
    emit revisionChanged();
}

QVariantMap TrackListModel::byId(qint64 id) const
{
    for (const QVariantMap &track : m_tracks) {
        if (idOf(track) == id) {
            return track;
        }
    }
    return QVariantMap();
}

QVariantMap TrackListModel::at(int row) const
{
    if (row < 0 || row >= m_tracks.size()) {
        return QVariantMap();
    }
    return m_tracks.at(row);
}

int TrackListModel::embeddedCount() const
{
    int count = 0;
    for (const QVariantMap &track : m_tracks) {
        if (!track.value(QStringLiteral("external")).toBool()) {
            ++count;
        }
    }
    return count;
}
