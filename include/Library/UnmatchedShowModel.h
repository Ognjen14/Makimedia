#pragma once

#include "Metadata/ShowGrouping.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

struct UnmatchedShow
{
    QString title;
    int fileCount = 0;
    QVariantList seasons;
    QStringList fileHandles;
    QStringList folderHandles;
    QStringList fileNames;
    QString folderPath;

    bool titleFromFolder = false;
    QString titleFolder;
    bool episodeMarkers = false;
    bool numberedEpisodes = false;
    bool seasonFolders = false;

    bool operator==(const UnmatchedShow &other) const
    {
        return title == other.title
            && fileCount == other.fileCount
            && seasons == other.seasons
            && fileHandles == other.fileHandles
            && folderHandles == other.folderHandles
            && fileNames == other.fileNames
            && folderPath == other.folderPath
            && titleFromFolder == other.titleFromFolder
            && titleFolder == other.titleFolder
            && episodeMarkers == other.episodeMarkers
            && numberedEpisodes == other.numberedEpisodes
            && seasonFolders == other.seasonFolders;
    }

    bool operator!=(const UnmatchedShow &other) const
    {
        return !(*this == other);
    }
};

class UnmatchedShowModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        FileCountRole,
        SeasonsRole,
        FileHandlesRole,
        FolderHandlesRole,
        FileNamesRole,
        FolderPathRole,
        TitleFromFolderRole,
        TitleFolderRole,
        EpisodeMarkersRole,
        NumberedEpisodesRole,
        SeasonFoldersRole
    };

    explicit UnmatchedShowModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setShows(const QList<ShowGrouping::Show> &shows,
                  const QHash<QString, QString> &folderPaths = {});

    void removeFiles(const QStringList &fileHandles);
    bool containsFile(const QString &fileHandle) const { return m_titleByHandle.contains(fileHandle); }

signals:
    void countChanged();

private:
    void reindex();
    void reindexRows();

    QList<UnmatchedShow> m_shows;
    QHash<QString, int> m_rowsByTitle;
    QHash<QString, QString> m_titleByHandle;
};
