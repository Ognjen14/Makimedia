#include "Streaming/StreamingController.h"

#include "AppSettings.h"
#include "Data/Database.h"
#include "Data/FileRepository.h"
#include "Data/MediaRepository.h"
#include "Library/LibraryController.h"
#include "MmLog.h"
#include "Metadata/MetadataService.h"
#include "Metadata/PosterCache.h"
#include "Streaming/AddressFilter.h"
#include "Library/LibrarySetup.h"
#include "Library/SubtitleFinder.h"
#include "Platform/MediaFileInfo.h"
#include "Platform/PlatformServices.h"
#include "Player/MpvController.h"
#include "Streaming/LibrarySnapshot.h"
#include "Streaming/Mirror.h"
#include "Streaming/RemoteMediaSource.h"
#include "Streaming/StreamProtocol.h"
#include "SystemBridge.h"

#include <QDir>
#include <QGuiApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QStandardPaths>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QUrl>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

#include <algorithm>

namespace {

constexpr int kHelloTimeoutMs = 5000;
constexpr int kGoodbyeTimeoutMs = 2000;
constexpr qint64 kServerGoneMs = 8000;
constexpr int kQuietWaitStepMs = 100;
constexpr int kQuietWaitSteps = 100;
constexpr int kTabletSmallestSide = 600;
constexpr int kDeveloperSilenceMs = 60000;
constexpr qint64 kDeveloperSlowBytesPerSecond = 256 * 1024;

}

