#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "Metadata/PosterCache.h"
#include "Streaming/AddressFilter.h"
#include "Streaming/ArtworkLookup.h"
#include "Streaming/HttpMessage.h"
#include "Streaming/RemoteMediaSource.h"
#include "Streaming/StreamProtocol.h"
#include "Streaming/StreamServer.h"

namespace {

constexpr qint64 kFileBytes = 3 * 1024 * 1024 + 17;

QByteArray patterned(qint64 size)
{
    QByteArray bytes;
    bytes.resize(size);
    for (qint64 i = 0; i < size; ++i) {
        bytes[i] = char((i * 31 + i / 7) & 0xff);
    }
    return bytes;
}

}

class TestStreamServer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void aRequestIsReadWhenItsHeadIsComplete();
    void aRequestWaitsForItsBody();
    void keepAliveFollowsTheVersionAndTheHeader();
    void anUnreadableRequestIsBad();

    void rangesAreReadTheWayMpvSendsThem();
    void aRangePastTheEndCannotBeSatisfied();
    void aRangeThatIsNotBytesIsIgnored();

    void onlyPrivateAddressesArePeers();
    void theRemoteUrlNamesTheFileById();

    void helloSaysWhoTheServerIs();
    void aWholeFileArrivesIntact();
    void aRangeInTheMiddleArrivesAsPartialContent();
    void anOpenEndedRangeRunsToTheEnd();
    void aRangePastTheEndIsRefused();
    void anUnknownIdIsNotFound();
    void aClientLeavingMidFileDoesNotStopTheServer();
    void theLibraryRevisionAndSnapshotAreServed();

    void attachedAndBesideSubtitlesAreListedOnce();
    void aWindows1250SubtitleArrivesAsUtf8();
    void aVobSubIndexFindsItsPictures();
    void aSubtitleNotOnTheListIsRefused();

    void artworkIsServedByItsKey();
    void artworkAtAnotherWidthStandsIn();
    void artworkThatIsNotThereIsNotFound();
    void theServerNamesArtworkTheWayTheCacheDoes();

    void aCallWithoutASessionIsRefused();
    void theSessionAnswerSaysWhichProtocolItSpeaks();
    void aDeviceKeepsItsSessionAndCanLeave();
    void heartbeatsReachTheDeviceList();
    void aStoppingServerTellsWhoIsListening();
    void theTokenIsKeptOutOfWhatIsLogged();

private:
    QNetworkReply *get(const QString &path, const QByteArray &range = QByteArray());
    QNetworkReply *bare(const QString &path);
    QNetworkReply *send(const QByteArray &verb, const QString &url, const QByteArray &body);
    QString openSession(const QString &serverBase, const QString &device, const QString &name);
    QString base() const;

    QString m_token;
    QTemporaryDir m_dir;
    QByteArray m_content;
    QByteArray m_vobSub;
    QByteArray m_poster;
    QByteArray m_backdrop;
    StreamServer m_server;
    QNetworkAccessManager m_network;
};

