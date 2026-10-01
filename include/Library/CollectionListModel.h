#pragma once

#include "Library/CollectionShelf.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QStringList>

class CollectionListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        CollectionIdRole = Qt::UserRole + 1,
        NameRole,
        PosterRole,
        BackdropRole,
        BehindPostersRole,
        TotalRole,
        OwnedRole,
        MissingRole,
        WatchedCountRole,
        SummaryRole,
        ProgressRole,
        CompletedRole,
        CustomRole,
        ArtworkStampRole
    };

    explicit CollectionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QList<CollectionShelf::Summary> &items);
    void touchArtwork(const QStringList &paths, bool everything);
    QList<CollectionShelf::Summary> items() const;

signals:
    void countChanged();

private:
    int stampFor(const CollectionShelf::Summary &item) const;

    QList<CollectionShelf::Summary> m_items;
    QHash<QString, int> m_artworkStamps;
    int m_artworkEverything = 0;
};