StreamingController::StreamingController(AppSettings &settings, Database &database,
                                         LibraryController &library,
                                         MetadataService &metadata,
                                         SystemBridge &system, PlatformServices &platform,
                                         LibrarySetup &setup, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_database(database)
    , m_library(library)
    , m_metadata(metadata)
    , m_system(system)
    , m_platform(platform)
    , m_setup(setup)
    , m_servers(new FoundServerModel(this))
    , m_sync(&m_network)
    , m_stored(new StoredMirrorModel(this))
    , m_devices(new ConnectedDeviceModel(this))
{
    m_clock.start();

    m_heartbeat.setInterval(StreamProtocol::HeartbeatIntervalMs);
    connect(&m_heartbeat, &QTimer::timeout, this, &StreamingController::sendHeartbeat);

    connect(&m_server, &StreamServer::devicesChanged, this, [this]() {
        m_devices->apply(withArtwork(m_server.devices()));
        emit devicesChanged();
    });

    connect(&m_server, &StreamServer::artworkMissed, this,
            [this](const QString &key) {
        m_metadata.warmArtworkKey(key);
    });

    m_silence.setSingleShot(true);
    m_silence.setInterval(kDeveloperSilenceMs);
    connect(&m_silence, &QTimer::timeout, this, &StreamingController::developerChanged);

    connect(&m_browser, &DiscoveryBrowser::found, this,
            [this](const QString &serverId, const QString &name, const QString &host,
                   quint16 port) {
        onFound(serverId, name, host, port);
    });

    connect(&m_sync, &MirrorSync::downloading, this, [this](bool firstTime) {
        setConnecting(true, firstTime ? tr("Getting the library from %1").arg(m_targetName)
                                      : tr("Updating the library from %1").arg(m_targetName));
    });
    connect(&m_sync, &MirrorSync::progress, this, [this](qint64 received, qint64 total) {
        m_connectProgress = total > 0 ? double(received) / double(total) : -1.0;
        emit connectProgressChanged();
    });
    connect(&m_sync, &MirrorSync::failed, this, [this](const QString &reason, bool) {
        stopStaying();
        closeSession();
        setConnecting(false, QString());
        setLastError(reason);
    });
    connect(&m_sync, &MirrorSync::ready, this, &StreamingController::onMirrorReady);

    m_expiry.setInterval(StreamProtocol::DiscoveryIntervalMs);
    connect(&m_expiry, &QTimer::timeout, this, &StreamingController::expireServers);

    if (canConnect()) {
        m_stored->setMirrors(Mirror::all(m_sync.root()));
        connect(qApp, &QGuiApplication::applicationStateChanged, this,
                &StreamingController::onApplicationState);
        onApplicationState(QGuiApplication::applicationState());
    }
}

StreamingController::~StreamingController()
{
    stopStaying();
    if (m_serving) {
        m_responder.stop();
        m_server.stop();
        m_system.setKeepSystemAwake(false);
        MM_LOG_I() << "streaming: off as the app closes";
    }
    m_browser.stop();
}

bool StreamingController::canServe() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool StreamingController::canConnect() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

bool StreamingController::serving() const
{
    return m_serving;
}

QString StreamingController::serverName() const
{
    return m_settings.streamingServerName();
}

QStringList StreamingController::addresses() const
{
    return m_addresses;
}

int StreamingController::port() const
{
    return m_server.port();
}

int StreamingController::servedFileCount() const
{
    return m_servedFileCount;
}

FoundServerModel *StreamingController::servers() const
{
    return m_servers;
}

bool StreamingController::looking() const
{
    return m_browser.isRunning();
}

QString StreamingController::lastError() const
{
    return m_lastError;
}

void StreamingController::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void StreamingController::startServing()
{
    if (m_serving || !canServe()) {
        return;
    }

    StreamServerInfo info;
    info.serverId = m_settings.streamingServerId();
    info.name = m_settings.streamingServerName();

    const QString snapshotPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/streaming/snapshot.sqlite");
    QDir().mkpath(QFileInfo(snapshotPath).absolutePath());

    const LibrarySnapshotResult snapshot =
        LibrarySnapshot::build(m_database.filePath(), snapshotPath, info.serverId, info.name);
    if (!snapshot.ok) {
        setLastError(tr("The library could not be prepared for streaming: %1")
                         .arg(snapshot.error));
        return;
    }

    info.schemaVersion = snapshot.schemaVersion;
    info.snapshotPath = snapshot.path;
    info.revision = snapshot.revision;
    info.snapshotBytes = snapshot.bytes;

    m_servedFileCount = int(snapshot.servedFiles.size());

    m_server.setInfo(info);
    m_server.setFiles(snapshot.servedFiles);
    m_server.setAttachedSubtitles(snapshot.attachedSubtitles);
    m_server.setArtworkDirectory(PosterCache::defaultDirectory());
    m_server.setSubtitleLister([](const QString &videoPath) {
        return SubtitleFinder::onDisk(videoPath, [](const QString &name) {
            return MediaFormats::isVideoFile(name);
        });
    });
    if (!m_server.start(StreamProtocol::HttpPort)) {
        m_servedFileCount = 0;
        setLastError(tr("Streaming could not start: %1").arg(m_server.errorString()));
        return;
    }
    if (m_developerSlow) {
        m_server.setDeveloperThrottle(kDeveloperSlowBytesPerSecond);
    }

    Discovery::Here here;
    here.serverId = info.serverId;
    here.name = info.name;
    here.port = m_server.port();
    if (!m_responder.start(here, StreamProtocol::DiscoveryPort)) {
        MM_LOG_W() << "streaming: discovery could not start, devices will not find this PC";
    }

    m_library.setHeldStill(true);
    m_metadata.setHeldForStreaming(true);
    m_system.setKeepSystemAwake(true);

    m_addresses = localAddresses();
    m_serving = true;
    setLastError(QString());
    MM_LOG_I() << "streaming: on as" << info.name << "at" << m_addresses
               << "port" << m_server.port() << "with" << m_servedFileCount << "files";
    emit servingChanged();
}

void StreamingController::stopServing()
{
    if (!m_serving) {
        return;
    }

    m_responder.stop();
    m_server.stop();
    m_system.setKeepSystemAwake(false);
    m_metadata.setHeldForStreaming(false);
    m_library.setHeldStill(false);
    if (m_silence.isActive()) {
        m_silence.stop();
        emit developerChanged();
    }

    m_serving = false;
    m_addresses.clear();
    m_servedFileCount = 0;
    MM_LOG_I() << "streaming: off";
    emit servingChanged();
}

void StreamingController::startLooking()
{
    if (!canConnect() || m_browser.isRunning()) {
        return;
    }
    m_browser.start(StreamProtocol::DiscoveryPort, StreamProtocol::DiscoveryIntervalMs);
    emit lookingChanged();
}

void StreamingController::stopLooking()
{
    if (!m_browser.isRunning()) {
        return;
    }
    m_browser.stop();
    emit lookingChanged();
}

void StreamingController::onFound(const QString &serverId, const QString &name,
                                  const QString &host, quint16 port)
{
    FoundServer server;
    server.serverId = serverId;
    server.name = name.isEmpty() ? host : name;
    server.host = host;
    server.port = port;
    server.lastSeenMs = m_clock.elapsed();

    if (m_servers->upsert(server)) {
        MM_LOG_I() << "streaming: found" << server.name << "at" << host << "port" << port
                   << "on the network";
        emit offerChanged();
    }
    if (m_servers->selectedId().isEmpty()) {
        selectServer(serverId);
    }
}

void StreamingController::expireServers()
{
    const QStringList gone = m_servers->expire(m_clock.elapsed(), kServerGoneMs,
                                               m_connected ? m_targetId : QString());
    if (gone.isEmpty()) {
        return;
    }
    for (const QString &serverId : gone) {
        m_dismissed.remove(serverId);
    }
    MM_LOG_I() << "streaming:" << gone.size() << "PCs stopped streaming or left the network";
    emit selectedServerChanged();
    emit offerChanged();
}

void StreamingController::onApplicationState(Qt::ApplicationState state)
{
    if (!canConnect()) {
        return;
    }
    if (state == Qt::ApplicationActive) {
        startLooking();
        m_expiry.start();
        if (!m_token.isEmpty() && !m_heartbeat.isActive()) {
            MM_LOG_I() << "streaming: back in the app, telling" << m_targetName
                       << "this device is still here";
            dropStaleRequests();
            m_misses = 0;
            m_heartbeat.start();
            sendHeartbeat();
            listenForStop();
        }
    } else if (!m_connecting) {
        stopLooking();
        m_expiry.stop();
        if (m_heartbeat.isActive()) {
            MM_LOG_I() << "streaming: the app is in the background, heartbeats wait";
            m_heartbeat.stop();
            dropStaleRequests();
        }
    }
}

QString StreamingController::offerServerId() const
{
    for (int row = 0; row < m_servers->rowCount(); ++row) {
        const QString serverId =
            m_servers->data(m_servers->index(row, 0), FoundServerModel::ServerIdRole).toString();
        if (!m_dismissed.contains(serverId)) {
            return serverId;
        }
    }
    return QString();
}

bool StreamingController::connected() const
{
    return m_connected;
}

bool StreamingController::connecting() const
{
    return m_connecting;
}

QString StreamingController::connectingText() const
{
    return m_connectingText;
}

double StreamingController::connectProgress() const
{
    return m_connectProgress;
}

QString StreamingController::connectedServerName() const
{
    return m_connected ? m_connectedName : QString();
}

bool StreamingController::offerVisible() const
{
    return canConnect() && !m_connected && !m_connecting && !offerServerId().isEmpty();
}

QString StreamingController::offerServerName() const
{
    const FoundServer *server = m_servers->find(offerServerId());
    return server ? server->name : QString();
}

StoredMirrorModel *StreamingController::storedLibraries() const
{
    return m_stored;
}

void StreamingController::setConnecting(bool connecting, const QString &text)
{
    m_connecting = connecting;
    m_connectingText = text;
    if (!connecting) {
        m_connectProgress = -1.0;
        emit connectProgressChanged();
    }
    emit connectionChanged();
    emit offerChanged();
}

void StreamingController::connectToOffer()
{
    const QString serverId = offerServerId();
    connectToServer(serverId.isEmpty() ? m_servers->firstId() : serverId);
}

void StreamingController::dismissOffer()
{
    const QString serverId = offerServerId();
    if (serverId.isEmpty()) {
        return;
    }
    m_dismissed.insert(serverId);
    MM_LOG_I() << "streaming: the offer to connect was put away for this time";
    emit offerChanged();
}

void StreamingController::connectToServer(const QString &serverId)
{
    if (!canConnect() || m_connected || m_connecting) {
        return;
    }
    const FoundServer *server = m_servers->find(serverId);
    if (!server) {
        setLastError(tr("That PC is no longer streaming"));
        return;
    }
    if (m_setup.active()) {
        setLastError(tr("Let the library setup on this device finish first"));
        return;
    }

    m_targetId = server->serverId;
    m_targetName = server->name;
    m_targetHost = server->host;
    m_targetPort = server->port;
    setLastError(QString());
    setConnecting(true, tr("Connecting to %1").arg(m_targetName));

    MM_LOG_I() << "streaming: connecting to" << m_targetName << "at" << m_targetHost;
    openSession();
}

QString StreamingController::sessionBase() const
{
    return RemoteMediaSource(m_targetHost, m_targetPort, m_token).sessionUrl();
}

QString StreamingController::deviceName()
{
    QString name;
#ifdef Q_OS_ANDROID
    QJniObject context(QNativeInterface::QAndroidApplication::context());
    if (context.isValid()) {
        const QJniObject resolver = context.callObjectMethod(
            "getContentResolver", "()Landroid/content/ContentResolver;");
        if (resolver.isValid()) {
            const QJniObject key = QJniObject::fromString(QStringLiteral("device_name"));
            const QJniObject given = QJniObject::callStaticObjectMethod(
                "android/provider/Settings$Global", "getString",
                "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
                resolver.object(), key.object<jstring>());
            if (given.isValid()) {
                name = given.toString().trimmed();
            }
        }
    }
    if (name.isEmpty()) {
        name = QJniObject::getStaticObjectField("android/os/Build", "MODEL",
                                                "Ljava/lang/String;").toString().trimmed();
    }
#endif
    if (name.isEmpty()) {
        name = QSysInfo::machineHostName();
    }
    return name.isEmpty() ? QStringLiteral("Makimedia") : name;
}

QString StreamingController::deviceForm() const
{
    if (m_system.isTelevision()) {
        return QStringLiteral("television");
    }
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QSize size = screen->size();
        if (qMin(size.width(), size.height()) >= kTabletSmallestSide) {
            return QStringLiteral("tablet");
        }
    }
    return QStringLiteral("phone");
}

