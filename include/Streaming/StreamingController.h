#pragma once

#include "Streaming/ConnectedDeviceModel.h"
#include "Streaming/Discovery.h"
#include "Streaming/FoundServerModel.h"
#include "Streaming/MirrorSync.h"
#include "Streaming/RemoteMediaSource.h"
#include "Streaming/StoredMirrorModel.h"
#include "Streaming/StreamServer.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QScopedPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class AppSettings;
class Database;
class LibraryController;
class LibrarySetup;
class MetadataService;
class MpvController;
class PlatformServices;
class SystemBridge;

class StreamingController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool canServe READ canServe CONSTANT FINAL)
    Q_PROPERTY(bool canConnect READ canConnect CONSTANT FINAL)

    Q_PROPERTY(bool serving READ serving NOTIFY servingChanged FINAL)
    Q_PROPERTY(QString serverName READ serverName CONSTANT FINAL)
    Q_PROPERTY(QStringList addresses READ addresses NOTIFY servingChanged FINAL)
    Q_PROPERTY(int port READ port NOTIFY servingChanged FINAL)
    Q_PROPERTY(int servedFileCount READ servedFileCount NOTIFY servingChanged FINAL)

    Q_PROPERTY(FoundServerModel *servers READ servers CONSTANT FINAL)
    Q_PROPERTY(bool looking READ looking NOTIFY lookingChanged FINAL)

    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged FINAL)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectionChanged FINAL)
    Q_PROPERTY(QString connectingText READ connectingText NOTIFY connectionChanged FINAL)
    Q_PROPERTY(double connectProgress READ connectProgress NOTIFY connectProgressChanged FINAL)
    Q_PROPERTY(QString connectedServerName READ connectedServerName NOTIFY connectionChanged FINAL)
    Q_PROPERTY(bool offerVisible READ offerVisible NOTIFY offerChanged FINAL)
    Q_PROPERTY(QString offerServerName READ offerServerName NOTIFY offerChanged FINAL)
    Q_PROPERTY(StoredMirrorModel *storedLibraries READ storedLibraries CONSTANT FINAL)
    Q_PROPERTY(bool pretendNewerLibrary READ pretendNewerLibrary WRITE setPretendNewerLibrary NOTIFY pretendNewerLibraryChanged FINAL)

    Q_PROPERTY(ConnectedDeviceModel *devices READ devices CONSTANT FINAL)
    Q_PROPERTY(int watchingCount READ watchingCount NOTIFY devicesChanged FINAL)
    Q_PROPERTY(QString lostServerName READ lostServerName NOTIFY lostChanged FINAL)
    Q_PROPERTY(QString playingTitle READ playingTitle WRITE setPlayingTitle NOTIFY playingTitleChanged FINAL)
    Q_PROPERTY(QString playingHandle READ playingHandle WRITE setPlayingHandle NOTIFY playingTitleChanged FINAL)
    Q_PROPERTY(bool developerSilent READ developerSilent NOTIFY developerChanged FINAL)
    Q_PROPERTY(bool developerSlowNetwork READ developerSlowNetwork WRITE setDeveloperSlowNetwork NOTIFY developerChanged FINAL)

