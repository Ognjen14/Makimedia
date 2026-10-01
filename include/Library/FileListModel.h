#pragma once

#include "Data/FileRepository.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QStringList>

#include <functional>

class ThumbnailService;

class FileListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        HandleRole,
        ParentHandleRole,
        DisplayNameRole,
        SizeTextRole,
        DurationTextRole,
        ProgressRole,
        WatchedRole,
        MissingRole,
        ResolutionRole,
        RemainingTextRole,
        UnmatchedRole,
        LocationTextRole,
        ThumbnailRole,
        PosterRole,
        BackdropRole,
        StillRole,
        MatchedYearRole,
        SuggestedRole,
        SeasonRole,
        EpisodeRole,
        EpisodeTitleRole,
        EpisodeLabelRole,
        TitleTextRole,
        ArtworkStampRole
    };

    explicit FileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    using Order = std::function<bool(const LibraryFile &, const LibraryFile &)>;

    void setFiles(const QList<LibraryFile> &files);
    bool updateFile(const LibraryFile &file);
    bool removeFile(const QString &handle);
    void placeFile(const LibraryFile &file, bool keep, const Order &isBefore);
    LibraryFile at(int row) const;
    LibraryFile fileWithHandle(const QString &handle) const;

    void setThumbnailService(ThumbnailService *service);
    void onThumbnailReady(const QString &handle);
    void touchArtwork(const QStringList &paths, bool everything);
    QString handleAt(int row) const;
    bool wantsThumbnailAt(int row) const;

signals:
    void countChanged();

private:
    void reindex(int first, int last);

    QString titleTextFor(const LibraryFile &file) const;

    QList<LibraryFile> m_files;
    QHash<QString, int> m_rowsByHandle;
    mutable QHash<QString, QString> m_prettyTitles;
    QHash<QString, int> m_artworkStamps;
    int m_artworkEverything = 0;
    ThumbnailService *m_thumbnails = nullptr;
};