void StreamingController::openSession()
{
    QNetworkRequest request(QUrl(RemoteMediaSource(m_targetHost, m_targetPort).baseUrl()
                                 + QStringLiteral("/v1/session")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(kHelloTimeoutMs);

    const QString name = deviceName();
    const QString form = deviceForm();
    const QJsonObject asking{
        { QStringLiteral("device"), m_settings.streamingDeviceId() },
        { QStringLiteral("name"), name },
        { QStringLiteral("form"), form }
    };
    MM_LOG_I() << "streaming: asking" << m_targetName << "for a session as" << name << form;

    QNetworkReply *reply =
        m_network.post(request, QJsonDocument(asking).toJson(QJsonDocument::Compact));
    m_sessionReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply != m_sessionReply || !m_connecting) {
            return;
        }
        m_sessionReply = nullptr;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
        const QString token = answer.value(QStringLiteral("token")).toString();
        if (status != 200 || token.isEmpty()) {
            MM_LOG_W() << "streaming: no session from" << m_targetName << "- status" << status
                       << StreamProtocol::withoutToken(reply->errorString());
            setConnecting(false, QString());
            setLastError(status == 404 || status == 405
                             ? tr("The PC runs a different version of Makimedia")
                             : tr("%1 did not answer").arg(m_targetName));
            return;
        }

        const int spoken = answer.value(QStringLiteral("v")).toInt();
        if (spoken != StreamProtocol::Version) {
            MM_LOG_W() << "streaming:" << m_targetName << "speaks protocol" << spoken
                       << "and this device speaks" << StreamProtocol::Version;
            m_token = token;
            closeSession();
            setConnecting(false, QString());
            setLastError(tr("The PC runs a different version of Makimedia"));
            return;
        }

        m_token = token;
        MM_LOG_I() << "streaming: session with" << m_targetName << "opened";
        startStaying();
        m_sync.start(sessionBase(), m_targetId, m_targetName);
    });
}

