#pragma once

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>

class IMediaSource;

class ThumbnailService : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailService(QObject *parent = nullptr);
    ~ThumbnailService() override;

    void setMediaSource(IMediaSource *mediaSource);

    void setEnabled(bool enabled);

    QString cachedUrl(const QString &handle) const;
    void setWanted(const QStringList &handles);
    void want(const QStringList &handles);
    void unwant(const QStringList &handles);

    bool discard(const QString &handle);

    void setDeferred(bool deferred);

    Q_INVOKABLE void clearCache();

signals:
    void thumbnailReady(const QString &handle, const QString &url);

private:
    void request(const QString &handle, const QString &path);
    void requestWanted();
    void compactWanted();
    void startNext();
    void startWorker(const QString &handle, const QString &source,
                     const QString &output);
    void notifyGenerated(const QString &handle, bool ok);
    void onGenerated(const QString &handle, bool ok);

    QString cacheFilePath(const QString &handle) const;

    struct PendingItem {
        QString handle;
        QString path;
    };

    IMediaSource *m_mediaSource = nullptr;
    bool m_enabled = true;
    QString m_cacheDir;
    QQueue<PendingItem> m_queue;
    QQueue<PendingItem> m_held;
    QStringList m_wanted;
    QSet<QString> m_wantedSet;
    bool m_wantedDirty = false;
    bool m_deferred = false;
    QHash<QString, bool> m_seen;
    QSet<QString> m_discarded;
    mutable QHash<QString, QString> m_urls;
    QSet<QString> m_unresolved;
    bool m_running = false;
};