public:
    StreamingController(AppSettings &settings, Database &database,
                        LibraryController &library, MetadataService &metadata,
                        SystemBridge &system, PlatformServices &platform,
                        LibrarySetup &setup, QObject *parent = nullptr);
    ~StreamingController() override;

    bool canServe() const;
    bool canConnect() const;

    bool serving() const;
    QString serverName() const;
    QStringList addresses() const;
    int port() const;
    int servedFileCount() const;

    FoundServerModel *servers() const;
    bool looking() const;

    QString lastError() const;

    bool connected() const;
    bool connecting() const;
    QString connectingText() const;
    double connectProgress() const;
    QString connectedServerName() const;
    bool offerVisible() const;
    QString offerServerName() const;
    StoredMirrorModel *storedLibraries() const;

    Q_INVOKABLE void connectToServer(const QString &serverId);
    Q_INVOKABLE void connectToOffer();
    Q_INVOKABLE void dismissOffer();
    Q_INVOKABLE void cancelConnect();
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void forgetStoredLibrary(const QString &serverId);

    bool pretendNewerLibrary() const;
    void setPretendNewerLibrary(bool pretend);
    Q_INVOKABLE void forgetStoredRevisions();

    Q_INVOKABLE void startServing();
    Q_INVOKABLE void stopServing();

    Q_INVOKABLE void startLooking();
    Q_INVOKABLE void stopLooking();
    Q_INVOKABLE void selectServer(const QString &serverId);
    Q_INVOKABLE QString remoteFileUrl(qint64 fileId);

    void setPlayer(MpvController *player);

    ConnectedDeviceModel *devices() const;
    int watchingCount() const;
    QString lostServerName() const;
    Q_INVOKABLE void acknowledgeLost();

    QString playingTitle() const;
    void setPlayingTitle(const QString &title);
    QString playingHandle() const;
    void setPlayingHandle(const QString &handle);

    bool developerSilent() const;
    Q_INVOKABLE void developerGoSilent();
    bool developerSlowNetwork() const;
    void setDeveloperSlowNetwork(bool slow);

signals:
    void devicesChanged();
    void lostChanged();
    void playingTitleChanged();
    void developerChanged();
    void servingChanged();
    void lookingChanged();
    void selectedServerChanged();
    void lastErrorChanged();
    void connectionChanged();
    void connectProgressChanged();
    void offerChanged();
    void pretendNewerLibraryChanged();

private:
    void setLastError(const QString &error);
    void onFound(const QString &serverId, const QString &name, const QString &host,
                 quint16 port);
    QList<StreamDevice> withArtwork(QList<StreamDevice> devices) const;
    void expireServers();
    void onApplicationState(Qt::ApplicationState state);
    QString offerServerId() const;
    void onMirrorReady(const QString &libraryPath, bool changed);
    void switchWhenQuiet(const QString &libraryPath, int attempt);
    bool openLibrary(const QString &path);
    void holdLocalWork();
    void releaseLocalWork();
    void setConnecting(bool connecting, const QString &text);
    static QStringList localAddresses();

    void openSession();
    void closeSession();
    void startStaying();
    void stopStaying();
    void dropStaleRequests();
    void sendHeartbeat();
    void listenForStop();
    void onHeartbeatFinished(QNetworkReply *reply);
    void loseServer(const QString &why);
    void finishLosing();
    QString sessionBase() const;
    static QString deviceName();
    QString deviceForm() const;

    AppSettings &m_settings;
    Database &m_database;
    LibraryController &m_library;
    MetadataService &m_metadata;
    SystemBridge &m_system;
    PlatformServices &m_platform;
    LibrarySetup &m_setup;

    StreamServer m_server;
    DiscoveryResponder m_responder;
    DiscoveryBrowser m_browser;
    FoundServerModel *m_servers = nullptr;
    QNetworkAccessManager m_network;

    MirrorSync m_sync;
    StoredMirrorModel *m_stored = nullptr;
    QScopedPointer<RemoteMediaSource> m_remoteSource;
    QTimer m_expiry;
    QElapsedTimer m_clock;
    QSet<QString> m_dismissed;

    bool m_connected = false;
    bool m_connecting = false;
    QString m_connectingText;
    double m_connectProgress = -1.0;
    QString m_targetId;
    QString m_targetName;
    QString m_targetHost;
    quint16 m_targetPort = 0;
    QString m_connectedName;

    QString m_token;
    QTimer m_heartbeat;
    int m_misses = 0;
    QPointer<QNetworkReply> m_sessionReply;
    QPointer<QNetworkReply> m_heartbeatReply;
    QPointer<QNetworkReply> m_eventsReply;
    QString m_lostName;
    MpvController *m_player = nullptr;
    QString m_playingTitle;
    QString m_playingHandle;
    qint64 m_playingFileId = -1;

    ConnectedDeviceModel *m_devices = nullptr;
    QTimer m_silence;
    bool m_developerSlow = false;

    bool m_serving = false;
    QStringList m_addresses;
    int m_servedFileCount = 0;
    QString m_lastError;
};