void StreamingController::closeSession()
{
    if (m_token.isEmpty()) {
        return;
    }
    QNetworkRequest request(QUrl(sessionBase() + QStringLiteral("/v1/session")));
    request.setTransferTimeout(kGoodbyeTimeoutMs);
    QNetworkReply *reply = m_network.deleteResource(request);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    m_token.clear();
}

void StreamingController::startStaying()
{
    m_misses = 0;
    m_heartbeat.start();
    listenForStop();
}

void StreamingController::dropStaleRequests()
{
    for (QPointer<QNetworkReply> *held : { &m_heartbeatReply, &m_eventsReply }) {
        QNetworkReply *reply = held->data();
        if (!reply) {
            continue;
        }
        MM_LOG_I() << "streaming: dropping a request left over from before the "
                      "app was put away, so the next one can be sent";
        *held = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void StreamingController::stopStaying()
{
    m_heartbeat.stop();
    m_misses = 0;
    for (QPointer<QNetworkReply> *held : { &m_heartbeatReply, &m_eventsReply, &m_sessionReply }) {
        QNetworkReply *reply = held->data();
        *held = nullptr;
        if (reply) {
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
    }
}

void StreamingController::listenForStop()
{
    if (m_token.isEmpty() || m_eventsReply) {
        return;
    }
    QNetworkRequest request(QUrl(sessionBase() + QStringLiteral("/v1/events")));
    request.setTransferTimeout(0);
    QNetworkReply *reply = m_network.get(request);
    m_eventsReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply != m_eventsReply) {
            return;
        }
        m_eventsReply = nullptr;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 503) {
            loseServer(QStringLiteral("it stopped streaming"));
            return;
        }
        MM_LOG_I() << "streaming: the line to" << m_targetName << "broke - status" << status
                   << StreamProtocol::withoutToken(reply->errorString()) << "- checking on it";
        sendHeartbeat();
    });
}

