#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

class IMediaSource;

struct ProbeInfo
{
    QString handle;
    double durationSeconds = 0.0;
    QString container;
    QString videoCodec;
    QString audioCodec;
    int width = 0;
    int height = 0;
    bool hdr = false;
    int audioTrackCount = 0;
    int subtitleTrackCount = 0;
    bool valid = false;
};

struct ProbeRequest
{
    QString handle;
    QString path;
};

class ProbeService : public QObject
{
    Q_OBJECT

public:
    explicit ProbeService(QObject *parent = nullptr);
    ~ProbeService() override;

    void setMediaSource(IMediaSource *mediaSource);
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setDeferred(bool deferred);

    void request(const QString &handle, const QString &path);
    void requestAll(const QList<ProbeRequest> &files);

    int pending() const;

signals:
    void probed(const ProbeInfo &info);
    void pendingChanged();

private:
    void startNext();
    void startWorker(const QString &handle, const QString &source);
    void onProbed(const ProbeInfo &info);

    IMediaSource *m_mediaSource = nullptr;
    bool m_enabled = true;
    bool m_deferred = false;
    bool m_running = false;
    QQueue<ProbeRequest> m_queue;
    QQueue<ProbeRequest> m_held;
    QSet<QString> m_seen;
    QSet<QString> m_waiting;
    QElapsedTimer m_passTimer;
    int m_probedOk = 0;
    int m_probedFailed = 0;
};