void TestStreamServer::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_content = patterned(kFileBytes);

    const QString path = m_dir.filePath(QStringLiteral("film.mkv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(m_content), kFileBytes);
    file.close();

    const QString snapshotPath = m_dir.filePath(QStringLiteral("snapshot.sqlite"));
    QFile snapshot(snapshotPath);
    QVERIFY(snapshot.open(QIODevice::WriteOnly));
    snapshot.write("SQLite format 3 pretend");
    snapshot.close();

    StreamServerInfo info;
    info.serverId = QStringLiteral("server-1");
    info.name = QStringLiteral("DESKTOP-TEST");
    info.schemaVersion = 17;
    info.snapshotPath = snapshotPath;
    info.revision = QStringLiteral("0123456789abcdef0123456789abcdef01234567");
    info.snapshotBytes = 23;
    const auto write = [](const QString &file, const QByteArray &bytes) {
        QDir().mkpath(QFileInfo(file).absolutePath());
        QFile out(file);
        if (!out.open(QIODevice::WriteOnly)) {
            return false;
        }
        out.write(bytes);
        return true;
    };
    QVERIFY(write(m_dir.filePath(QStringLiteral("film.hr.srt")),
                  QByteArray("1\r\n00:00:01,000 --> 00:00:02,000\r\n")
                  + QByteArray::fromHex("84416b6f206d6f9e659a2c209a75746972616a2e93")));
    QVERIFY(write(m_dir.filePath(QStringLiteral("film.idx")),
                  "# VobSub index file, v7\nsize: 720x480\n"));
    m_vobSub = QByteArray("\x00\x00\x01\xba", 4) + QByteArray(2048, '\x7f');
    QVERIFY(write(m_dir.filePath(QStringLiteral("film.sub")), m_vobSub));
    const QString attachedPath = m_dir.filePath(QStringLiteral("elsewhere/attached.en.srt"));
    QVERIFY(write(attachedPath, "1\n00:00:01,000 --> 00:00:02,000\nHello\n"));

    const QString dir = m_dir.path();
    m_server.setSubtitleLister([dir](const QString &videoPath) {
        if (!videoPath.endsWith(QLatin1String("film.mkv"))) {
            return QStringList();
        }
        return QStringList({ dir + QStringLiteral("/film.hr.srt"),
                             dir + QStringLiteral("/film.idx"),
                             dir + QStringLiteral("/film.sub") });
    });
    m_server.setAttachedSubtitles({ { 42, { AttachedSubtitle{ attachedPath,
                                                              QStringLiteral("Mine") } } } });

    m_poster = QByteArray::fromHex("ffd8ffe000104a464946") + QByteArray(500, 'p');
    m_backdrop = QByteArray::fromHex("ffd8ffe000104a464946") + QByteArray(700, 'b');
    const QString posters = m_dir.filePath(QStringLiteral("posters"));
    QVERIFY(write(posters + QLatin1Char('/')
                  + ArtworkLookup::cacheFileName(QStringLiteral("/heat.jpg@342")), m_poster));
    QVERIFY(write(posters + QLatin1Char('/')
                  + ArtworkLookup::cacheFileName(QStringLiteral("backdrop:/heat-wide.jpg@780")),
                  m_backdrop));
    m_server.setArtworkDirectory(posters);

    m_server.setInfo(info);
    m_server.setFiles({ { 42, path }, { 43, m_dir.filePath(QStringLiteral("gone.mkv")) } });
    QVERIFY2(m_server.start(0), qPrintable(m_server.errorString()));
    QVERIFY(m_server.port() != 0);

    m_token = openSession(base(), QStringLiteral("device-tests"), QStringLiteral("Test tablet"));
    QVERIFY(!m_token.isEmpty());
}

void TestStreamServer::cleanupTestCase()
{
    m_server.stop();
    QVERIFY(!m_server.isRunning());
}

QString TestStreamServer::base() const
{
    return QStringLiteral("http://127.0.0.1:%1").arg(m_server.port());
}

QNetworkReply *TestStreamServer::get(const QString &path, const QByteArray &range)
{
    QNetworkRequest request(QUrl(base() + StreamProtocol::tokenPrefix(m_token) + path));
    if (!range.isEmpty()) {
        request.setRawHeader("Range", range);
    }
    QNetworkReply *reply = m_network.get(request);
    QSignalSpy finished(reply, &QNetworkReply::finished);
    if (!reply->isFinished()) {
        finished.wait(10000);
    }
    return reply;
}

QNetworkReply *TestStreamServer::bare(const QString &path)
{
    QNetworkReply *reply = m_network.get(QNetworkRequest(QUrl(base() + path)));
    QSignalSpy finished(reply, &QNetworkReply::finished);
    if (!reply->isFinished()) {
        finished.wait(10000);
    }
    return reply;
}

QNetworkReply *TestStreamServer::send(const QByteArray &verb, const QString &url,
                                      const QByteArray &body)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_network.sendCustomRequest(request, verb, body);
    QSignalSpy finished(reply, &QNetworkReply::finished);
    if (!reply->isFinished()) {
        finished.wait(10000);
    }
    return reply;
}

QString TestStreamServer::openSession(const QString &serverBase, const QString &device,
                                      const QString &name)
{
    const QJsonObject asking{
        { QStringLiteral("device"), device },
        { QStringLiteral("name"), name },
        { QStringLiteral("form"), QStringLiteral("tablet") }
    };
    QNetworkReply *reply = send("POST", serverBase + QStringLiteral("/v1/session"),
                                QJsonDocument(asking).toJson(QJsonDocument::Compact));
    const QString token = QJsonDocument::fromJson(reply->readAll())
                              .object().value(QStringLiteral("token")).toString();
    reply->deleteLater();
    return token;
}

