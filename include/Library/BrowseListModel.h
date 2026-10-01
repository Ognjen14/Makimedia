#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QMultiHash>
#include <QString>
#include <QStringList>

class ThumbnailService;

struct BrowseEntry
{
    QString handle;
    QString displayName;
    bool isFolder = false;
    qint64 sizeBytes = 0;
    int childFileCount = 0;
    qint64 childBytes = 0;
    double durationSeconds = 0.0;
    double progress = 0.0;
    bool watched = false;
    bool indexed = false;
    QString matchedPosterPath;

    bool operator==(const BrowseEntry &other) const
    {
        return handle == other.handle
            && displayName == other.displayName
            && isFolder == other.isFolder
            && sizeBytes == other.sizeBytes
            && childFileCount == other.childFileCount
            && childBytes == other.childBytes
            && durationSeconds == other.durationSeconds
            && progress == other.progress
            && watched == other.watched
            && indexed == other.indexed
            && matchedPosterPath == other.matchedPosterPath;
    }

    bool operator!=(const BrowseEntry &other) const
    {
        return !(*this == other);
    }
};

class BrowseListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        HandleRole = Qt::UserRole + 1,
        DisplayNameRole,
        IsFolderRole,
        SummaryRole,
        DurationTextRole,
        WatchProgressRole,
        WatchedRole,
        IndexedRole,
        ThumbnailRole
    };

    explicit BrowseListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QList<BrowseEntry> &entries);

    void setThumbnailService(ThumbnailService *service);
    void onThumbnailReady(const QString &handle);
    QString handleAt(int row) const;
    bool wantsThumbnailAt(int row) const;

signals:
    void countChanged();

private:
    void reindex();

    QList<BrowseEntry> m_entries;
    QMultiHash<QString, int> m_rowsByHandle;
    ThumbnailService *m_thumbnails = nullptr;
};
