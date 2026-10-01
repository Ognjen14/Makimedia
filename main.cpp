#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QTimer>

#include "AppSettings.h"
#include "Data/Database.h"
#include "Library/BrowseListModel.h"
#include "Library/CollectionListModel.h"
#include "Library/FileListModel.h"
#include "Library/FolderListModel.h"
#include "Library/GenreRowModel.h"
#include "Library/MediaListModel.h"
#include "Library/ShowEpisodeModel.h"
#include "Library/UnmatchedShowModel.h"
#include "Library/RemovedFileModel.h"
#include "Streaming/ConnectedDeviceModel.h"
#include "Streaming/FoundServerModel.h"
#include "Streaming/StoredMirrorModel.h"
#include "Streaming/StreamingController.h"
#include "Subtitles/SubtitleSearch.h"
#include "Library/LibraryController.h"
#include "Library/LibrarySetup.h"
#include "MmLog.h"
#include "Metadata/MetadataService.h"
#include "Player/SeekPreviewService.h"
#include "Player/MpvController.h"
#include "Player/TrackListModel.h"
#include "Platform/MediaFileInfo.h"
#if defined(Q_OS_ANDROID)
#  include "Platform/Android/AndroidVideoSurface.h"
#endif
#include "Platform/PlatformServices.h"
#include "Player/MpvRenderItem.h"
#include "SingleInstance.h"
#include "SystemBridge.h"

namespace {

void registerBundledFonts()
{
    const QStringList files = {
        QStringLiteral(":/assets/fonts/Roboto-Regular.ttf"),
        QStringLiteral(":/assets/fonts/Roboto-Medium.ttf"),
        QStringLiteral(":/assets/fonts/Roboto-SemiBold.ttf"),
        QStringLiteral(":/assets/fonts/Roboto-Bold.ttf"),
        QStringLiteral(":/assets/fonts/RobotoMono-Regular.ttf")
    };

    for (const QString &file : files) {
        const int id = QFontDatabase::addApplicationFont(file);
        if (id < 0) {
            MM_LOG_E() << "bundled font failed to load" << file;
            continue;
        }
        MM_LOG_D() << "bundled font loaded" << file << "as"
                   << QFontDatabase::applicationFontFamilies(id);
    }
}

}

int main(int argc, char *argv[])
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);

    app.setOrganizationName(QStringLiteral("TopicDev"));
    app.setApplicationName(QStringLiteral("Makimedia"));
    app.setApplicationVersion(QStringLiteral("1.0.2"));

#ifdef Q_OS_WIN
    app.setQuitOnLastWindowClosed(false);
#endif

    MmLog::install(true, QString());
    MM_LOG_I() << "Makimedia" << app.applicationVersion() << "starting";
    MM_LOG_I() << "log file:" << MmLog::logFilePath() << "- keeping"
               << (mmLogCat().isDebugEnabled() ? "every line, debug included"
                                               : "info, warnings and errors only");

    registerBundledFonts();
    app.setFont(QFont(QStringLiteral("Roboto")));

    static AppSettings appSettings;
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0, "AppSettings", &appSettings);
    qmlRegisterType<MpvRenderItem>("com.topicdev.makimedia", 1, 0, "MpvRenderItem");

    static SubtitleSearch subtitleSearch(appSettings);
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0,
                                 "SubtitleSearch", &subtitleSearch);

    static SystemBridge systemBridge;
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0, "System", &systemBridge);

#if defined(Q_OS_ANDROID)
    if (SystemBridge::runningOnTelevision()) {
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        format.setAlphaBufferSize(8);
        QSurfaceFormat::setDefaultFormat(format);

        AndroidVideoSurface::instance()->create();
    }