void TestStreamServer::aRequestIsReadWhenItsHeadIsComplete()
{
    QByteArray buffer = "GET /v1/file/7?t=abc HTTP/1.1\r\nHost: x\r\nRange: bytes=0-\r\n";
    HttpRequest request;
    QCOMPARE(HttpMessage::takeRequest(buffer, request), HttpMessage::Parse::NeedMore);

    buffer += "\r\nGET /v1/hello HTTP/1.1\r\n\r\n";
    QCOMPARE(HttpMessage::takeRequest(buffer, request), HttpMessage::Parse::Ready);
    QCOMPARE(request.method, QByteArray("GET"));
    QCOMPARE(request.path, QStringLiteral("/v1/file/7"));
    QCOMPARE(request.query, QByteArray("t=abc"));
    QCOMPARE(request.header("RANGE"), QByteArray("bytes=0-"));
    QVERIFY(request.keepAlive);

    QCOMPARE(HttpMessage::takeRequest(buffer, request), HttpMessage::Parse::Ready);
    QCOMPARE(request.path, QStringLiteral("/v1/hello"));
    QVERIFY(buffer.isEmpty());
}

void TestStreamServer::aRequestWaitsForItsBody()
{
    QByteArray buffer = "POST /v1/session HTTP/1.1\r\nContent-Length: 5\r\n\r\nab";
    HttpRequest request;
    QCOMPARE(HttpMessage::takeRequest(buffer, request), HttpMessage::Parse::NeedMore);
    buffer += "cde";
    QCOMPARE(HttpMessage::takeRequest(buffer, request), HttpMessage::Parse::Ready);
    QCOMPARE(request.body, QByteArray("abcde"));
}

void TestStreamServer::keepAliveFollowsTheVersionAndTheHeader()
{
    HttpRequest request;
    QByteArray closing = "GET / HTTP/1.1\r\nConnection: close\r\n\r\n";
    QCOMPARE(HttpMessage::takeRequest(closing, request), HttpMessage::Parse::Ready);
    QVERIFY(!request.keepAlive);

    QByteArray old = "GET / HTTP/1.0\r\n\r\n";
    QCOMPARE(HttpMessage::takeRequest(old, request), HttpMessage::Parse::Ready);
    QVERIFY(!request.keepAlive);

    QByteArray oldKept = "GET / HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n";
    QCOMPARE(HttpMessage::takeRequest(oldKept, request), HttpMessage::Parse::Ready);
    QVERIFY(request.keepAlive);
}

void TestStreamServer::anUnreadableRequestIsBad()
{
    HttpRequest request;
    QByteArray noVersion = "GET /\r\n\r\n";
    QCOMPARE(HttpMessage::takeRequest(noVersion, request), HttpMessage::Parse::Bad);

    QByteArray relative = "GET v1/hello HTTP/1.1\r\n\r\n";
    QCOMPARE(HttpMessage::takeRequest(relative, request), HttpMessage::Parse::Bad);

    QByteArray huge(HttpMessage::MaxHeadBytes + 10, 'a');
    QCOMPARE(HttpMessage::takeRequest(huge, request), HttpMessage::Parse::Bad);

    QByteArray tooMuchBody = "POST / HTTP/1.1\r\nContent-Length: 99999999\r\n\r\n";
    QCOMPARE(HttpMessage::takeRequest(tooMuchBody, request), HttpMessage::Parse::Bad);
}

void TestStreamServer::rangesAreReadTheWayMpvSendsThem()
{
    const ByteRange fromStart = HttpMessage::parseRange("bytes=0-", 1000);
    QVERIFY(fromStart.requested && fromStart.satisfiable);
    QCOMPARE(fromStart.start, Q_INT64_C(0));
    QCOMPARE(fromStart.end, Q_INT64_C(999));

    const ByteRange middle = HttpMessage::parseRange("bytes=100-199", 1000);
    QCOMPARE(middle.start, Q_INT64_C(100));
    QCOMPARE(middle.end, Q_INT64_C(199));
    QCOMPARE(middle.length(), Q_INT64_C(100));

    const ByteRange pastEndClamped = HttpMessage::parseRange("bytes=900-5000", 1000);
    QCOMPARE(pastEndClamped.end, Q_INT64_C(999));

    const ByteRange suffix = HttpMessage::parseRange("bytes=-100", 1000);
    QCOMPARE(suffix.start, Q_INT64_C(900));
    QCOMPARE(suffix.end, Q_INT64_C(999));

    const ByteRange firstOfMany = HttpMessage::parseRange("bytes=10-19, 50-59", 1000);
    QCOMPARE(firstOfMany.start, Q_INT64_C(10));
    QCOMPARE(firstOfMany.end, Q_INT64_C(19));
}

