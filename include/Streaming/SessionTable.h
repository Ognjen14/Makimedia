#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct StreamDevice
{
    QString deviceId;
    QString name;
    QString form;
    QString peer;
    QString title;
    qint64 fileId = -1;
    QString posterPath;
    QString backdropPath;
    double position = 0.0;
    double duration = 0.0;
    bool paused = false;

    bool watching() const { return !title.isEmpty(); }

    bool operator==(const StreamDevice &other) const
    {
        return deviceId == other.deviceId && name == other.name && form == other.form
               && peer == other.peer && title == other.title && fileId == other.fileId
               && posterPath == other.posterPath && backdropPath == other.backdropPath
               && qFuzzyCompare(position + 1.0, other.position + 1.0)
               && qFuzzyCompare(duration + 1.0, other.duration + 1.0)
               && paused == other.paused;
    }
};

struct Playing
{
    QString title;
    qint64 fileId = -1;
    double position = 0.0;
    double duration = 0.0;
    bool paused = false;
};

class SessionTable
{
public:
    QString open(const QString &deviceId, const QString &name, const QString &form,
                 const QString &peer, qint64 nowMs, bool *listChanged);
    bool touch(const QString &token, qint64 nowMs, bool *listChanged);
    bool heartbeat(const QString &token, const Playing &playing, qint64 nowMs,
                   bool *listChanged);
    bool close(const QString &token, QString *name);
    QStringList expire(qint64 nowMs, qint64 maxSilenceMs);

    StreamDevice deviceFor(const QString &token) const;
    QList<StreamDevice> active() const;
    int size() const { return int(m_sessions.size()); }

private:
    struct Session
    {
        StreamDevice device;
        qint64 lastSeenMs = 0;
        qint64 openedOrder = 0;
        bool active = true;
    };

    static QString newToken();

    QHash<QString, Session> m_sessions;
    qint64 m_order = 0;
};
