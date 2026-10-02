#include "SystemBridge.h"

#include "AppSettings.h"
#include "MmLog.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QInputDevice>
#include <QList>
#include <QScreen>
#include <QStyleHints>
#include <QWindow>

#ifdef Q_OS_ANDROID
#  include "Platform/Android/AndroidMediaSource.h"

#  include <QCoreApplication>
#  include <QJniObject>
#  include <QMetaObject>
#endif

#ifdef Q_OS_WIN
#  include "Platform/Windows/WindowsFileTypes.h"
#  include "Platform/Windows/WindowsMiniPlayer.h"
#  include "Platform/Windows/WindowsTrayIcon.h"

#  include <windows.h>
#  include <shlobj.h>
#endif

namespace {
#ifdef Q_OS_ANDROID
constexpr const char *kActivityClass = "com/topicdev/makimedia/org/MakimediaActivity";
SystemBridge *g_systemBridge = nullptr;
#endif
}

SystemBridge::SystemBridge(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_ANDROID
    g_systemBridge = this;
#endif

#ifdef Q_OS_ANDROID
    m_hasTouchInput = true;
    MM_LOG_I() << "input model: touch";
#else
    const QList<const QInputDevice *> devices = QInputDevice::devices();
    for (const QInputDevice *device : devices) {
        if (device->type() == QInputDevice::DeviceType::TouchScreen) {
            m_hasTouchInput = true;
            break;
        }
    }
    MM_LOG_I() << "input devices:" << devices.size()
               << (m_hasTouchInput ? "touchscreen present" : "no touchscreen");
#endif

    m_lastVideoAccess = videoAccessState();
    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) {
            return;
        }
        const int access = videoAccessState();
        if (access == m_lastVideoAccess) {
            return;
        }
        MM_LOG_I() << "video access changed while the app was away:"
                   << m_lastVideoAccess << "->" << access;
        m_lastVideoAccess = access;
        emit videoPermissionChanged();
    });
}

int SystemBridge::videoAccessState() const
{
    return (hasVideoPermission() ? 1 : 0) | (videoAccessPartial() ? 2 : 0);
}

SystemBridge::~SystemBridge()
{
    if (m_keepScreenOn) {
        setKeepScreenOn(false);
    }
    if (m_keepSystemAwake) {
        setKeepSystemAwake(false);
    }
#ifdef Q_OS_ANDROID
    if (g_systemBridge == this) {
        g_systemBridge = nullptr;
    }
#endif
}

bool SystemBridge::usesSystemBackNavigation() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

bool SystemBridge::isTelevision() const
{
    return runningOnTelevision();
}

bool SystemBridge::runningOnTelevision()
{
#ifdef Q_OS_ANDROID
    static const bool television = []() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) {
            return false;
        }

        QJniObject manager = activity.callObjectMethod(
            "getPackageManager", "()Landroid/content/pm/PackageManager;");
        if (!manager.isValid()) {
            return false;
        }

        QJniObject name = QJniObject::fromString(
            QStringLiteral("android.software.leanback"));
        const bool leanback = manager.callMethod<jboolean>(
            "hasSystemFeature", "(Ljava/lang/String;)Z", name.object<jstring>());
        const bool forced = AppSettings::treatAsTelevisionAtLaunch();

        MM_LOG_I() << "running on a television:" << (leanback || forced)
                   << "- leanback" << leanback
                   << "- treated as one by the developer setting" << forced;
        return leanback || forced;
    }();
    return television;
#else
    return false;
#endif
}

bool SystemBridge::canSimulateTelevision() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

bool SystemBridge::pausesPlaybackInBackground() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

bool SystemBridge::hasTouchInput() const
{
    return m_hasTouchInput;
}

bool SystemBridge::usesWindowGeometry() const
{
#ifdef Q_OS_ANDROID
    return false;
#else
    return true;
#endif
}

bool SystemBridge::canRegisterFileTypes() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool SystemBridge::fileTypesRegistered() const
{
#ifdef Q_OS_WIN
    return WindowsFileTypes::isRegistered();
#else
    return false;
#endif
}