void TestStreamServer::aRangePastTheEndCannotBeSatisfied()
{
    const ByteRange past = HttpMessage::parseRange("bytes=1000-", 1000);
    QVERIFY(past.requested);
    QVERIFY(!past.satisfiable);

    const ByteRange emptySuffix = HttpMessage::parseRange("bytes=-0", 1000);
    QVERIFY(!emptySuffix.satisfiable);
}

void TestStreamServer::aRangeThatIsNotBytesIsIgnored()
{
    QVERIFY(!HttpMessage::parseRange(QByteArray(), 1000).requested);
    QVERIFY(!HttpMessage::parseRange("items=0-5", 1000).requested);
    QVERIFY(!HttpMessage::parseRange("bytes=abc-", 1000).requested);
    QVERIFY(!HttpMessage::parseRange("bytes=50-10", 1000).requested);
}

void TestStreamServer::onlyPrivateAddressesArePeers()
{
    for (const char *address : { "10.0.0.5", "172.16.0.1", "172.31.255.255", "192.168.1.12",
                                 "127.0.0.1", "169.254.10.10", "::1", "fd12:3456::1",
                                 "fe80::1", "::ffff:192.168.1.12" }) {
        QVERIFY2(AddressFilter::isPrivatePeer(QHostAddress(QString::fromLatin1(address))),
                 address);
    }
    for (const char *address : { "8.8.8.8", "172.32.0.1", "192.169.1.1", "11.0.0.1",
                                 "2001:4860::8888", "::ffff:8.8.8.8" }) {
        QVERIFY2(!AddressFilter::isPrivatePeer(QHostAddress(QString::fromLatin1(address))),
                 address);
    }
    QVERIFY(!AddressFilter::isPrivatePeer(QHostAddress()));
    QCOMPARE(AddressFilter::readable(QHostAddress(QStringLiteral("::ffff:192.168.1.12"))),
             QStringLiteral("192.168.1.12"));
}

void TestStreamServer::theRemoteUrlNamesTheFileById()
{
    RemoteMediaSource source(QStringLiteral("192.168.1.20"), 47811);
    QCOMPARE(source.urlForFileId(42), QStringLiteral("http://192.168.1.20:47811/v1/file/42"));
    QCOMPARE(source.mpvUrl(QStringLiteral("42")), QStringLiteral("http://192.168.1.20:47811/v1/file/42"));
    QVERIFY(source.mpvUrl(QStringLiteral("D:/Movies/Heat.mkv")).isEmpty());

    source.setIdResolver([](const QString &handle) {
        return handle == QLatin1String("D:/Movies/Heat.mkv") ? qint64(7) : qint64(0);
    });
    QCOMPARE(source.mpvUrl(QStringLiteral("D:/Movies/Heat.mkv")),
             QStringLiteral("http://192.168.1.20:47811/v1/file/7"));

    RemoteMediaSource six(QStringLiteral("fd00::5"), 47811);
    QCOMPARE(six.baseUrl(), QStringLiteral("http://[fd00::5]:47811"));

    RemoteMediaSource withSession(QStringLiteral("192.168.1.20"), 47811, QStringLiteral("ab12"));
    QCOMPARE(withSession.baseUrl(), QStringLiteral("http://192.168.1.20:47811"));
    QCOMPARE(withSession.sessionUrl(), QStringLiteral("http://192.168.1.20:47811/t/ab12"));
    QCOMPARE(withSession.urlForFileId(42),
             QStringLiteral("http://192.168.1.20:47811/t/ab12/v1/file/42"));
}

void TestStreamServer::helloSaysWhoTheServerIs()
{
    QNetworkReply *reply = get(QStringLiteral("/v1/hello"));
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    const QJsonObject hello = QJsonDocument::fromJson(reply->readAll()).object();
    QCOMPARE(hello.value(QStringLiteral("mm")).toString(), QStringLiteral("hello"));
    QCOMPARE(hello.value(QStringLiteral("v")).toInt(), 1);
    QCOMPARE(hello.value(QStringLiteral("id")).toString(), QStringLiteral("server-1"));
    QCOMPARE(hello.value(QStringLiteral("name")).toString(), QStringLiteral("DESKTOP-TEST"));
    QCOMPARE(hello.value(QStringLiteral("schema")).toInt(), 17);
    reply->deleteLater();
}

