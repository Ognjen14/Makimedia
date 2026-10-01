#pragma once

#include "Data/MediaRepository.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QStringList>

class MediaListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        MediaIdRole = Qt::UserRole + 1,
        TmdbIdRole,
        KindRole,
        TitleRole,
        YearRole,
        PosterRole,
        OverviewRole,
        RatingRole,
        GenresRole,
        RuntimeRole,
        FileCountRole,
        SeasonCountRole,
        HandleRole,
        SummaryRole,
        WatchedRole,
        ProgressRole,
        ArtworkStampRole,
        NewCountRole
    };

    explicit MediaListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QList<MediaRecord> &items);
    void placeItem(const MediaRecord &item, bool keep);
    void touchArtwork(const QStringList &paths, bool everything);
    MediaRecord at(int row) const;

signals:
    void countChanged();

private:
    void reindex(int first, int last);

    QList<MediaRecord> m_items;
    QHash<qint64, int> m_rowsById;
    QHash<QString, int> m_artworkStamps;
    int m_artworkEverything = 0;
};
