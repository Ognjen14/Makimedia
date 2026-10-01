#include "Streaming/StreamServer.h"

#include "Library/SubtitleEncoding.h"
#include "MmLog.h"
#include "Streaming/AddressFilter.h"
#include "Streaming/ArtworkLookup.h"
#include "Streaming/HttpMessage.h"
#include "Streaming/StreamProtocol.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>
#include <QMetaObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <utility>

namespace {

constexpr qint64 kChunkBytes = 256 * 1024;
constexpr qint64 kHighWaterBytes = 1024 * 1024;
constexpr qint64 kMaxTextSubtitleBytes = 16 * 1024 * 1024;
constexpr int kExpiryCheckMs = 1000;
constexpr int kRetryPumpMs = 50;
constexpr int kGoodbyeWaitMs = 300;

QByteArray mimeFor(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("mkv")) {
        return "video/x-matroska";
    }
    if (suffix == QLatin1String("mp4") || suffix == QLatin1String("m4v")) {
        return "video/mp4";
    }
    if (suffix == QLatin1String("webm")) {
        return "video/webm";
    }
    if (suffix == QLatin1String("avi")) {
        return "video/x-msvideo";
    }
    if (suffix == QLatin1String("ts") || suffix == QLatin1String("m2ts")) {
        return "video/mp2t";
    }
    return "application/octet-stream";
}

}

struct ListedSubtitle
{
    QString path;
    QString name;
    QString title;
    bool attached = false;
};

using DevicesSink = std::function<void(const QList<StreamDevice> &devices)>;
using MissSink = std::function<void(const QString &key)>;

class StreamServerWorker : public QObject
{
public:
    StreamServerWorker(const StreamServerInfo &info, const QHash<qint64, QString> &files,
                       const QHash<qint64, QList<AttachedSubtitle>> &attached,
                       const SubtitleLister &lister, const QString &artworkDirectory,
                       DevicesSink sink, MissSink misses)
        : m_info(info)
        , m_files(files)
        , m_attached(attached)
        , m_lister(lister)
        , m_artworkDirectory(artworkDirectory)
        , m_sink(std::move(sink))
        , m_misses(std::move(misses))
    {
        m_clock.start();
    }

    QString artworkDirectory() const { return m_artworkDirectory; }

    void noteArtworkMissed(const QString &key)
    {
        if (m_misses) {
            m_misses(key);
        }
    }

    quint16 listen(quint16 port, QString *error);
    void shutDown();

    const StreamServerInfo &info() const { return m_info; }
    QString pathFor(qint64 id) const { return m_files.value(id); }
    QList<ListedSubtitle> subtitlesFor(qint64 id) const;

    QString openSession(const QString &deviceId, const QString &name, const QString &form,
                        const QString &peer);
    bool authorize(const QString &token);
    bool heartbeat(const QString &token, const Playing &playing);
    bool closeSession(const QString &token);

    void setSilence(int milliseconds);
    bool silent() const { return m_silentUntilMs > m_clock.elapsed(); }
    void setThrottle(qint64 bytesPerSecond) { m_throttle = bytesPerSecond; }
    qint64 throttle() const { return m_throttle; }
    qint64 nowMs() const { return m_clock.elapsed(); }

private:
    void acceptPending();
    void expireSessions();
    void publish();

    StreamServerInfo m_info;
    QHash<qint64, QString> m_files;
    QHash<qint64, QList<AttachedSubtitle>> m_attached;
    SubtitleLister m_lister;
    QString m_artworkDirectory;
    DevicesSink m_sink;
    MissSink m_misses;
    QTcpServer *m_server = nullptr;
    QTimer *m_expiry = nullptr;
    SessionTable m_sessions;
    QElapsedTimer m_clock;
    qint64 m_silentUntilMs = 0;
    qint64 m_throttle = 0;
};