void StreamingController::sendHeartbeat()
{
    if (m_token.isEmpty() || m_heartbeatReply) {
        return;
    }

    QJsonObject beat;
    const bool playing = m_player && !m_playingTitle.isEmpty()
                         && (m_player->fileLoaded() || m_player->loading());
    if (playing) {
        beat.insert(QStringLiteral("title"), m_playingTitle);
        beat.insert(QStringLiteral("file"), double(m_playingFileId));
        beat.insert(QStringLiteral("position"), m_player->position());
        beat.insert(QStringLiteral("duration"), m_player->duration());
        beat.insert(QStringLiteral("paused"), m_player->paused());
    }

    QNetworkRequest request(QUrl(sessionBase() + QStringLiteral("/v1/heartbeat")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(StreamProtocol::HeartbeatTimeoutMs);
    QNetworkReply *reply = m_network.post(request, QJsonDocument(beat).toJson(QJsonDocument::Compact));
    m_heartbeatReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHeartbeatFinished(reply);
    });
}

void StreamingController::onHeartbeatFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply != m_heartbeatReply) {
        return;
    }
    m_heartbeatReply = nullptr;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 200) {
        if (m_misses > 0) {
            MM_LOG_I() << "streaming:" << m_targetName << "answered again after" << m_misses
                       << "missed heartbeats";
        }
        m_misses = 0;
        listenForStop();
        return;
    }
    if (status == 401) {
        loseServer(QStringLiteral("it no longer knows this device"));
        return;
    }
    if (status == 503 || reply->error() == QNetworkReply::ConnectionRefusedError) {
        loseServer(QStringLiteral("it stopped streaming"));
        return;
    }

    ++m_misses;
    MM_LOG_W() << "streaming: heartbeat" << m_misses << "of"
               << StreamProtocol::HeartbeatMissesAllowed << "missed - status" << status
               << StreamProtocol::withoutToken(reply->errorString());
    if (m_misses >= StreamProtocol::HeartbeatMissesAllowed) {
        loseServer(QStringLiteral("it did not answer %1 heartbeats in a row").arg(m_misses));
    }
}