void TestStreamServer::aWholeFileArrivesIntact()
{
    QNetworkReply *reply = get(QStringLiteral("/v1/file/42"));
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    QCOMPARE(reply->rawHeader("Accept-Ranges"), QByteArray("bytes"));
    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader).toString(),
             QStringLiteral("video/x-matroska"));
    const QByteArray body = reply->readAll();
    QCOMPARE(body.size(), m_content.size());
    QVERIFY(body == m_content);
    reply->deleteLater();
}

void TestStreamServer::aRangeInTheMiddleArrivesAsPartialContent()
{
    QNetworkReply *reply = get(QStringLiteral("/v1/file/42"), "bytes=1000000-1999999");
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 206);
    QCOMPARE(reply->rawHeader("Content-Range"),
             QByteArray("bytes 1000000-1999999/") + QByteArray::number(kFileBytes));
    const QByteArray body = reply->readAll();
    QCOMPARE(body.size(), qsizetype(1000000));
    QVERIFY(body == m_content.mid(1000000, 1000000));
    reply->deleteLater();
}

void TestStreamServer::anOpenEndedRangeRunsToTheEnd()
{
    const qint64 start = kFileBytes - 12345;
    QNetworkReply *reply = get(QStringLiteral("/v1/file/42"),
                               "bytes=" + QByteArray::number(start) + '-');
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 206);
    const QByteArray body = reply->readAll();
    QCOMPARE(body.size(), qsizetype(12345));
    QVERIFY(body == m_content.mid(start));
    reply->deleteLater();
}

void TestStreamServer::aRangePastTheEndIsRefused()
{
    QNetworkReply *reply = get(QStringLiteral("/v1/file/42"),
                               "bytes=" + QByteArray::number(kFileBytes) + '-');
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 416);
    QCOMPARE(reply->rawHeader("Content-Range"),
             QByteArray("bytes */") + QByteArray::number(kFileBytes));
    reply->deleteLater();
}

void TestStreamServer::anUnknownIdIsNotFound()
{
    for (const QString &path : { QStringLiteral("/v1/file/999"), QStringLiteral("/v1/file/43"),
                                 QStringLiteral("/v1/file/abc"),
                                 QStringLiteral("/v1/file/..%2F..%2Fsecret"),
                                 QStringLiteral("/v1/nothing") }) {
        QNetworkReply *reply = get(path);
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 404);
        reply->deleteLater();
    }
}

void TestStreamServer::aClientLeavingMidFileDoesNotStopTheServer()
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, m_server.port());
    QVERIFY(socket.waitForConnected(5000));
    socket.write("GET " + StreamProtocol::tokenPrefix(m_token).toLatin1()
                 + "/v1/file/42 HTTP/1.1\r\nHost: test\r\n\r\n");
    QVERIFY(socket.waitForReadyRead(5000));
    QVERIFY(socket.readAll().startsWith("HTTP/1.1 200"));
    socket.abort();

    QTest::qWait(200);

    QNetworkReply *reply = get(QStringLiteral("/v1/hello"));
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    reply->deleteLater();

    QNetworkReply *file = get(QStringLiteral("/v1/file/42"), "bytes=0-99");
    QCOMPARE(file->readAll(), m_content.left(100));
    file->deleteLater();
}

void TestStreamServer::theLibraryRevisionAndSnapshotAreServed()
{
    QNetworkReply *library = get(QStringLiteral("/v1/library"));
    QCOMPARE(library->error(), QNetworkReply::NoError);
    const QJsonObject info = QJsonDocument::fromJson(library->readAll()).object();
    QCOMPARE(info.value(QStringLiteral("revision")).toString(),
             QStringLiteral("0123456789abcdef0123456789abcdef01234567"));
    QCOMPARE(info.value(QStringLiteral("bytes")).toInteger(), qint64(23));
    QCOMPARE(info.value(QStringLiteral("schema")).toInt(), 17);
    library->deleteLater();

    QNetworkReply *snapshot = get(QStringLiteral("/v1/library/snapshot"));
    QCOMPARE(snapshot->error(), QNetworkReply::NoError);
    QCOMPARE(snapshot->readAll(), QByteArray("SQLite format 3 pretend"));
    snapshot->deleteLater();
}