QString StreamServerWorker::openSession(const QString &deviceId, const QString &name,
                                        const QString &form, const QString &peer)
{
    bool changed = false;
    const int before = m_sessions.size();
    const QString token = m_sessions.open(deviceId, name, form, peer, m_clock.elapsed(), &changed);
    MM_LOG_I() << "stream:" << name << "(" << form << ")"
               << (m_sessions.size() > before ? "connected from" : "connected again from")
               << peer;
    if (changed) {
        publish();
    }
    return token;
}

bool StreamServerWorker::authorize(const QString &token)
{
    bool changed = false;
    if (token.isEmpty() || !m_sessions.touch(token, m_clock.elapsed(), &changed)) {
        return false;
    }
    if (changed) {
        MM_LOG_I() << "stream:" << m_sessions.deviceFor(token).name << "is back";
        publish();
    }
    return true;
}

bool StreamServerWorker::heartbeat(const QString &token, const Playing &playing)
{
    const QString previousTitle = m_sessions.deviceFor(token).title;
    bool changed = false;
    if (!m_sessions.heartbeat(token, playing, m_clock.elapsed(), &changed)) {
        return false;
    }
    if (!changed) {
        return true;
    }

    const QString name = m_sessions.deviceFor(token).name;
    if (previousTitle != playing.title) {
        if (playing.title.isEmpty()) {
            MM_LOG_I() << "stream:" << name << "stopped watching" << previousTitle;
        } else {
            MM_LOG_I() << "stream:" << name << "is watching" << playing.title;
        }
    }
    publish();
    return true;
}

bool StreamServerWorker::closeSession(const QString &token)
{
    QString name;
    if (!m_sessions.close(token, &name)) {
        return false;
    }
    MM_LOG_I() << "stream:" << name << "disconnected";
    publish();
    return true;
}

void StreamServerWorker::expireSessions()
{
    const QStringList dropped =
        m_sessions.expire(m_clock.elapsed(), StreamProtocol::DeviceSilenceMs);
    if (dropped.isEmpty()) {
        return;
    }
    MM_LOG_I() << "stream: nothing heard from" << dropped << "for"
               << StreamProtocol::DeviceSilenceMs / 1000 << "s, off the list";
    publish();
}

void StreamServerWorker::publish()
{
    if (m_sink) {
        m_sink(m_sessions.active());
    }
}

void StreamServerWorker::setSilence(int milliseconds)
{
    m_silentUntilMs = milliseconds > 0 ? m_clock.elapsed() + milliseconds : 0;
    MM_LOG_I() << "stream: developer -"
               << (milliseconds > 0 ? QStringLiteral("silent for %1 s").arg(milliseconds / 1000)
                                    : QStringLiteral("answering again"));
}

QList<ListedSubtitle> StreamServerWorker::subtitlesFor(qint64 id) const
{
    QList<ListedSubtitle> listed;
    const QString video = pathFor(id);
    if (video.isEmpty()) {
        return listed;
    }

    QSet<QString> seen;
    const auto keyOf = [](const QString &path) {
        return QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower();
    };

    const QList<AttachedSubtitle> attached = m_attached.value(id);
    for (const AttachedSubtitle &subtitle : attached) {
        if (!QFile::exists(subtitle.path) || seen.contains(keyOf(subtitle.path))) {
            continue;
        }
        seen.insert(keyOf(subtitle.path));
        listed.append({ subtitle.path, QFileInfo(subtitle.path).fileName(),
                        subtitle.displayName, true });
    }

    if (m_lister) {
        const QStringList found = m_lister(video);
        for (const QString &path : found) {
            if (seen.contains(keyOf(path))) {
                continue;
            }
            seen.insert(keyOf(path));
            listed.append({ path, QFileInfo(path).fileName(), QString(), false });
        }
    }

    QSet<QString> idxBases;
    for (const ListedSubtitle &subtitle : std::as_const(listed)) {
        const QFileInfo info(subtitle.path);
        if (info.suffix().compare(QLatin1String("idx"), Qt::CaseInsensitive) == 0) {
            idxBases.insert(keyOf(info.absoluteDir().filePath(info.completeBaseName())));
        }
    }
    if (!idxBases.isEmpty()) {
        listed.erase(std::remove_if(listed.begin(), listed.end(),
                                    [&](const ListedSubtitle &subtitle) {
            const QFileInfo info(subtitle.path);
            return info.suffix().compare(QLatin1String("sub"), Qt::CaseInsensitive) == 0
                   && idxBases.contains(
                       keyOf(info.absoluteDir().filePath(info.completeBaseName())));
        }), listed.end());
    }
    return listed;
}