void SystemBridge::setFileTypesRegistered(bool registered)
{
#ifdef Q_OS_WIN
    const bool ok = registered ? WindowsFileTypes::registerTypes()
                               : WindowsFileTypes::unregisterTypes();
    if (ok) {
        MM_LOG_I() << "Makimedia is" << (registered ? "in" : "out of")
                   << "the Open with menu";
        emit fileTypesRegisteredChanged();
    }
#else
    Q_UNUSED(registered)
    MM_LOG_W() << "file type registration is not available on this platform";
#endif
}

bool SystemBridge::needsVideoPermission() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

bool SystemBridge::hasVideoPermission() const
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>(kActivityClass,
                                                  "hasVideoPermission",
                                                  "()Z");
#else
    return true;
#endif
}

void SystemBridge::requestVideoPermission()
{
#ifdef Q_OS_ANDROID
    MM_LOG_I() << "requesting the video storage permission";
    m_settings.setValue(QStringLiteral("videoPermissionAsked"), true);
    QJniObject::callStaticMethod<void>(kActivityClass,
                                       "requestVideoPermission",
                                       "()V");
#endif
}

bool SystemBridge::videoAccessPartial() const
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>(kActivityClass,
                                                  "hasPartialVideoAccess",
                                                  "()Z");
#else
    return false;
#endif
}

bool SystemBridge::videoPartialAccessAccepted() const
{
    return videoAccessPartial()
        && m_settings.value(QStringLiteral("videoPartialAccessAccepted"), false).toBool();
}

void SystemBridge::acceptPartialVideoAccess()
{
    MM_LOG_I() << "continuing with only the videos Android allowed";
    m_settings.setValue(QStringLiteral("videoPartialAccessAccepted"), true);
    m_lastVideoAccess = videoAccessState();
    emit videoPermissionChanged();
}

bool SystemBridge::videoAccessSkipped() const
{
    if (hasVideoPermission() || videoPartialAccessAccepted()) {
        return false;
    }
    return m_settings.value(QStringLiteral("videoAccessSkipped"), false).toBool();
}

void SystemBridge::continueWithoutVideos()
{
    MM_LOG_I() << "carrying on without access to this device's own videos";
    m_settings.setValue(QStringLiteral("videoAccessSkipped"), true);
    m_lastVideoAccess = videoAccessState();
    emit videoPermissionChanged();
}

bool SystemBridge::videoPermissionBlocked() const
{
#ifdef Q_OS_ANDROID
    if (hasVideoPermission()) {
        return false;
    }
    if (videoAccessPartial()) {
        return true;
    }
    if (!m_settings.value(QStringLiteral("videoPermissionAsked"), false).toBool()) {
        return false;
    }

    const bool rationale = QJniObject::callStaticMethod<jboolean>(
        kActivityClass, "shouldShowVideoPermissionRationale", "()Z");
    return !rationale;
#else
    return false;
#endif
}

void SystemBridge::openAppSettings()
{
#ifdef Q_OS_ANDROID
    MM_LOG_I() << "opening the system app settings page";
    QJniObject::callStaticMethod<void>(kActivityClass, "openAppSettings", "()V");
#endif
}

bool SystemBridge::usesManagedStorageRoots() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

bool SystemBridge::supportsMiniPlayer() const
{
#ifdef Q_OS_WIN
    return WindowsMiniPlayer::supported();
#else
    return false;
#endif
}

bool SystemBridge::inMiniPlayer() const
{
#ifdef Q_OS_WIN
    return WindowsMiniPlayer::active();
#else
    return false;
#endif
}

void SystemBridge::enterMiniPlayer(QObject *window, int videoWidth, int videoHeight)
{
#ifdef Q_OS_WIN
    QWindow *target = qobject_cast<QWindow *>(window);
    if (!target) {
        MM_LOG_W() << "mini player asked for without a window";
        return;
    }

    if (WindowsMiniPlayer::enter(target->winId(), videoWidth, videoHeight)) {
        emit inMiniPlayerChanged();
    }
#else
    Q_UNUSED(window)
    Q_UNUSED(videoWidth)
    Q_UNUSED(videoHeight)
#endif
}