void StreamingController::loseServer(const QString &why)
{
    if (!m_lostName.isEmpty() || m_token.isEmpty()) {
        return;
    }
    MM_LOG_W() << "streaming: lost" << m_targetName << "-" << why;
    stopStaying();
    m_token.clear();

    if (!m_connected) {
        if (m_connecting) {
            m_sync.cancel();
            setConnecting(false, QString());
        }
        setLastError(tr("Lost %1").arg(m_targetName));
        return;
    }

    const bool playing = m_player && !m_playingTitle.isEmpty()
                         && (m_player->fileLoaded() || m_player->loading());
    if (playing) {
        m_player->setPaused(true);
        m_lostName = m_connectedName;
        MM_LOG_I() << "streaming: the player stops on" << m_playingTitle
                   << "and waits to be dismissed";
        emit lostChanged();
        return;
    }
    finishLosing();
}

void StreamingController::finishLosing()
{
    const QString name = m_connectedName;
    disconnectFromServer();
    setLastError(tr("Lost %1").arg(name));
}

void StreamingController::acknowledgeLost()
{
    if (m_lostName.isEmpty()) {
        return;
    }
    MM_LOG_I() << "streaming: the loss of" << m_lostName << "was seen, back to this device";
    m_lostName.clear();
    emit lostChanged();
    disconnectFromServer();
}

QString StreamingController::lostServerName() const
{
    return m_lostName;
}

void StreamingController::setPlayer(MpvController *player)
{
    m_player = player;
}

QString StreamingController::playingTitle() const
{
    return m_playingTitle;
}

void StreamingController::setPlayingTitle(const QString &title)
{
    if (m_playingTitle == title) {
        return;
    }
    m_playingTitle = title;
    emit playingTitleChanged();
    if (!m_token.isEmpty()) {
        sendHeartbeat();
    }
}

QString StreamingController::playingHandle() const
{
    return m_playingHandle;
}

void StreamingController::setPlayingHandle(const QString &handle)
{
    if (m_playingHandle == handle) {
        return;
    }
    m_playingHandle = handle;
    m_playingFileId = handle.isEmpty() || !m_connected
        ? -1
        : FileRepository(m_database).byHandle(handle).id;
    emit playingTitleChanged();
    if (!m_token.isEmpty()) {
        sendHeartbeat();
    }
}

ConnectedDeviceModel *StreamingController::devices() const
{
    return m_devices;
}

