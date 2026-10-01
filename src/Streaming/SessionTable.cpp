#include "Streaming/SessionTable.h"

#include <QRandomGenerator>

#include <algorithm>

QString SessionTable::newToken()
{
    quint32 words[4];
    QRandomGenerator::system()->fillRange(words);
    QString token;
    for (const quint32 word : words) {
        token += QStringLiteral("%1").arg(word, 8, 16, QLatin1Char('0'));
    }
    return token;
}

QString SessionTable::open(const QString &deviceId, const QString &name, const QString &form,
                           const QString &peer, qint64 nowMs, bool *listChanged)
{
    *listChanged = false;

    if (!deviceId.isEmpty()) {
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            Session &session = it.value();
            if (session.device.deviceId != deviceId) {
                continue;
            }
            const StreamDevice before = session.device;
            const bool wasActive = session.active;
            session.device.name = name;
            session.device.form = form;
            session.device.peer = peer;
            session.lastSeenMs = nowMs;
            session.active = true;
            *listChanged = !wasActive || !(before == session.device);
            return it.key();
        }
    }

    Session session;
    session.device.deviceId = deviceId;
    session.device.name = name;
    session.device.form = form;
    session.device.peer = peer;
    session.lastSeenMs = nowMs;
    session.openedOrder = ++m_order;

    QString token = newToken();
    while (m_sessions.contains(token)) {
        token = newToken();
    }
    m_sessions.insert(token, session);
    *listChanged = true;
    return token;
}

bool SessionTable::touch(const QString &token, qint64 nowMs, bool *listChanged)
{
    *listChanged = false;
    const auto it = m_sessions.find(token);
    if (it == m_sessions.end()) {
        return false;
    }
    it->lastSeenMs = nowMs;
    if (!it->active) {
        it->active = true;
        *listChanged = true;
    }
    return true;
}

bool SessionTable::heartbeat(const QString &token, const Playing &playing, qint64 nowMs,
                             bool *listChanged)
{
    if (!touch(token, nowMs, listChanged)) {
        return false;
    }
    StreamDevice &device = m_sessions[token].device;
    const StreamDevice before = device;
    device.title = playing.title;
    device.fileId = playing.title.isEmpty() ? -1 : playing.fileId;
    device.position = playing.title.isEmpty() ? 0.0 : playing.position;
    device.duration = playing.title.isEmpty() ? 0.0 : playing.duration;
    device.paused = !playing.title.isEmpty() && playing.paused;
    if (!(before == device)) {
        *listChanged = true;
    }
    return true;
}

bool SessionTable::close(const QString &token, QString *name)
{
    const auto it = m_sessions.find(token);
    if (it == m_sessions.end()) {
        return false;
    }
    *name = it->device.name;
    m_sessions.erase(it);
    return true;
}

QStringList SessionTable::expire(qint64 nowMs, qint64 maxSilenceMs)
{
    QStringList dropped;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        Session &session = it.value();
        if (!session.active || nowMs - session.lastSeenMs <= maxSilenceMs) {
            continue;
        }
        session.active = false;
        session.device.title.clear();
        session.device.fileId = -1;
        session.device.posterPath.clear();
        session.device.backdropPath.clear();
        session.device.position = 0.0;
        session.device.duration = 0.0;
        session.device.paused = false;
        dropped.append(session.device.name);
    }
    return dropped;
}

StreamDevice SessionTable::deviceFor(const QString &token) const
{
    const auto it = m_sessions.constFind(token);
    return it == m_sessions.constEnd() ? StreamDevice() : it->device;
}

QList<StreamDevice> SessionTable::active() const
{
    QList<const Session *> ordered;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        if (it->active) {
            ordered.append(&it.value());
        }
    }
    std::sort(ordered.begin(), ordered.end(), [](const Session *a, const Session *b) {
        return a->openedOrder < b->openedOrder;
    });

    QList<StreamDevice> devices;
    devices.reserve(ordered.size());
    for (const Session *session : std::as_const(ordered)) {
        devices.append(session->device);
    }
    return devices;
}