void SystemBridge::leaveMiniPlayer(QObject *window)
{
#ifdef Q_OS_WIN
    QWindow *target = qobject_cast<QWindow *>(window);
    if (!target) {
        return;
    }

    if (WindowsMiniPlayer::leave(target->winId())) {
        emit inMiniPlayerChanged();
    }
#else
    Q_UNUSED(window)
#endif
}

bool SystemBridge::supportsTrayIcon() const
{
#ifdef Q_OS_WIN
    return WindowsTrayIcon::supported();
#else
    return false;
#endif
}

bool SystemBridge::inTray() const
{
#ifdef Q_OS_WIN
    return m_inTray;
#else
    return false;
#endif
}

#ifdef Q_OS_WIN
bool SystemBridge::ensureTray()
{
    if (m_tray) {
        return true;
    }

    m_tray = new WindowsTrayIcon(this);
    connect(m_tray, &WindowsTrayIcon::openRequested,
            this, &SystemBridge::trayOpenRequested);
    connect(m_tray, &WindowsTrayIcon::quitRequested,
            this, &SystemBridge::trayQuitRequested);
    connect(m_tray, &WindowsTrayIcon::stopStreamingRequested,
            this, &SystemBridge::trayStopStreamingRequested);
    return true;
}
#endif

void SystemBridge::hideToTray(QObject *window)
{
#ifdef Q_OS_WIN
    QWindow *target = qobject_cast<QWindow *>(window);
    if (!target) {
        MM_LOG_W() << "asked to hide to the notification area without a window";
        return;
    }

    if (!ensureTray() || !m_tray->showIcon(QString())) {
        MM_LOG_W() << "the notification area would not take the icon,"
                   << "so the window stays where it is";
        return;
    }

    target->hide();
    m_inTray = true;
    MM_LOG_I() << "the window is closed and Makimedia keeps running";
    emit inTrayChanged();
#else
    Q_UNUSED(window)
#endif
}

void SystemBridge::restoreFromTray(QObject *window)
{
#ifdef Q_OS_WIN
    QWindow *target = qobject_cast<QWindow *>(window);
    if (!target) {
        MM_LOG_W() << "asked for the window back without a window";
        return;
    }

    target->show();
    target->raise();
    target->requestActivate();

    if (m_tray) {
        m_tray->hideIcon();
    }

    if (m_inTray) {
        m_inTray = false;
        MM_LOG_I() << "the window is back from the notification area";
        emit inTrayChanged();
    }
#else
    Q_UNUSED(window)
#endif
}

void SystemBridge::setTrayStatus(const QString &tooltip, bool streaming)
{
#ifdef Q_OS_WIN
    if (!ensureTray()) {
        return;
    }
    m_tray->setTooltip(tooltip);
    m_tray->setStreaming(streaming);
#else
    Q_UNUSED(tooltip)
    Q_UNUSED(streaming)
#endif
}

void SystemBridge::showTrayNotice(const QString &title, const QString &text)
{
#ifdef Q_OS_WIN
    if (!m_tray) {
        return;
    }
    m_tray->notify(title, text);
#else
    Q_UNUSED(title)
    Q_UNUSED(text)
#endif
}

QString SystemBridge::readBundledText(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        MM_LOG_W() << "could not open the bundled text" << path << file.errorString();
        return QString();
    }

    const QString text = QString::fromUtf8(file.readAll());
    MM_LOG_I() << "read bundled text" << path << text.size() << "characters";
    return text;
}

void SystemBridge::copyToClipboard(const QString &text)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        MM_LOG_W() << "there is no clipboard on this platform";
        return;
    }

    clipboard->setText(text);
    MM_LOG_I() << "copied" << text.size() << "characters to the clipboard";
}