void TestStreamServer::attachedAndBesideSubtitlesAreListedOnce()
{
    QNetworkReply *reply = get(QStringLiteral("/v1/file/42/subtitles"));
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    const QJsonArray listed = QJsonDocument::fromJson(reply->readAll())
                                  .object().value(QStringLiteral("subtitles")).toArray();
    reply->deleteLater();

    QCOMPARE(listed.size(), qsizetype(3));
    QCOMPARE(listed.at(0).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("attached.en.srt"));
    QVERIFY(listed.at(0).toObject().value(QStringLiteral("attached")).toBool());
    QCOMPARE(listed.at(0).toObject().value(QStringLiteral("title")).toString(),
             QStringLiteral("Mine"));
    QCOMPARE(listed.at(1).toObject().value(QStringLiteral("url")).toString(),
             QStringLiteral("/v1/file/42/sub/1/film.hr.srt"));
    QCOMPARE(listed.at(2).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("film.idx"));

    QNetworkReply *none = get(QStringLiteral("/v1/file/999/subtitles"));
    QCOMPARE(none->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 404);
    none->deleteLater();
}

void TestStreamServer::aWindows1250SubtitleArrivesAsUtf8()
{
    QNetworkReply *reply = get(QStringLiteral("/v1/file/42/sub/1/film.hr.srt"));
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    QVERIFY(reply->header(QNetworkRequest::ContentTypeHeader).toString()
                .contains(QLatin1String("utf-8")));
    const QString text = QString::fromUtf8(reply->readAll());
    QVERIFY2(text.contains(QStringLiteral("Ako možeš, šutiraj.")), qPrintable(text));
    reply->deleteLater();

    QNetworkReply *attached = get(QStringLiteral("/v1/file/42/sub/0/attached.en.srt"));
    QVERIFY(QString::fromUtf8(attached->readAll()).contains(QStringLiteral("Hello")));
    attached->deleteLater();
}

void TestStreamServer::aVobSubIndexFindsItsPictures()
{
    QNetworkReply *index = get(QStringLiteral("/v1/file/42/sub/2/film.idx"));
    QCOMPARE(index->error(), QNetworkReply::NoError);
    QVERIFY(index->readAll().startsWith("# VobSub index file"));
    index->deleteLater();

    QNetworkReply *pictures = get(QStringLiteral("/v1/file/42/sub/2/film.sub"));
    QCOMPARE(pictures->error(), QNetworkReply::NoError);
    QVERIFY(pictures->readAll() == m_vobSub);
    pictures->deleteLater();
}

void TestStreamServer::aSubtitleNotOnTheListIsRefused()
{
    for (const QString &path : { QStringLiteral("/v1/file/42/sub/1/other.srt"),
                                 QStringLiteral("/v1/file/42/sub/1/..%2F..%2Fsecret.srt"),
                                 QStringLiteral("/v1/file/42/sub/9/film.hr.srt"),
                                 QStringLiteral("/v1/file/42/sub/2/..%2Ffilm.sub"),
                                 QStringLiteral("/v1/file/42/sub/1/film.sub"),
                                 QStringLiteral("/v1/file/43/sub/0/attached.en.srt") }) {
        QNetworkReply *reply = get(path);
        QVERIFY2(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404,
                 qPrintable(path));
        reply->deleteLater();
    }
}

void TestStreamServer::artworkIsServedByItsKey()
{
    QNetworkReply *poster = get(QStringLiteral("/v1/art?k=")
                                + QString::fromLatin1(QUrl::toPercentEncoding(
                                    QStringLiteral("/heat.jpg@342"))));
    QCOMPARE(poster->error(), QNetworkReply::NoError);
    QCOMPARE(poster->header(QNetworkRequest::ContentTypeHeader).toString(),
             QStringLiteral("image/jpeg"));
    QVERIFY(poster->readAll() == m_poster);
    poster->deleteLater();

    QNetworkReply *backdrop = get(QStringLiteral("/v1/art?k=")
                                  + QString::fromLatin1(QUrl::toPercentEncoding(
                                      QStringLiteral("backdrop:/heat-wide.jpg@780"))));
    QVERIFY(backdrop->readAll() == m_backdrop);
    backdrop->deleteLater();
}

void TestStreamServer::artworkAtAnotherWidthStandsIn()
{
    QNetworkReply *reply = get(QStringLiteral("/v1/art?k=")
                               + QString::fromLatin1(QUrl::toPercentEncoding(
                                   QStringLiteral("/heat.jpg@500"))));
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    QVERIFY(reply->readAll() == m_poster);
    reply->deleteLater();
}

void TestStreamServer::artworkThatIsNotThereIsNotFound()
{
    for (const QString &key : { QStringLiteral("/nothing.jpg@342"),
                                QStringLiteral("../../secret@342"),
                                QStringLiteral("still:/heat.jpg@300"),
                                QString() }) {
        QNetworkReply *reply = get(QStringLiteral("/v1/art?k=")
                                   + QString::fromLatin1(QUrl::toPercentEncoding(key)));
        QVERIFY2(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404,
                 qPrintable(key));
        reply->deleteLater();
    }
}