class StreamConnection : public QObject
{
public:
    StreamConnection(QTcpSocket *socket, StreamServerWorker *owner)
        : QObject(owner)
        , m_socket(socket)
        , m_owner(owner)
        , m_peer(AddressFilter::readable(socket->peerAddress()))
    {
        socket->setParent(this);
        QObject::connect(socket, &QTcpSocket::readyRead, this, [this]() { onReadyRead(); });
        QObject::connect(socket, &QTcpSocket::bytesWritten, this, [this](qint64) { pump(); });
        QObject::connect(socket, &QTcpSocket::disconnected, this, [this]() {
            if (m_file.isOpen()) {
                MM_LOG_D() << "stream:" << m_peer << "left with" << m_remaining
                           << "bytes of a file unsent";
                m_file.close();
            }
            deleteLater();
        });
    }

    void abort() { m_socket->abort(); }

    bool watching() const { return m_watching; }

    void sayGoodbye()
    {
        m_socket->write(HttpMessage::head(503, {
            { "Content-Type", "text/plain" },
            { "Content-Length", "0" },
            { "Connection", "close" }
        }));
        m_socket->waitForBytesWritten(kGoodbyeWaitMs);
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(kGoodbyeWaitMs);
        }
    }

private:
    bool sending() const { return m_file.isOpen() || m_watching; }

    void onReadyRead()
    {
        m_buffer += m_socket->readAll();
        if (!sending()) {
            handleBuffered();
        }
    }

    void handleBuffered()
    {
        while (!sending() && m_socket->state() == QAbstractSocket::ConnectedState) {
            HttpRequest request;
            const HttpMessage::Parse result = HttpMessage::takeRequest(m_buffer, request);
            if (result == HttpMessage::Parse::NeedMore) {
                return;
            }
            if (result == HttpMessage::Parse::Bad) {
                MM_LOG_W() << "stream: a request from" << m_peer << "could not be read";
                m_keepAlive = false;
                respond(400, QByteArray(), "text/plain");
                m_buffer.clear();
                return;
            }
            m_keepAlive = request.keepAlive;
            handle(request);
        }
    }

    void handle(const HttpRequest &asked)
    {
        if (m_owner->silent()) {
            MM_LOG_D() << "stream: developer - left a request from" << m_peer << "unanswered";
            return;
        }

        HttpRequest request = asked;
        QString token;
        const QString tokenMarker = QStringLiteral("/t/");
        if (request.path.startsWith(tokenMarker)) {
            const qsizetype end = request.path.indexOf(QLatin1Char('/'), tokenMarker.size());
            token = end < 0 ? request.path.mid(tokenMarker.size())
                            : request.path.mid(tokenMarker.size(), end - tokenMarker.size());
            request.path = end < 0 ? QString() : request.path.mid(end);
        }

        const bool isGet = request.method == "GET";
        const bool isHead = request.method == "HEAD";
        const bool isPost = request.method == "POST";
        const bool isDelete = request.method == "DELETE";
        if (!isGet && !isHead && !isPost && !isDelete) {
            respond(405, QByteArray(), "text/plain");
            return;
        }

        if (request.path == QLatin1String("/v1/hello") && (isGet || isHead)) {
            const QJsonObject hello{
                { QStringLiteral("mm"), QStringLiteral("hello") },
                { QStringLiteral("v"), StreamProtocol::Version },
                { QStringLiteral("id"), m_owner->info().serverId },
                { QStringLiteral("name"), m_owner->info().name },
                { QStringLiteral("schema"), m_owner->info().schemaVersion }
            };
            MM_LOG_D() << "stream: hello asked by" << m_peer;
            respond(200, QJsonDocument(hello).toJson(QJsonDocument::Compact),
                    "application/json", isHead);
            return;
        }

        if (request.path == QLatin1String("/v1/session") && isPost) {
            const QJsonObject asking = QJsonDocument::fromJson(request.body).object();
            const QString name = asking.value(QStringLiteral("name")).toString().trimmed();
            const QString sessionToken = m_owner->openSession(
                asking.value(QStringLiteral("device")).toString(),
                name.isEmpty() ? m_peer : name.left(80),
                asking.value(QStringLiteral("form")).toString().left(20), m_peer);
            const QJsonObject answer{
                { QStringLiteral("token"), sessionToken },
                { QStringLiteral("v"), StreamProtocol::Version }
            };
            respond(200, QJsonDocument(answer).toJson(QJsonDocument::Compact),
                    "application/json");
            return;
        }

        if (!m_owner->authorize(token)) {
            MM_LOG_W() << "stream: refused" << request.method << request.path << "from" << m_peer
                       << (token.isEmpty() ? "- it has no session"
                                           : "- not a session this PC knows");
            respond(401, QByteArray(), "text/plain", isHead);
            return;
        }

        if (request.path == QLatin1String("/v1/session") && isDelete) {
            m_owner->closeSession(token);
            respond(200, QByteArray(), "text/plain");
            return;
        }

        if (request.path == QLatin1String("/v1/heartbeat") && isPost) {
            const QJsonObject beat = QJsonDocument::fromJson(request.body).object();
            Playing playing;
            playing.title = beat.value(QStringLiteral("title")).toString().left(200);
            playing.fileId = qint64(beat.value(QStringLiteral("file")).toDouble(-1));
            playing.position = beat.value(QStringLiteral("position")).toDouble();
            playing.duration = beat.value(QStringLiteral("duration")).toDouble();
            playing.paused = beat.value(QStringLiteral("paused")).toBool();
            m_owner->heartbeat(token, playing);
            respond(200, QByteArray(), "text/plain");
            return;
        }

        if (request.path == QLatin1String("/v1/events") && isGet) {
            MM_LOG_D() << "stream:" << m_peer << "listens for the PC stopping";
            m_watching = true;
            return;
        }

        if (isPost || isDelete) {
            respond(405, QByteArray(), "text/plain");
            return;
        }

        if (request.path == QLatin1String("/v1/library") && (isGet || isHead)) {
            const StreamServerInfo &info = m_owner->info();
            if (info.revision.isEmpty()) {
                respond(503, QByteArray(), "text/plain");
                return;
            }
            const QJsonObject library{
                { QStringLiteral("revision"), info.revision },
                { QStringLiteral("bytes"), double(info.snapshotBytes) },
                { QStringLiteral("schema"), info.schemaVersion }
            };
            MM_LOG_I() << "stream: library revision asked by" << m_peer;
            respond(200, QJsonDocument(library).toJson(QJsonDocument::Compact),
                    "application/json", isHead);
            return;
        }

        if (request.path == QLatin1String("/v1/library/snapshot") && (isGet || isHead)) {
            const QString path = m_owner->info().snapshotPath;
            if (path.isEmpty()) {
                respond(503, QByteArray(), "text/plain");
                return;
            }
            serveFile(request, path, QStringLiteral("the library snapshot"),
                      "application/vnd.sqlite3", isHead);
            return;
        }

        if (request.path == QLatin1String("/v1/art") && (isGet || isHead)) {
            const QString key = QUrlQuery(QString::fromLatin1(request.query))
                                    .queryItemValue(QStringLiteral("k"), QUrl::FullyDecoded);
            const QString path = ArtworkLookup::fileFor(m_owner->artworkDirectory(), key);
            if (path.isEmpty()) {
                MM_LOG_D() << "stream: no artwork for" << key;
                m_owner->noteArtworkMissed(key);
                respond(404, QByteArray(), "text/plain");
                return;
            }
            serveFile(request, path, QStringLiteral("artwork ") + key, "image/jpeg", isHead);
            return;
        }

        const QString filePrefix = QStringLiteral("/v1/file/");
        if (request.path.startsWith(filePrefix) && (isGet || isHead)) {
            const QStringList segments = request.path.mid(filePrefix.size())
                                             .split(QLatin1Char('/'));
            if (segments.size() == 2 && segments.at(1) == QLatin1String("subtitles")) {
                listSubtitles(segments.at(0), isHead);
                return;
            }
            if (segments.size() >= 4 && segments.at(1) == QLatin1String("sub")) {
                serveSubtitle(segments.at(0), segments.at(2),
                              segments.mid(3).join(QLatin1Char('/')), request, isHead);
                return;
            }

            bool ok = false;
            const qint64 id = request.path.mid(filePrefix.size()).toLongLong(&ok);
            const QString path = ok && id > 0 ? m_owner->pathFor(id) : QString();
            if (path.isEmpty()) {
                MM_LOG_W() << "stream: nothing served as" << request.path;
                respond(404, QByteArray(), "text/plain");
                return;
            }
            serveFile(request, path, QStringLiteral("file %1").arg(id), mimeFor(path), isHead);
            return;
        }

        MM_LOG_D() << "stream: nothing at" << request.path << "for" << m_peer;
        respond(404, QByteArray(), "text/plain");
    }

    void listSubtitles(const QString &idText, bool headOnly)
    {
        bool ok = false;
        const qint64 id = idText.toLongLong(&ok);
        if (!ok || m_owner->pathFor(id).isEmpty()) {
            respond(404, QByteArray(), "text/plain");
            return;
        }

        const QList<ListedSubtitle> listed = m_owner->subtitlesFor(id);
        QJsonArray entries;
        for (int n = 0; n < listed.size(); ++n) {
            const ListedSubtitle &subtitle = listed.at(n);
            const QString url = QStringLiteral("/v1/file/%1/sub/%2/").arg(id).arg(n)
                                + QString::fromLatin1(QUrl::toPercentEncoding(subtitle.name));
            entries.append(QJsonObject{
                { QStringLiteral("n"), n },
                { QStringLiteral("name"), subtitle.name },
                { QStringLiteral("title"), subtitle.title },
                { QStringLiteral("attached"), subtitle.attached },
                { QStringLiteral("url"), url }
            });
        }

        MM_LOG_I() << "stream: file" << id << "has" << listed.size() << "subtitles for"
                   << m_peer;
        const QJsonObject body{ { QStringLiteral("subtitles"), entries } };
        respond(200, QJsonDocument(body).toJson(QJsonDocument::Compact),
                "application/json", headOnly);
    }

    void serveSubtitle(const QString &idText, const QString &indexText, const QString &name,
                       const HttpRequest &request, bool headOnly)
    {
        bool idOk = false;
        bool indexOk = false;
        const qint64 id = idText.toLongLong(&idOk);
        const int index = indexText.toInt(&indexOk);
        const QList<ListedSubtitle> listed =
            idOk ? m_owner->subtitlesFor(id) : QList<ListedSubtitle>();
        QString path;
        if (indexOk && index >= 0 && index < listed.size()) {
            const ListedSubtitle &entry = listed.at(index);
            if (entry.name == name) {
                path = entry.path;
            } else {
                const QFileInfo idx(entry.path);
                const QFileInfo asked(name);
                const QString pair = idx.absoluteDir().filePath(name);
                if (idx.suffix().compare(QLatin1String("idx"), Qt::CaseInsensitive) == 0
                    && asked.suffix().compare(QLatin1String("sub"), Qt::CaseInsensitive) == 0
                    && asked.completeBaseName() == idx.completeBaseName()
                    && !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'))
                    && QFile::exists(pair)) {
                    path = pair;
                }
            }
        }
        if (path.isEmpty()) {
            MM_LOG_W() << "stream: refused a subtitle that is not on the list of file"
                       << idText << "-" << name;
            respond(404, QByteArray(), "text/plain");
            return;
        }

        const QString suffix = QFileInfo(path).suffix().toLower();
        static const QStringList textSuffixes = {
            QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"),
            QStringLiteral("vtt"), QStringLiteral("txt"), QStringLiteral("smi"),
            QStringLiteral("sami"), QStringLiteral("idx"), QStringLiteral("sub")
        };

        if (textSuffixes.contains(suffix)) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                respond(404, QByteArray(), "text/plain");
                return;
            }
            const bool vobSub = suffix == QLatin1String("sub")
                                && file.peek(4) == QByteArray("\x00\x00\x01\xba", 4);
            const QByteArray bytes = vobSub ? QByteArray() : file.read(kMaxTextSubtitleBytes);
            file.close();

            if (!vobSub) {
                const QString encoding = SubtitleEncoding::guess(bytes);
                const QByteArray utf8 = SubtitleEncoding::decode(bytes, encoding).toUtf8();
                MM_LOG_I() << "stream: subtitle" << name << "of file" << id << "as UTF-8 from"
                           << encoding << "to" << m_peer;
                respond(200, utf8, "text/plain; charset=utf-8", headOnly);
                return;
            }
        }

        serveFile(request, path, QStringLiteral("subtitle %1").arg(name),
                  "application/octet-stream", headOnly);
    }

    void serveFile(const HttpRequest &request, const QString &path, const QString &label,
                   const QByteArray &contentType, bool headOnly)
    {
        m_file.setFileName(path);
        if (!m_file.open(QIODevice::ReadOnly)) {
            MM_LOG_W() << "stream:" << label << "could not be opened:" << path
                       << m_file.errorString();
            respond(404, QByteArray(), "text/plain");
            return;
        }

        const qint64 size = m_file.size();
        const ByteRange range = HttpMessage::parseRange(request.header("range"), size);

        if (range.requested && !range.satisfiable) {
            m_file.close();
            MM_LOG_I() << "stream:" << label << "range past the end asked by" << m_peer;
            m_socket->write(HttpMessage::head(416, {
                { "Content-Range", "bytes */" + QByteArray::number(size) },
                { "Content-Length", "0" },
                { "Connection", m_keepAlive ? "keep-alive" : "close" }
            }));
            finishResponse();
            return;
        }

        const qint64 start = range.requested ? range.start : 0;
        const qint64 length = range.requested ? range.length() : size;

        QList<QPair<QByteArray, QByteArray>> headers = {
            { "Content-Type", contentType },
            { "Content-Length", QByteArray::number(length) },
            { "Accept-Ranges", "bytes" },
            { "Connection", m_keepAlive ? "keep-alive" : "close" }
        };
        if (range.requested) {
            headers.append({ "Content-Range", "bytes " + QByteArray::number(range.start)
                                              + '-' + QByteArray::number(range.end)
                                              + '/' + QByteArray::number(size) });
        }

        if (contentType.startsWith("image/")) {
            MM_LOG_D() << "stream:" << label << "to" << m_peer << length << "bytes";
        } else {
            MM_LOG_I() << "stream:" << label << "to" << m_peer
                       << (range.requested ? "from byte" : "whole,") << start
                       << "for" << length << "of" << size << "bytes";
        }

        m_socket->write(HttpMessage::head(range.requested ? 206 : 200, headers));

        if (headOnly || length == 0) {
            m_file.close();
            finishResponse();
            return;
        }

        if (!m_file.seek(start)) {
            MM_LOG_W() << "stream: could not seek" << label << "to" << start;
            m_file.close();
            m_socket->abort();
            return;
        }
        m_remaining = length;
        pump();
    }

    void pumpLater()
    {
        if (m_pumpScheduled) {
            return;
        }
        m_pumpScheduled = true;
        QTimer::singleShot(kRetryPumpMs, this, [this]() {
            m_pumpScheduled = false;
            pump();
        });
    }

    void pump()
    {
        if (!m_file.isOpen()) {
            return;
        }
        if (m_owner->silent()) {
            pumpLater();
            return;
        }

        const qint64 throttle = m_owner->throttle();
        qint64 allowance = m_remaining;
        if (throttle > 0) {
            const qint64 now = m_owner->nowMs();
            if (m_budgetAtMs == 0) {
                m_budgetAtMs = now;
            }
            m_budget = qMin(throttle / 2, m_budget + throttle * (now - m_budgetAtMs) / 1000);
            m_budgetAtMs = now;
            allowance = m_budget;
            if (allowance <= 0) {
                pumpLater();
                return;
            }
        } else {
            m_budget = 0;
            m_budgetAtMs = 0;
        }

        while (m_remaining > 0 && allowance > 0 && m_socket->bytesToWrite() < kHighWaterBytes) {
            const QByteArray chunk = m_file.read(qMin(qMin(kChunkBytes, m_remaining), allowance));
            if (chunk.isEmpty()) {
                MM_LOG_W() << "stream: reading a served file stopped early:"
                           << m_file.fileName() << m_file.errorString();
                m_file.close();
                m_socket->abort();
                return;
            }
            m_socket->write(chunk);
            m_remaining -= chunk.size();
            allowance -= chunk.size();
            if (throttle > 0) {
                m_budget -= chunk.size();
            }
        }

        if (m_remaining == 0) {
            m_file.close();
            finishResponse();
        } else if (throttle > 0 && allowance <= 0) {
            pumpLater();
        }
    }

    void respond(int status, const QByteArray &body, const QByteArray &contentType,
                 bool headOnly = false)
    {
        m_socket->write(HttpMessage::head(status, {
            { "Content-Type", contentType },
            { "Content-Length", QByteArray::number(body.size()) },
            { "Connection", m_keepAlive ? "keep-alive" : "close" }
        }));
        if (!headOnly) {
            m_socket->write(body);
        }
        finishResponse();
    }

    void finishResponse()
    {
        if (!m_keepAlive) {
            m_socket->disconnectFromHost();
            return;
        }
        if (!m_buffer.isEmpty()) {
            QMetaObject::invokeMethod(this, [this]() { handleBuffered(); }, Qt::QueuedConnection);
        }
    }

    QTcpSocket *m_socket = nullptr;
    StreamServerWorker *m_owner = nullptr;
    QString m_peer;
    QByteArray m_buffer;
    QFile m_file;
    qint64 m_remaining = 0;
    qint64 m_budget = 0;
    qint64 m_budgetAtMs = 0;
    bool m_keepAlive = true;
    bool m_watching = false;
    bool m_pumpScheduled = false;
};