QList<StreamDevice> StreamingController::withArtwork(QList<StreamDevice> devices) const
{
    MediaRepository media(m_database);
    for (StreamDevice &device : devices) {
        if (!device.watching()) {
            continue;
        }
        if (device.fileId <= 0) {
            MM_LOG_D() << "stream:" << device.name << "says what it plays but not which file,"
                       << "so the tile has no artwork - an older build of the app";
            continue;
        }
        const MediaRecord record = media.mediaForFile(device.fileId);
        if (!record.isValid()) {
            MM_LOG_D() << "stream: file" << device.fileId << "on" << device.name
                       << "belongs to no title here, so the tile has no artwork";
            continue;
        }
        device.posterPath = record.posterPath;
        device.backdropPath = record.backdropPath;
        if (record.posterPath.isEmpty() && record.backdropPath.isEmpty()) {
            MM_LOG_D() << "stream:" << record.title << "has no artwork for" << device.name;
        }
    }
    return devices;
}

int StreamingController::watchingCount() const
{
    return m_devices->watchingCount();
}

bool StreamingController::developerSilent() const
{
    return m_silence.isActive();
}

void StreamingController::developerGoSilent()
{
    if (!m_serving) {
        return;
    }
    m_server.setDeveloperSilence(kDeveloperSilenceMs);
    m_silence.start();
    emit developerChanged();
}

bool StreamingController::developerSlowNetwork() const
{
    return m_developerSlow;
}

void StreamingController::setDeveloperSlowNetwork(bool slow)
{
    if (m_developerSlow == slow) {
        return;
    }
    m_developerSlow = slow;
    m_server.setDeveloperThrottle(slow ? kDeveloperSlowBytesPerSecond : 0);
    emit developerChanged();
}

void StreamingController::cancelConnect()
{
    if (!m_connecting) {
        return;
    }
    m_sync.cancel();
    stopStaying();
    closeSession();
    setConnecting(false, QString());
    MM_LOG_I() << "streaming: connecting was cancelled";
}

void StreamingController::onMirrorReady(const QString &libraryPath, bool changed)
{
    Mirror::Stored stored = Mirror::readInfo(m_sync.root(), m_targetId);
    stored.serverId = m_targetId;
    stored.bytes = QFileInfo(libraryPath).size();
    m_stored->upsert(stored);

    MM_LOG_I() << "streaming: the library of" << m_targetName
               << (changed ? "was brought up to date" : "was already up to date");
    setConnecting(true, tr("Opening the library from %1").arg(m_targetName));
    holdLocalWork();
    switchWhenQuiet(libraryPath, 0);
}

void StreamingController::holdLocalWork()
{
    m_library.setHeldStill(true);
    m_metadata.setHeldForStreaming(true);
    m_metadata.forgetLibraryState();
}

void StreamingController::releaseLocalWork()
{
    m_library.setHeldStill(false);
    m_metadata.setHeldForStreaming(false);
}

void StreamingController::switchWhenQuiet(const QString &libraryPath, int attempt)
{
    if (!m_connecting || m_token.isEmpty()) {
        releaseLocalWork();
        return;
    }

    if (m_library.scanning() || m_setup.active()) {
        if (attempt >= kQuietWaitSteps) {
            MM_LOG_W() << "streaming: this device's library would not settle, not switching";
            releaseLocalWork();
            stopStaying();
            closeSession();
            setConnecting(false, QString());
            setLastError(tr("This device is still busy with its own library. Try again in a moment"));
            return;
        }
        if (attempt == 0) {
            MM_LOG_I() << "streaming: waiting for this device's own scan to stop before switching";
        }
        QTimer::singleShot(kQuietWaitStepMs, this, [this, libraryPath, attempt]() {
            switchWhenQuiet(libraryPath, attempt + 1);
        });
        return;
    }

    m_remoteSource.reset(new RemoteMediaSource(m_targetHost, m_targetPort, m_token));
    Database *database = &m_database;
    m_remoteSource->setIdResolver([database](const QString &handle) {
        return FileRepository(*database).byHandle(handle).id;
    });
    m_platform.setMediaSourceOverride(m_remoteSource.data());

    if (!openLibrary(libraryPath)) {
        m_platform.setMediaSourceOverride(nullptr);
        m_remoteSource.reset();
        openLibrary(QString());
        releaseLocalWork();
        stopStaying();
        closeSession();
        setConnecting(false, QString());
        setLastError(tr("The library from %1 could not be opened").arg(m_targetName));
        return;
    }

    m_library.setRemote(true);
    m_library.reloadAfterSwitch();
    m_metadata.setArtworkServer(m_remoteSource->sessionUrl());
    m_metadata.readLibraryAfterSwitch();

    m_connected = true;
    m_connectedName = m_targetName;
    setConnecting(false, QString());
    MM_LOG_I() << "streaming: watching the library of" << m_connectedName;
}

