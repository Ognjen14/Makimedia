#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QVariantList>
#include <QVariantMap>

class TrackListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged FINAL)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        LangRole,
        CodecRole,
        SelectedRole,
        ExternalRole
    };

    explicit TrackListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int revision() const;

    void setTracks(const QVariantList &tracks);

    Q_INVOKABLE QVariantMap byId(qint64 id) const;
    Q_INVOKABLE QVariantMap at(int row) const;
    Q_INVOKABLE int embeddedCount() const;

signals:
    void countChanged();
    void revisionChanged();

private:
    QList<QVariantMap> m_tracks;
    int m_revision = 0;
};