quint16 StreamServerWorker::listen(quint16 port, QString *error)
{
    m_server = new QTcpServer(this);
    QObject::connect(m_server, &QTcpServer::newConnection, this, [this]() { acceptPending(); });

    if (!m_server->listen(QHostAddress::Any, port)) {
        *error = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return 0;
    }

    m_expiry = new QTimer(this);
    m_expiry->setInterval(kExpiryCheckMs);
    QObject::connect(m_expiry, &QTimer::timeout, this, [this]() { expireSessions(); });
    m_expiry->start();
    return m_server->serverPort();
}

void StreamServerWorker::acceptPending()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        const QHostAddress peer = socket->peerAddress();
        if (!AddressFilter::isPrivatePeer(peer)) {
            MM_LOG_W() << "stream: refused a connection from" << AddressFilter::readable(peer)
                       << "- not a private address";
            socket->abort();
            socket->deleteLater();
            continue;
        }
        MM_LOG_D() << "stream: connection from" << AddressFilter::readable(peer);
        new StreamConnection(socket, this);
    }
}

void StreamServerWorker::shutDown()
{
    delete m_expiry;
    m_expiry = nullptr;
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }

    int told = 0;
    const QObjectList connections = children();
    for (QObject *child : connections) {
        StreamConnection *connection = static_cast<StreamConnection *>(child);
        if (connection->watching()) {
            connection->sayGoodbye();
            ++told;
        }
        connection->abort();
        delete child;
    }
    if (told > 0) {
        MM_LOG_I() << "stream: told" << told << "devices this PC stops streaming";
    }
}