#endif

    if (!appSettings.accentChosen() && systemBridge.isTelevision()) {
        appSettings.setAccentIndex(11);
    }

    if (systemBridge.canRegisterFileTypes() && !systemBridge.fileTypesRegistered()) {
        systemBridge.setFileTypesRegistered(true);
    }

    Database database;
    if (!database.open()) {
        MM_LOG_E() << "library database unavailable, library features are disabled";
    }

    PlatformServices platformServices;
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0, "Platform", &platformServices);

    qmlRegisterUncreatableType<TrackListModel>(
        "com.topicdev.makimedia", 1, 0, "TrackListModel",
        QStringLiteral("TrackListModel comes from MpvPlayer"));

    MpvController mpvController;
    mpvController.setAudioFocus(platformServices.audioFocus());
    mpvController.setTransportControls(platformServices.transportControls());
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0, "MpvPlayer", &mpvController);

    qmlRegisterUncreatableType<FolderListModel>(
        "com.topicdev.makimedia", 1, 0, "FolderListModel",
        QStringLiteral("FolderListModel comes from LibraryController"));
    qmlRegisterUncreatableType<FileListModel>(
        "com.topicdev.makimedia", 1, 0, "FileListModel",
        QStringLiteral("FileListModel comes from LibraryController"));
    qmlRegisterUncreatableType<BrowseListModel>(
        "com.topicdev.makimedia", 1, 0, "BrowseListModel",
        QStringLiteral("BrowseListModel comes from LibraryController"));
    qmlRegisterUncreatableType<MediaListModel>(
        "com.topicdev.makimedia", 1, 0, "MediaListModel",
        QStringLiteral("MediaListModel comes from LibraryController"));
    qmlRegisterUncreatableType<CollectionListModel>(
        "com.topicdev.makimedia", 1, 0, "CollectionListModel",
        QStringLiteral("CollectionListModel comes from LibraryController"));
    qmlRegisterUncreatableType<CollectionPageModel>(
        "com.topicdev.makimedia", 1, 0, "CollectionPageModel",
        QStringLiteral("CollectionPageModel comes from LibraryController"));
    qmlRegisterUncreatableType<GenreRowModel>(
        "com.topicdev.makimedia", 1, 0, "GenreRowModel",
        QStringLiteral("GenreRowModel comes from LibraryController"));
    qmlRegisterUncreatableType<UnmatchedShowModel>(
        "com.topicdev.makimedia", 1, 0, "UnmatchedShowModel",
        QStringLiteral("UnmatchedShowModel comes from LibraryController"));
    qmlRegisterUncreatableType<RemovedFileModel>(
        "com.topicdev.makimedia", 1, 0, "RemovedFileModel",
        QStringLiteral("RemovedFileModel comes from LibraryController"));
    qmlRegisterUncreatableType<ShowEpisodeModel>(
        "com.topicdev.makimedia", 1, 0, "ShowEpisodeModel",
        QStringLiteral("ShowEpisodeModel comes from LibraryController"));

    LibraryController libraryController(database, platformServices);
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0, "Library", &libraryController);

    MetadataService metadataService(appSettings, database);
    metadataService.setUniverses(libraryController.universes());
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0,
                                 "Metadata", &metadataService);
    QObject::connect(&metadataService, &MetadataService::libraryChanged,
                     &libraryController, &LibraryController::applyLibraryChange);
    QObject::connect(&metadataService, &MetadataService::artworkLanded,
                     &libraryController, &LibraryController::applyArtworkLanded);
    QObject::connect(&metadataService, &MetadataService::collectionsChanged,
                     &libraryController, &LibraryController::refreshCollections);

    libraryController.setThumbnailsEnabled(!systemBridge.isTelevision());
    libraryController.setProbesEnabled(!systemBridge.isTelevision());

    LibrarySetup librarySetup(libraryController, metadataService, systemBridge);
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0, "Setup", &librarySetup);

    qmlRegisterUncreatableType<FoundServerModel>(
        "com.topicdev.makimedia", 1, 0, "FoundServerModel",
        QStringLiteral("FoundServerModel comes from Streaming"));
    qmlRegisterUncreatableType<StoredMirrorModel>(
        "com.topicdev.makimedia", 1, 0, "StoredMirrorModel",
        QStringLiteral("StoredMirrorModel comes from Streaming"));
    qmlRegisterUncreatableType<ConnectedDeviceModel>(
        "com.topicdev.makimedia", 1, 0, "ConnectedDeviceModel",
        QStringLiteral("ConnectedDeviceModel comes from Streaming"));
    StreamingController streaming(appSettings, database, libraryController,
                                  metadataService, systemBridge, platformServices,
                                  librarySetup);
    streaming.setPlayer(&mpvController);
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0, "Streaming", &streaming);

    QObject::connect(&streaming, &StreamingController::connectionChanged, &libraryController,
                     [&libraryController, &streaming]() {
        const bool local = !streaming.connected() && !systemBridge.isTelevision();
        libraryController.setThumbnailsEnabled(local);
        libraryController.setProbesEnabled(local);
    });

    QTimer thumbnailGate;
    thumbnailGate.setSingleShot(true);

    QObject::connect(&thumbnailGate, &QTimer::timeout, &libraryController,
                     [&libraryController, &metadataService, &librarySetup, &streaming]() {
        const bool busy = librarySetup.active()
                          || streaming.serving()
                          || streaming.connected()
                          || (metadataService.available()
                              && (libraryController.scanning()
                                  || metadataService.pending() > 0));
        libraryController.setThumbnailsDeferred(busy);
        libraryController.setProbesDeferred(busy);
        libraryController.setMatchingBusy(busy);
    });

    QObject::connect(&metadataService, &MetadataService::pendingChanged,
                     &thumbnailGate, [&thumbnailGate]() {
        thumbnailGate.start(750);
    });
    QObject::connect(&libraryController, &LibraryController::scanningChanged,
                     &thumbnailGate, [&thumbnailGate]() {
        thumbnailGate.start(750);
    });
    QObject::connect(&librarySetup, &LibrarySetup::activeChanged,
                     &thumbnailGate, [&thumbnailGate, &librarySetup]() {
        thumbnailGate.start(librarySetup.active() ? 0 : 750);
    });
    QObject::connect(&streaming, &StreamingController::servingChanged,
                     &thumbnailGate, [&thumbnailGate, &streaming]() {
        thumbnailGate.start(streaming.serving() ? 0 : 750);
    });
    QObject::connect(&streaming, &StreamingController::connectionChanged,
                     &thumbnailGate, [&thumbnailGate, &streaming]() {
        thumbnailGate.start(streaming.connected() ? 0 : 750);
    });

    libraryController.setThumbnailsDeferred(metadataService.available());
    libraryController.setProbesDeferred(metadataService.available());
    thumbnailGate.start(5000);

    libraryController.probeUnprobed();

    SeekPreviewService seekPreview;
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0,
                                 "SeekPreview", &seekPreview);

    QString launchPath;
    {
        const QStringList args = app.arguments();
        for (int i = 1; i < args.size(); ++i) {
            const QString candidate = args.at(i);
            if (candidate.startsWith(QLatin1Char('-'))) {
                continue;
            }
            const QString path = libraryController.handleFromUrl(candidate);
            if (!MediaFormats::looksLikeVideoFile(path)) {
                MM_LOG_W() << "ignoring command line argument, not a video file"
                           << candidate;
                continue;
            }
            if (!QFileInfo::exists(path)) {
                MM_LOG_W() << "ignoring command line argument, file not found"
                           << path;
                continue;
            }
            launchPath = path;
            break;
        }
    }

    SingleInstance instance(QStringLiteral("makimedia-single-instance"));
    if (!instance.isPrimary()) {
        instance.handOver(launchPath);
        MM_LOG_I() << "Makimedia exiting, handed over to the running instance";
        return 0;
    }

    libraryController.setLaunchPath(launchPath);
    qmlRegisterSingletonInstance("com.topicdev.makimedia", 1, 0,
                                 "AppInstance", &instance);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("makimedia", "Main");

    const int rc = QGuiApplication::exec();
    MM_LOG_I() << "Makimedia exiting with" << rc;
    return rc;
}