void TestStreamServer::theServerNamesArtworkTheWayTheCacheDoes()
{
    QTemporaryDir dir;
    QNetworkAccessManager network;
    PosterCache cache(network, dir.path());
    const QString key = QStringLiteral("still:/episode.jpg@300");
    QVERIFY(cache.store(key, m_poster));
    const QString local = QUrl(cache.localUrl(key)).toLocalFile();
    QCOMPARE(QFileInfo(local).fileName(), ArtworkLookup::cacheFileName(key));
    QCOMPARE(ArtworkLookup::fileFor(dir.path(), key), local);
}

void TestStreamServer::aCallWithoutASessionIsRefused()
{
    QNetworkReply *hello = bare(QStringLiteral("/v1/hello"));
    QCOMPARE(hello->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    hello->deleteLater();

    for (const QString &path : { QStringLiteral("/v1/file/42"),
                                 QStringLiteral("/v1/library"),
                                 QStringLiteral("/v1/library/snapshot"),
                                 QStringLiteral("/v1/file/42/subtitles"),
                                 QStringLiteral("/v1/art?k=%2Fheat.jpg%40342"),
                                 QStringLiteral("/t/not-a-session/v1/file/42"),
                                 QStringLiteral("/t//v1/file/42"),
                                 QStringLiteral("/t/") + m_token + QStringLiteral("x/v1/file/42") }) {
        QNetworkReply *reply = bare(path);
        QVERIFY2(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401,
                 qPrintable(path));
        reply->deleteLater();
    }
}

void TestStreamServer::theSessionAnswerSaysWhichProtocolItSpeaks()
{
    const QJsonObject asking{
        { QStringLiteral("device"), QStringLiteral("device-versions") },
        { QStringLiteral("name"), QStringLiteral("Phone") },
        { QStringLiteral("form"), QStringLiteral("phone") }
    };
    QNetworkReply *reply = send("POST", base() + QStringLiteral("/v1/session"),
                                QJsonDocument(asking).toJson(QJsonDocument::Compact));
    const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();

    QCOMPARE(answer.value(QStringLiteral("v")).toInt(), StreamProtocol::Version);
    QVERIFY(!answer.value(QStringLiteral("token")).toString().isEmpty());

    QNetworkReply *hello = bare(QStringLiteral("/v1/hello"));
    const QJsonObject said = QJsonDocument::fromJson(hello->readAll()).object();
    hello->deleteLater();
    QCOMPARE(said.value(QStringLiteral("v")).toInt(), StreamProtocol::Version);
    QCOMPARE(said.value(QStringLiteral("v")).toInt(),
             answer.value(QStringLiteral("v")).toInt());

    QNetworkReply *bye = send("DELETE",
                              base()
                                  + StreamProtocol::tokenPrefix(
                                      answer.value(QStringLiteral("token")).toString())
                                  + QStringLiteral("/v1/session"),
                              QByteArray());
    bye->deleteLater();
}

void TestStreamServer::aDeviceKeepsItsSessionAndCanLeave()
{
    const QString first = openSession(base(), QStringLiteral("device-leaves"),
                                       QStringLiteral("Phone"));
    const QString again = openSession(base(), QStringLiteral("device-leaves"),
                                      QStringLiteral("Phone"));
    QVERIFY(!first.isEmpty());
    QCOMPARE(again, first);
    QVERIFY(first != m_token);
    QCOMPARE(first.size(), qsizetype(32));

    QNetworkReply *file = bare(StreamProtocol::tokenPrefix(first) + QStringLiteral("/v1/file/42"));
    QCOMPARE(file->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    file->deleteLater();

    QNetworkReply *leave = send("DELETE", base() + StreamProtocol::tokenPrefix(first)
                                              + QStringLiteral("/v1/session"), QByteArray());
    QCOMPARE(leave->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    leave->deleteLater();

    QNetworkReply *after = bare(StreamProtocol::tokenPrefix(first) + QStringLiteral("/v1/file/42"));
    QCOMPARE(after->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
    after->deleteLater();

    const QString fresh = openSession(base(), QStringLiteral("device-leaves"),
                                      QStringLiteral("Phone"));
    QVERIFY(!fresh.isEmpty());
    QVERIFY(fresh != first);
    QNetworkReply *bye = send("DELETE", base() + StreamProtocol::tokenPrefix(fresh)
                                            + QStringLiteral("/v1/session"), QByteArray());
    bye->deleteLater();
}

void TestStreamServer::heartbeatsReachTheDeviceList()
{
    const auto findTablet = [this]() {
        const QList<StreamDevice> devices = m_server.devices();
        for (const StreamDevice &device : devices) {
            if (device.deviceId == QLatin1String("device-tests")) {
                return device;
            }
        }
        return StreamDevice();
    };

    QTRY_COMPARE(findTablet().name, QStringLiteral("Test tablet"));
    QCOMPARE(findTablet().form, QStringLiteral("tablet"));
    QVERIFY(!findTablet().watching());

    const QJsonObject beat{
        { QStringLiteral("title"), QStringLiteral("Heat") },
        { QStringLiteral("file"), 42 },
        { QStringLiteral("position"), 125.0 },
        { QStringLiteral("duration"), 10200.0 },
        { QStringLiteral("paused"), true }
    };
    QNetworkReply *reply = send("POST", base() + StreamProtocol::tokenPrefix(m_token)
                                            + QStringLiteral("/v1/heartbeat"),
                                QJsonDocument(beat).toJson(QJsonDocument::Compact));
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    reply->deleteLater();

    QTRY_COMPARE(findTablet().title, QStringLiteral("Heat"));
    QCOMPARE(findTablet().fileId, Q_INT64_C(42));
    QCOMPARE(findTablet().position, 125.0);
    QCOMPARE(findTablet().duration, 10200.0);
    QVERIFY(findTablet().paused);

    QNetworkReply *idle = send("POST", base() + StreamProtocol::tokenPrefix(m_token)
                                           + QStringLiteral("/v1/heartbeat"), "{}");
    idle->deleteLater();
    QTRY_VERIFY(!findTablet().watching());
    QCOMPARE(findTablet().position, 0.0);
    QCOMPARE(findTablet().fileId, Q_INT64_C(-1));

    QNetworkReply *stranger = bare(QStringLiteral("/t/nobody/v1/heartbeat"));
    QCOMPARE(stranger->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
    stranger->deleteLater();
}

void TestStreamServer::aStoppingServerTellsWhoIsListening()
{
    StreamServer server;
    StreamServerInfo info;
    info.serverId = QStringLiteral("server-2");
    info.name = QStringLiteral("DESKTOP-STOPS");
    server.setInfo(info);
    server.setFiles({ { 42, m_dir.filePath(QStringLiteral("film.mkv")) } });
    QVERIFY(server.start(0));
    const QString serverBase = QStringLiteral("http://127.0.0.1:%1").arg(server.port());

    const QString token = openSession(serverBase, QStringLiteral("device-stops"),
                                      QStringLiteral("Phone"));
    QVERIFY(!token.isEmpty());
    QTRY_COMPARE(server.devices().size(), qsizetype(1));

    QNetworkReply *events = m_network.get(QNetworkRequest(
        QUrl(serverBase + StreamProtocol::tokenPrefix(token) + QStringLiteral("/v1/events"))));
    QSignalSpy finished(events, &QNetworkReply::finished);
    QTest::qWait(300);
    QVERIFY(!events->isFinished());

    server.stop();
    QVERIFY(events->isFinished() || finished.wait(5000));
    QCOMPARE(events->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 503);
    QVERIFY(server.devices().isEmpty());
    events->deleteLater();
}

void TestStreamServer::theTokenIsKeptOutOfWhatIsLogged()
{
    QCOMPARE(StreamProtocol::withoutToken(
                 QStringLiteral("http://192.168.0.9:47811/t/0123abcd/v1/file/42")),
             QStringLiteral("http://192.168.0.9:47811/t/-/v1/file/42"));
    QCOMPARE(StreamProtocol::withoutToken(
                 QStringLiteral("Error transferring http://h:1/t/abc/v1/art?k=x - server replied")),
             QStringLiteral("Error transferring http://h:1/t/-/v1/art?k=x - server replied"));
    QCOMPARE(StreamProtocol::withoutToken(QStringLiteral("http://h:1/t/abc")),
             QStringLiteral("http://h:1/t/-"));
    QCOMPARE(StreamProtocol::withoutToken(QStringLiteral("D:/Movies/Heat.mkv")),
             QStringLiteral("D:/Movies/Heat.mkv"));
}

QTEST_GUILESS_MAIN(TestStreamServer)
#include "tst_streamserver.moc"