bool SystemBridge::canShowInFileManager() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void SystemBridge::showInFileManager(const QString &path)
{
#ifdef Q_OS_WIN
    if (path.isEmpty()) {
        MM_LOG_W() << "asked to show nothing in Explorer";
        return;
    }

    const QString native = QDir::toNativeSeparators(path);
    PIDLIST_ABSOLUTE item = ILCreateFromPathW(
        reinterpret_cast<const wchar_t *>(native.utf16()));
    if (!item) {
        MM_LOG_W() << "Explorer could not be pointed at" << native;
        return;
    }

    const HRESULT started = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const HRESULT shown = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
    if (SUCCEEDED(started)) {
        CoUninitialize();
    }
    ILFree(item);

    if (FAILED(shown)) {
        MM_LOG_W() << "Explorer refused to show" << native
                   << "result" << int(shown);
        return;
    }

    MM_LOG_I() << "showed" << native << "in Explorer";
#else
    Q_UNUSED(path)
    MM_LOG_W() << "there is no file manager to show a file in on this platform";
#endif
}

bool SystemBridge::startWindowMove(QObject *window)
{
    QWindow *target = qobject_cast<QWindow *>(window);
    return target && target->startSystemMove();
}

bool SystemBridge::usesSystemFilePicker() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

bool SystemBridge::canForceHardwareDecoding() const
{
#ifdef Q_OS_ANDROID
    return false;
#else
    return true;
#endif
}

void SystemBridge::pickSubtitleFile()
{
#ifdef Q_OS_ANDROID
    MM_LOG_I() << "opening the system document picker for a subtitle";
    QJniObject::callStaticMethod<void>(kActivityClass, "pickSubtitleFile", "()V");
#endif
}

void SystemBridge::saveLogFile()
{
#ifdef Q_OS_ANDROID
    const QString path = MmLog::logFilePath();
    const bool straightToDownloads = isTelevision();
    MM_LOG_I() << (straightToDownloads
                       ? "saving the log straight to Download from"
                       : "opening the system save picker for the log at")
               << path;
    QJniObject::callStaticMethod<void>(
        kActivityClass,
        "saveLogFile",
        "(Ljava/lang/String;Z)V",
        QJniObject::fromString(path).object<jstring>(),
        jboolean(straightToDownloads));
#endif
}

void SystemBridge::onLogSaved(int outcome)
{
    MM_LOG_I() << "log save outcome" << outcome;
    emit logFileSaved(outcome);
}

QStringList SystemBridge::readingDrives() const
{
    QStringList labels;
#ifdef Q_OS_ANDROID
    for (const QString &volume : m_readingVolumes) {
        labels.append(AndroidMediaSource::volumeLabel(QStringLiteral("mediastore://") + volume));
    }
#endif
    return labels;
}

void SystemBridge::onStorageIndexChanged(const QString &volumeName)
{
    MM_LOG_I() << "Android's video index changed on"
               << (volumeName.isEmpty() ? QStringLiteral("an unnamed volume") : volumeName);
    emit storageIndexChanged(volumeName);
}

void SystemBridge::onStorageReading(const QString &volumeName, bool reading)
{
    const bool known = m_readingVolumes.contains(volumeName);
    MM_LOG_I() << "Android" << (reading ? "started" : "stopped") << "reading drive" << volumeName;

    if (reading && !known) {
        m_readingVolumes.append(volumeName);
        emit readingDrivesChanged();
    } else if (!reading && known) {
        m_readingVolumes.removeAll(volumeName);
        emit readingDrivesChanged();
    }

    if (!reading) {
        emit storageIndexChanged(volumeName);
    }
}

void SystemBridge::requestTextInput(const QString &title, const QString &text)
{
#ifdef Q_OS_ANDROID
    MM_LOG_I() << "opening the system text input for" << title;
    QJniObject::callStaticMethod<void>(
        kActivityClass,
        "requestTextInput",
        "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString(title).object<jstring>(),
        QJniObject::fromString(text).object<jstring>());
#else
    Q_UNUSED(title)
    Q_UNUSED(text)
#endif
}

void SystemBridge::onTextEntered(const QString &text, bool accepted)
{
    MM_LOG_I() << "text input" << (accepted ? "entered" : "cancelled");
    emit textInputFinished(text, accepted);
}