bool StreamingController::openLibrary(const QString &path)
{
    m_database.close();
    if (m_database.open(path)) {
        return true;
    }
    MM_LOG_E() << "streaming: could not open" << (path.isEmpty() ? QStringLiteral("this device's library") : path);
    return false;
}

void StreamingController::disconnectFromServer()
{
    if (!m_connected) {
        return;
    }

    MM_LOG_I() << "streaming: leaving the library of" << m_connectedName;
    stopStaying();
    closeSession();
    if (!m_lostName.isEmpty()) {
        m_lostName.clear();
        emit lostChanged();
    }
    m_metadata.forgetLibraryState();

    if (!openLibrary(QString())) {
        MM_LOG_E() << "streaming: this device's own library could not be opened again";
    }
    m_platform.setMediaSourceOverride(nullptr);
    m_remoteSource.reset();

    m_library.setRemote(false);
    m_library.reloadAfterSwitch();
    m_metadata.setArtworkServer(QString());
    m_metadata.readLibraryAfterSwitch();
    releaseLocalWork();

    m_connected = false;
    m_connectedName.clear();
    emit connectionChanged();
    emit offerChanged();
}

bool StreamingController::pretendNewerLibrary() const
{
    return m_sync.pretendNewerLibrary();
}

void StreamingController::setPretendNewerLibrary(bool pretend)
{
    if (m_sync.pretendNewerLibrary() == pretend) {
        return;
    }
    m_sync.setPretendNewerLibrary(pretend);
    emit pretendNewerLibraryChanged();
}

void StreamingController::forgetStoredRevisions()
{
    const QList<Mirror::Stored> stored = Mirror::all(m_sync.root());
    for (Mirror::Stored mirror : stored) {
        mirror.revision.clear();
        Mirror::writeInfo(m_sync.root(), mirror);
    }
    MM_LOG_I() << "streaming: developer - the next connect to" << stored.size()
               << "PCs downloads their library again";
}

void StreamingController::forgetStoredLibrary(const QString &serverId)
{
    if (m_connected && serverId == m_targetId) {
        setLastError(tr("Disconnect before forgetting this library"));
        return;
    }
    Mirror::forget(m_sync.root(), serverId);
    m_stored->remove(serverId);
}

void StreamingController::selectServer(const QString &serverId)
{
    const FoundServer *server = m_servers->find(serverId);
    if (!server || m_servers->selectedId() == serverId) {
        return;
    }
    m_servers->setSelected(serverId);
    MM_LOG_I() << "streaming: the PC to play from is" << server->name << "at" << server->host;
    emit selectedServerChanged();
}

QString StreamingController::remoteFileUrl(qint64 fileId)
{
    if (!m_connected || m_token.isEmpty()) {
        setLastError(tr("Connect to the PC first"));
        return QString();
    }
    if (fileId <= 0) {
        return QString();
    }
    return RemoteMediaSource(m_targetHost, m_targetPort, m_token).urlForFileId(fileId);
}

QStringList StreamingController::localAddresses()
{
    QStringList found;
    const QList<QHostAddress> all = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : all) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback()
            || !AddressFilter::isPrivatePeer(address) || address.isLinkLocal()) {
            continue;
        }
        found.append(address.toString());
    }
    std::sort(found.begin(), found.end());
    return found;
}
