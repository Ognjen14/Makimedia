#pragma once

#include "Data/MediaRepository.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ShowEpisodeModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(qint64 mediaId READ mediaId WRITE setMediaId NOTIFY mediaIdChanged FINAL)
    Q_PROPERTY(int season READ season WRITE setSeason NOTIFY seasonChanged FINAL)
    Q_PROPERTY(QVariantList seasons READ seasons NOTIFY seasonsChanged FINAL)
    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged FINAL)
    Q_PROPERTY(QString seasonPosterPath READ seasonPosterPath NOTIFY seasonArtworkChanged FINAL)
    Q_PROPERTY(QString seasonBackdropPath READ seasonBackdropPath NOTIFY seasonArtworkChanged FINAL)
    Q_PROPERTY(bool hasNextUp READ hasNextUp NOTIFY nextUpChanged FINAL)
    Q_PROPERTY(QVariantMap nextUp READ nextUp NOTIFY nextUpChanged FINAL)
    Q_PROPERTY(bool seasonWatched READ seasonWatched NOTIFY seasonStateChanged FINAL)
    Q_PROPERTY(bool seasonHasFiles READ seasonHasFiles NOTIFY seasonStateChanged FINAL)

public:
    enum Roles {
        SeasonRole = Qt::UserRole + 1,
        EpisodeRole,
        TitleRole,
        OverviewRole,
        AirDateRole,
        StillPathRole,
        RuntimeMinutesRole,
        HandleRole,
        FileNameRole,
        HasFileRole,
        MissingRole,
        WatchedRole,
        PositionSecondsRole,
        DurationSecondsRole,
        ProgressRole,
        ResumeSecondsRole,
        ArtworkStampRole
    };

    explicit ShowEpisodeModel(const MediaRepository &media, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    qint64 mediaId() const;
    void setMediaId(qint64 mediaId);
    int season() const;
    void setSeason(int season);
    QVariantList seasons() const;
    QVariantMap info() const;
    QString seasonPosterPath() const;
    QString seasonBackdropPath() const;
    bool hasNextUp() const;
    QVariantMap nextUp() const;
    bool seasonWatched() const;
    bool seasonHasFiles() const;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QStringList fileHandles() const;
    Q_INVOKABLE QStringList fileNames() const;
    Q_INVOKABLE QStringList seasonFileHandles() const;
    Q_INVOKABLE int episodeCountFor(int season) const;

    static QVariantMap toMap(const EpisodeRecord &episode);

    void reload();
    bool concerns(const QList<qint64> &mediaIds, const QList<qint64> &fileIds) const;
    void touchArtwork(const QStringList &paths, bool everything);

signals:
    void countChanged();
    void mediaIdChanged();
    void seasonChanged();
    void seasonsChanged();
    void infoChanged();
    void seasonArtworkChanged();
    void nextUpChanged();
    void seasonStateChanged();

private:
    void applySeason();
    void updateSeasonState();
    void updateSeasonArtwork();

    const MediaRepository &m_media;
    qint64 m_mediaId = 0;
    int m_season = 0;

    QList<EpisodeRecord> m_all;
    QList<EpisodeRecord> m_rows;
    qint64 m_rowsMediaId = 0;
    int m_rowsSeason = -1;
    QSet<qint64> m_fileIds;

    QVariantList m_seasons;
    QHash<int, QString> m_seasonPosters;
    QString m_seasonPosterPath;
    QString m_seasonBackdropPath;
    QVariantMap m_info;
    QVariantMap m_nextUp;
    bool m_seasonWatched = false;
    bool m_seasonHasFiles = false;
    QHash<QString, int> m_artworkStamps;
    int m_artworkEverything = 0;
};