void SystemBridge::onSubtitlePicked(const QString &url)
{
    if (url.isEmpty()) {
        MM_LOG_I() << "subtitle pick cancelled";
        emit subtitleFilePickCancelled();
        return;
    }

    MM_LOG_I() << "subtitle picked" << url;
    emit subtitleFilePicked(url);
}

void SystemBridge::onSubtitleFolderResult(bool granted)
{
    MM_LOG_I() << "subtitle folder access" << (granted ? "granted" : "not granted");
    emit subtitleFolderAccessResult(granted);
}

void SystemBridge::onVideoPermissionResult(bool granted)
{
    MM_LOG_I() << "video permission result:" << (granted ? "granted" : "denied")
               << "- only some videos allowed:" << videoAccessPartial();
    m_lastVideoAccess = videoAccessState();
    emit videoPermissionChanged();
}

bool SystemBridge::canSetBrightness() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

double SystemBridge::systemBrightness() const
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jfloat>(kActivityClass,
                                                "systemBrightness",
                                                "()F");
#else
    return 1.0;
#endif
}

void SystemBridge::setScreenBrightness(double value)
{
#ifdef Q_OS_ANDROID
    const float clamped = static_cast<float>(qBound(0.01, value, 1.0));
    QJniObject::callStaticMethod<void>(kActivityClass,
                                       "setScreenBrightness",
                                       "(F)V",
                                       clamped);
#else
    Q_UNUSED(value)
#endif
}

void SystemBridge::releaseScreenBrightness()
{
#ifdef Q_OS_ANDROID
    MM_LOG_D() << "released the brightness override";
    QJniObject::callStaticMethod<void>(kActivityClass,
                                       "setScreenBrightness",
                                       "(F)V",
                                       -1.0f);
#endif
}

void SystemBridge::setImmersiveMode(bool immersive)
{
#ifdef Q_OS_ANDROID
    MM_LOG_D() << "immersive mode ->" << immersive;
    QJniObject::callStaticMethod<void>(kActivityClass,
                                       "setImmersiveMode",
                                       "(Z)V",
                                       static_cast<jboolean>(immersive));
#else
    Q_UNUSED(immersive)
#endif
}

void SystemBridge::setLightStatusBar(bool light)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(
        kActivityClass,
        "setLightSystemBars",
        "(Z)V",
        static_cast<jboolean>(light));
#else
    Q_UNUSED(light)
#endif
}

bool SystemBridge::keepScreenOn() const
{
    return m_keepScreenOn;
}

void SystemBridge::setKeepScreenOn(bool keepOn)
{
    m_keepScreenOn = keepOn;

#if defined(Q_OS_ANDROID)
    QJniObject::callStaticMethod<void>(
        kActivityClass,
        "setKeepScreenOn",
        "(Z)V",
        static_cast<jboolean>(keepOn));
#else
    applyExecutionState();
#endif
}

bool SystemBridge::keepSystemAwake() const
{
    return m_keepSystemAwake;
}

void SystemBridge::setKeepSystemAwake(bool awake)
{
    if (m_keepSystemAwake == awake) {
        return;
    }
    m_keepSystemAwake = awake;
    MM_LOG_I() << "keeping the system awake:" << awake;
    applyExecutionState();
}

void SystemBridge::applyExecutionState()
{
#if defined(Q_OS_WIN)
    EXECUTION_STATE state = ES_CONTINUOUS;
    if (m_keepScreenOn) {
        state |= ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED;
    }
    if (m_keepSystemAwake) {
        state |= ES_SYSTEM_REQUIRED;
    }
    SetThreadExecutionState(state);
#endif
}

void SystemBridge::minimizeApp()
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(kActivityClass, "minimizeApp", "()V");
#endif
}

void SystemBridge::onBackInternal()
{
    MM_LOG_D() << "SystemBridge back button received";
    emit backPressed();
}

qreal SystemBridge::keyboardHeight() const
{
    return m_keyboardHeight;
}

