#pragma once

#include "Streaming/LibrarySnapshot.h"
#include "Streaming/SessionTable.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QThread;
class StreamServerWorker;

using SubtitleLister = std::function<QStringList(const QString &videoPath)>;

struct SubtitleAsk
{
    bool fetch = false;
    qint64 fileId = 0;
    QString videoPath;
    qint64 tmdbId = 0;
    int season = 0;
    int episode = 0;
    qint64 pick = 0;
};

struct SubtitleAnswer
{
    QJsonObject body;
    QString savedPath;
};

using SubtitleDone = std::function<void(const SubtitleAnswer &answer)>;
using SubtitleHelper = std::function<void(const SubtitleAsk &ask, const SubtitleDone &done)>;

struct StreamServerInfo
{
    QString serverId;
    QString name;
    int schemaVersion = 0;
    QString snapshotPath;
    QString revision;
    qint64 snapshotBytes = 0;
};

class StreamServer : public QObject
{
    Q_OBJECT

public:
    explicit StreamServer(QObject *parent = nullptr);
    ~StreamServer() override;

    void setInfo(const StreamServerInfo &info);
    void setFiles(const QHash<qint64, QString> &paths);
    void setAttachedSubtitles(const QHash<qint64, QList<AttachedSubtitle>> &attached);
    void setSubtitleLister(SubtitleLister lister);
    void setSubtitleHelper(SubtitleHelper helper);
    void setArtworkDirectory(const QString &directory);

    bool start(quint16 port);
    void stop();

    bool isRunning() const;
    quint16 port() const;
    QString errorString() const;

    QList<StreamDevice> devices() const;

    void setDeveloperSilence(int milliseconds);
    void setDeveloperThrottle(qint64 bytesPerSecond);

signals:
    void devicesChanged();

    void artworkMissed(const QString &key);

private:
    void takeDevices(const QList<StreamDevice> &devices);

    StreamServerInfo m_info;
    QList<StreamDevice> m_devices;
    QHash<qint64, QString> m_files;
    QHash<qint64, QList<AttachedSubtitle>> m_attached;
    SubtitleLister m_lister;
    SubtitleHelper m_subtitleHelper;
    QString m_artworkDirectory;
    QThread *m_thread = nullptr;
    StreamServerWorker *m_worker = nullptr;
    quint16 m_port = 0;
    int m_generation = 0;
    QString m_error;
};
