#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

struct FoundServer
{
    QString serverId;
    QString name;
    QString host;
    quint16 port = 0;
    qint64 lastSeenMs = 0;
};

class FoundServerModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        ServerIdRole = Qt::UserRole + 1,
        NameRole,
        HostRole,
        PortRole,
        SelectedRole
    };

    explicit FoundServerModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool upsert(const FoundServer &server);
    const FoundServer *find(const QString &serverId) const;

    QString selectedId() const { return m_selectedId; }
    void setSelected(const QString &serverId);

    QStringList expire(qint64 nowMs, qint64 maxAgeMs, const QString &keepId);
    QString firstId() const;

signals:
    void countChanged();

private:
    int rowOf(const QString &serverId) const;

    QList<FoundServer> m_servers;
    QString m_selectedId;
};