void SystemBridge::onKeyboardHeight(int pixels)
{
    qreal ratio = 1.0;
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        if (screen->devicePixelRatio() > 0) {
            ratio = screen->devicePixelRatio();
        }
    }

    const qreal height = pixels > 0 ? pixels / ratio : 0.0;
    if (qFuzzyCompare(height + 1.0, m_keyboardHeight + 1.0)) {
        return;
    }

    m_keyboardHeight = height;
    MM_LOG_D() << "keyboard covers" << pixels << "device pixels," << height
               << "of the window";
    emit keyboardHeightChanged();
}

#ifdef Q_OS_ANDROID
extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeOnBack(JNIEnv *, jclass)
{
    if (g_systemBridge) {
        QMetaObject::invokeMethod(g_systemBridge, "onBackInternal", Qt::QueuedConnection);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeKeyboardHeight(JNIEnv *, jclass,
                                                                  jint pixels)
{
    if (g_systemBridge) {
        QMetaObject::invokeMethod(g_systemBridge, "onKeyboardHeight",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, static_cast<int>(pixels)));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeVideoPermissionResult(JNIEnv *, jclass,
                                                                         jboolean granted)
{
    if (g_systemBridge) {
        QMetaObject::invokeMethod(g_systemBridge, "onVideoPermissionResult",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, granted != JNI_FALSE));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeSubtitlePicked(JNIEnv *env, jclass,
                                                                  jstring uri)
{
    if (!g_systemBridge) {
        return;
    }

    QString value;
    if (uri) {
        const char *chars = env->GetStringUTFChars(uri, nullptr);
        if (chars) {
            value = QString::fromUtf8(chars);
            env->ReleaseStringUTFChars(uri, chars);
        }
    }

    QMetaObject::invokeMethod(g_systemBridge, "onSubtitlePicked",
                              Qt::QueuedConnection,
                              Q_ARG(QString, value));
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeSubtitleFolderResult(JNIEnv *, jclass,
                                                                        jboolean granted)
{
    if (g_systemBridge) {
        QMetaObject::invokeMethod(g_systemBridge, "onSubtitleFolderResult",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, granted != JNI_FALSE));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeLogSaved(JNIEnv *, jclass, jint outcome)
{
    if (g_systemBridge) {
        QMetaObject::invokeMethod(g_systemBridge, "onLogSaved",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, int(outcome)));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeStorageIndexChanged(JNIEnv *env, jclass,
                                                                       jstring volumeName)
{
    if (!g_systemBridge) {
        return;
    }

    QString value;
    if (volumeName) {
        const char *chars = env->GetStringUTFChars(volumeName, nullptr);
        if (chars) {
            value = QString::fromUtf8(chars);
            env->ReleaseStringUTFChars(volumeName, chars);
        }
    }

    QMetaObject::invokeMethod(g_systemBridge, "onStorageIndexChanged",
                              Qt::QueuedConnection,
                              Q_ARG(QString, value));
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeStorageReading(JNIEnv *env, jclass,
                                                                  jstring volumeName,
                                                                  jboolean reading)
{
    if (!g_systemBridge) {
        return;
    }

    QString value;
    if (volumeName) {
        const char *chars = env->GetStringUTFChars(volumeName, nullptr);
        if (chars) {
            value = QString::fromUtf8(chars);
            env->ReleaseStringUTFChars(volumeName, chars);
        }
    }

    QMetaObject::invokeMethod(g_systemBridge, "onStorageReading",
                              Qt::QueuedConnection,
                              Q_ARG(QString, value),
                              Q_ARG(bool, reading != JNI_FALSE));
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaActivity_nativeTextEntered(JNIEnv *env, jclass,
                                                               jstring text,
                                                               jboolean accepted)
{
    if (!g_systemBridge) {
        return;
    }

    QString value;
    if (text) {
        const char *chars = env->GetStringUTFChars(text, nullptr);
        if (chars) {
            value = QString::fromUtf8(chars);
            env->ReleaseStringUTFChars(text, chars);
        }
    }

    QMetaObject::invokeMethod(g_systemBridge, "onTextEntered",
                              Qt::QueuedConnection,
                              Q_ARG(QString, value),
                              Q_ARG(bool, accepted != JNI_FALSE));
}
#endif
