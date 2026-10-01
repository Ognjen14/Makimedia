#include "Streaming/RemoteMediaSource.h"

#include "MmLog.h"
#include "Streaming/StreamProtocol.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUrl>

#include <utility>

namespace {

constexpr int kFetchTimeoutMs = 4000;

}

RemoteMediaSource::RemoteMediaSource(const QString &host, quint16 port, const QString &token)
    : m_host(host)
    , m_port(port)
    , m_token(token)
{
}

void RemoteMediaSource::setIdResolver(IdResolver resolver)
{
    m_resolver = std::move(resolver);
}

QString RemoteMediaSource::baseUrl() const
{
    const QHostAddress address(m_host);
    const bool bareIpv6 = address.protocol() == QAbstractSocket::IPv6Protocol;
    const QString host = bareIpv6 ? QLatin1Char('[') + m_host + QLatin1Char(']') : m_host;
    return QStringLiteral("http://%1:%2").arg(host).arg(m_port);
}

QString RemoteMediaSource::sessionUrl() const
{
    return m_token.isEmpty() ? baseUrl() : baseUrl() + StreamProtocol::tokenPrefix(m_token);
}

QString RemoteMediaSource::urlForFileId(qint64 fileId) const
{
    return sessionUrl() + QStringLiteral("/v1/file/") + QString::number(fileId);
}

qint64 RemoteMediaSource::idFor(const QString &handle) const
{
    if (m_resolver) {
        return m_resolver(handle);
    }
    bool ok = false;
    const qint64 id = handle.toLongLong(&ok);
    return ok ? id : 0;
}

QString RemoteMediaSource::mpvUrl(const QString &handle)
{
    const qint64 id = idFor(handle);
    if (id <= 0) {
        MM_LOG_W() << "remote: no file id for" << handle;
        return QString();
    }
    return urlForFileId(id);
}

bool RemoteMediaSource::exists(const QString &handle) const
{
    Q_UNUSED(handle)
    return true;
}

MediaFileInfo RemoteMediaSource::info(const QString &handle) const
{
    MediaFileInfo info;
    info.handle = handle;
    return info;
}

QString RemoteMediaSource::displayPath(const QString &handle) const
{
    if (handle.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)) {
        return QUrl(handle).fileName();
    }
    return handle;
}

QByteArray RemoteMediaSource::fetch(const QString &path, int *status) const
{
    *status = 0;
    QTcpSocket socket;
    socket.connectToHost(m_host, m_port);
    if (!socket.waitForConnected(kFetchTimeoutMs)) {
        MM_LOG_W() << "remote: could not reach" << m_host << "for" << path;
        return QByteArray();
    }

    const QString asked = m_token.isEmpty() ? path : StreamProtocol::tokenPrefix(m_token) + path;
    socket.write("GET " + asked.toUtf8() + " HTTP/1.1\r\nHost: " + m_host.toUtf8()
                 + "\r\nConnection: close\r\n\r\n");

    QElapsedTimer timer;
    timer.start();
    QByteArray response;
    while (timer.elapsed() < kFetchTimeoutMs) {
        const int left = int(kFetchTimeoutMs - timer.elapsed());
        if (!socket.waitForReadyRead(qMax(1, left))) {
            break;
        }
        response += socket.readAll();
    }
    response += socket.readAll();

    const int headEnd = int(response.indexOf("\r\n\r\n"));
    if (headEnd < 0) {
        MM_LOG_W() << "remote: no answer from" << m_host << "for" << path;
        return QByteArray();
    }
    const QList<QByteArray> statusLine = response.left(response.indexOf("\r\n")).split(' ');
    *status = statusLine.size() >= 2 ? statusLine.at(1).toInt() : 0;
    return response.mid(headEnd + 4);
}

QStringList RemoteMediaSource::siblingSubtitles(const QString &handle) const
{
    const qint64 id = idFor(handle);
    if (id <= 0) {
        return QStringList();
    }

    int status = 0;
    const QByteArray body =
        fetch(QStringLiteral("/v1/file/%1/subtitles").arg(id), &status);
    if (status != 200) {
        MM_LOG_W() << "remote: the PC did not list the subtitles of file" << id
                   << "- status" << status;
        return QStringList();
    }

    QStringList urls;
    const QJsonArray entries =
        QJsonDocument::fromJson(body).object().value(QStringLiteral("subtitles")).toArray();
    for (const QJsonValue &entry : entries) {
        const QString path = entry.toObject().value(QStringLiteral("url")).toString();
        if (path.startsWith(QLatin1Char('/'))) {
            urls.append(sessionUrl() + path);
        }
    }
    MM_LOG_I() << "remote: the PC lists" << urls.size() << "subtitles for file" << id;
    return urls;
}

QList<MediaFileInfo> RemoteMediaSource::listChildren(const QString &handle) const
{
    Q_UNUSED(handle)
    return QList<MediaFileInfo>();
}

QString RemoteMediaSource::parentOf(const QString &handle) const
{
    Q_UNUSED(handle)
    return QString();
}

bool RemoteMediaSource::isRootAvailable(const QString &rootHandle) const
{
    Q_UNUSED(rootHandle)
    return true;
}
