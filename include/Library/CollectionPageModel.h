#pragma once

#include "Library/CollectionPage.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QVariantMap>

class CollectionPageModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(qint64 collectionId READ collectionId WRITE setCollectionId NOTIFY collectionIdChanged FINAL)
    Q_PROPERTY(int order READ order WRITE setOrder NOTIFY orderChanged FINAL)
    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged FINAL)

public:
    enum Roles {
        HeadingRole = Qt::UserRole + 1,
        ShowRole,
        PhaseRole,
        NumberRole,
        TmdbIdRole,
        TitleRole,
        YearRole,
        RuntimeMinutesRole,
        RatingRole,
        PosterPathRole,
        OwnedRole,
        AddedRole,
        HandleRole,
        WatchedRole,
        ProgressRole,
        LeftMinutesRole,
        ArtworkStampRole
    };

    explicit CollectionPageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    qint64 collectionId() const;
    void setCollectionId(qint64 collectionId);
    int order() const;
    void setOrder(int order);
    QVariantMap info() const;

    void setPage(const CollectionPage::Page &page);
    void touchArtwork(const QStringList &paths, bool everything);

signals:
    void countChanged();
    void collectionIdChanged();
    void orderChanged();
    void infoChanged();

private:
    qint64 m_collectionId = 0;
    int m_order = CollectionPage::Release;
    QVariantMap m_info;
    QList<CollectionPage::Row> m_rows;
    QHash<QString, int> m_artworkStamps;
    int m_artworkEverything = 0;
};