StreamServer::StreamServer(QObject *parent)
    : QObject(parent)
{
}

StreamServer::~StreamServer()
{
    stop();
}

void StreamServer::setInfo(const StreamServerInfo &info)
{
    m_info = info;
}

void StreamServer::setFiles(const QHash<qint64, QString> &paths)
{
    m_files = paths;
}

void StreamServer::setAttachedSubtitles(const QHash<qint64, QList<AttachedSubtitle>> &attached)
{
    m_attached = attached;
}

void StreamServer::setSubtitleLister(SubtitleLister lister)
{
    m_lister = std::move(lister);
}

void StreamServer::setArtworkDirectory(const QString &directory)
{
    m_artworkDirectory = directory;
}

bool StreamServer::start(quint16 port)
{
    if (isRunning()) {
        return true;
    }

    m_error.clear();
    const int generation = ++m_generation;
    DevicesSink sink = [this, generation](const QList<StreamDevice> &devices) {
        QMetaObject::invokeMethod(this, [this, generation, devices]() {
            if (generation == m_generation && m_thread) {
                takeDevices(devices);
            }
        }, Qt::QueuedConnection);
    };

    MissSink misses = [this, generation](const QString &key) {
        QMetaObject::invokeMethod(this, [this, generation, key]() {
            if (generation == m_generation && m_thread) {
                emit artworkMissed(key);
            }
        }, Qt::QueuedConnection);
    };

    m_thread = new QThread();
    m_thread->setObjectName(QStringLiteral("StreamServer"));
    m_worker = new StreamServerWorker(m_info, m_files, m_attached, m_lister,
                                      m_artworkDirectory, std::move(sink),
                                      std::move(misses));
    m_worker->moveToThread(m_thread);
    m_thread->start();

    quint16 bound = 0;
    QString error;
    StreamServerWorker *worker = m_worker;
    QMetaObject::invokeMethod(m_worker, [worker, port, &bound, &error]() {
        bound = worker->listen(port, &error);
    }, Qt::BlockingQueuedConnection);

    if (bound == 0) {
        MM_LOG_E() << "stream: could not listen on port" << port << "-" << error;
        stop();
        m_error = error;
        return false;
    }

    m_port = bound;
    MM_LOG_I() << "stream: serving" << m_files.size() << "files on port" << m_port;
    return true;
}

