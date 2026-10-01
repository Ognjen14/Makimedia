#pragma once

#include "Library/GenreShelf.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class MediaListModel;

class GenreRowModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        GenreRole = Qt::UserRole + 1,
        TitleCountRole,
        TitlesRole
    };

    explicit GenreRowModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(const QList<GenreShelf::Row> &rows);
    void touchArtwork(const QStringList &paths, bool everything);

signals:
    void countChanged();

private:
    struct Entry
    {
        QString genre;
        MediaListModel *titles = nullptr;
    };

    qint64 keyOf(const QString &genre);

    QList<Entry> m_rows;
    QHash<QString, qint64> m_keys;
    qint64 m_nextKey = 1;
};