void StreamServer::stop()
{
    if (!m_thread) {
        return;
    }

    StreamServerWorker *worker = m_worker;
    QMetaObject::invokeMethod(m_worker, [worker]() { worker->shutDown(); },
                              Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
    delete m_worker;
    m_worker = nullptr;
    delete m_thread;
    m_thread = nullptr;

    ++m_generation;

    if (m_port != 0) {
        MM_LOG_I() << "stream: stopped serving on port" << m_port;
    }
    m_port = 0;
    takeDevices({});
}

QList<StreamDevice> StreamServer::devices() const
{
    return m_devices;
}

void StreamServer::takeDevices(const QList<StreamDevice> &devices)
{
    if (m_devices == devices) {
        return;
    }
    m_devices = devices;
    emit devicesChanged();
}

void StreamServer::setDeveloperSilence(int milliseconds)
{
    if (!m_worker) {
        return;
    }
    StreamServerWorker *worker = m_worker;
    QMetaObject::invokeMethod(m_worker, [worker, milliseconds]() {
        worker->setSilence(milliseconds);
    }, Qt::QueuedConnection);
}

void StreamServer::setDeveloperThrottle(qint64 bytesPerSecond)
{
    if (!m_worker) {
        return;
    }
    StreamServerWorker *worker = m_worker;
    QMetaObject::invokeMethod(m_worker, [worker, bytesPerSecond]() {
        worker->setThrottle(bytesPerSecond);
        MM_LOG_I() << "stream: developer -"
                   << (bytesPerSecond > 0
                           ? QStringLiteral("files sent at %1 KB/s").arg(bytesPerSecond / 1024)
                           : QStringLiteral("files sent at full speed"));
    }, Qt::QueuedConnection);
}

bool StreamServer::isRunning() const
{
    return m_thread && m_port != 0;
}

quint16 StreamServer::port() const
{
    return m_port;
}

QString StreamServer::errorString() const
{
    return m_error;
}
