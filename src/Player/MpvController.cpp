#include "Player/MpvController.h"

#include "MmLog.h"
#include "Platform/IAudioFocus.h"
#include "Platform/ITransportControls.h"
#include "Player/VideoTraits.h"
#include "Streaming/StreamProtocol.h"
#include "SystemBridge.h"

#include <QColor>
#include <QMetaObject>
#include <QSize>
#include <QStringList>
#include <QThread>
#include <QVariantMap>
#include <QVector>

#include <utility>

#include <mpv/client.h>

#if defined(Q_OS_ANDROID)
#  include <QCoreApplication>
#  include <QDir>
#  include <QFile>
#  include <QFileInfo>
#  include <QJniEnvironment>
#  include <QJniObject>

#  include "Platform/Android/AndroidVideoSurface.h"
#  include <QStandardPaths>

#  include <dlfcn.h>
#  include <jni.h>
#endif

namespace {

constexpr qint64 kLargestSupportedWidth = 4096;

QVariant nodeToVariant(const mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(node->u.string);
    case MPV_FORMAT_FLAG:
        return node->u.flag != 0;
    case MPV_FORMAT_INT64:
        return static_cast<qint64>(node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return node->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        mpv_node_list *entries = node->u.list;
        for (int i = 0; i < entries->num; ++i) {
            list.append(nodeToVariant(&entries->values[i]));
        }
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        mpv_node_list *entries = node->u.list;
        for (int i = 0; i < entries->num; ++i) {
            map.insert(QString::fromUtf8(entries->keys[i]), nodeToVariant(&entries->values[i]));
        }
        return map;
    }
    default:
        return QVariant();
    }
}

void wakeupCallback(void *ctx)
{
    MpvController *self = static_cast<MpvController *>(ctx);
    QMetaObject::invokeMethod(self, "drainEvents", Qt::QueuedConnection);
}

bool usesVideoSurface()
{
#if defined(Q_OS_ANDROID)
    return SystemBridge::runningOnTelevision();
#else
    return false;
#endif
}

const char *defaultHwdec()
{
#if defined(Q_OS_ANDROID)
    return "mediacodec,mediacodec-copy";
#else
    return "auto-safe";
#endif
}

#if defined(Q_OS_ANDROID)
bool handOverJavaVm()
{
    using SetJavaVmFn = int (*)(void *, void *);

    SetJavaVmFn setJavaVm = reinterpret_cast<SetJavaVmFn>(
        dlsym(RTLD_DEFAULT, "av_jni_set_java_vm"));

    if (!setJavaVm) {
        MM_LOG_E() << "av_jni_set_java_vm was not found in any loaded library."
                   << "MediaCodec cannot be reached, so hardware decoding will"
                   << "fall back to software with no error from mpv."
                   << "The libmpv build must export this symbol, or FFmpeg must"
                   << "be packaged as shared libraries alongside it.";
        return false;
    }

    JavaVM *vm = QJniEnvironment::javaVM();
    if (!vm) {
        MM_LOG_E() << "no JavaVM available, hardware decoding will fall back"
                   << "to software";
        return false;
    }

    const int rc = setJavaVm(vm, nullptr);
    if (rc != 0) {
        MM_LOG_E() << "av_jni_set_java_vm failed with" << rc
                   << "- hardware decoding will fall back to software";
        return false;
    }

    MM_LOG_I() << "JavaVM handed to FFmpeg, MediaCodec is reachable";
    return true;
}

QString externalConfigRoot()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return QString();
    }

    QJniObject dir = context.callObjectMethod("getExternalFilesDir",
                                              "(Ljava/lang/String;)Ljava/io/File;",
                                              nullptr);
    if (!dir.isValid()) {
        return QString();
    }

    return dir.callObjectMethod<jstring>("getAbsolutePath").toString();
}

QString prepareSubtitleFont()
{
    QString root = externalConfigRoot();
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    const QString dir = root + QStringLiteral("/mpv");

    if (!QDir().mkpath(dir)) {
        MM_LOG_E() << "could not create the mpv config directory" << dir
                   << "- subtitles will not render";
        return QString();
    }

    const QString target = dir + QStringLiteral("/subfont.ttf");
    if (QFileInfo::exists(target)) {
        return dir;
    }

    if (!QFile::copy(QStringLiteral(":/assets/fonts/Roboto-Regular.ttf"), target)) {
        MM_LOG_E() << "could not install the subtitle font at" << target
                   << "- Android has no fontconfig, so libass will have no"
                   << "font and subtitles will not render";
        return dir;
    }

    QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    MM_LOG_I() << "installed the subtitle font at" << target;
    return dir;
}
#endif

}

MpvController::MpvController(QObject *parent)
    : QObject(parent)
    , m_audioTracks(new TrackListModel(this))
    , m_subtitleTracks(new TrackListModel(this))
{
#if defined(Q_OS_ANDROID)
    handOverJavaVm();
#endif

    m_mpv = mpv_create();
    if (!m_mpv) {
        MM_LOG_E() << "mpv_create failed, playback unavailable";
        return;
    }

    mpv_set_option_string(m_mpv, "vo",
                          usesVideoSurface() ? "null" : "libmpv");
    mpv_set_option_string(m_mpv, "hwdec", defaultHwdec());

    if (usesVideoSurface()) {
        mpv_set_option_string(m_mpv, "vd-lavc-software-fallback", "no");
    }

    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "config", "no");
    mpv_set_option_string(m_mpv, "osc", "no");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_mpv, "sub-auto", "no");
#ifdef Q_OS_WIN
    mpv_set_option_string(m_mpv, "sub-file-paths", "Subs;Subtitles;Sub");
#else
    mpv_set_option_string(m_mpv, "sub-file-paths", "Subs:subs:Subtitles:subtitles:Sub:sub");
#endif

    if (SystemBridge::runningOnTelevision()) {
        mpv_set_option_string(m_mpv, "audio-buffer", "0.5");

        mpv_set_option_string(m_mpv, "profile", "fast");
    }

#if defined(Q_OS_ANDROID)
    const QString configDir = prepareSubtitleFont();
    if (!configDir.isEmpty()) {
        MM_LOG_I() << "mpv config directory" << configDir
                   << "- an mpv.conf placed here is read at startup";
        mpv_set_option_string(m_mpv, "config-dir", configDir.toUtf8().constData());
        mpv_set_option_string(m_mpv, "config", "yes");
    }
    mpv_set_option_string(m_mpv, "sub-font-provider", "none");
#endif

    const int rc = mpv_initialize(m_mpv);
    if (rc < 0) {
        MM_LOG_E() << "mpv_initialize failed:" << mpv_error_string(rc);
        mpv_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    mpv_request_log_messages(m_mpv, "info");
    observeProperties();
    mpv_set_wakeup_callback(m_mpv, wakeupCallback, this);
    mpv_hook_add(m_mpv, kPreloadedHookId, "on_preloaded", 0);
    startRequestThread();

    m_healthTimer.setInterval(3000);
    connect(&m_healthTimer, &QTimer::timeout, this,
            &MpvController::logPlaybackHealth);

    m_startTimer.setSingleShot(true);
    m_startTimer.setInterval(20000);
    connect(&m_startTimer, &QTimer::timeout, this,
            &MpvController::giveUpOnOpening);

#if defined(Q_OS_ANDROID)
    if (usesVideoSurface()) {
        connect(AndroidVideoSurface::instance(),
                &AndroidVideoSurface::readyChanged, this, [this]() {
            if (!AndroidVideoSurface::instance()->isReady()) {
                detachVideoSurface();
            } else if (m_fileLoaded) {
                attachVideoSurface();
            }
        });
    }
#endif

    m_ready = true;
    char *effectiveHwdec = mpv_get_property_string(m_mpv, "hwdec");
    MM_LOG_I() << "mpv initialised, hwdec requested" << defaultHwdec()
               << "in effect"
               << (effectiveHwdec ? effectiveHwdec : "<unreadable>");
    if (effectiveHwdec) {
        mpv_free(effectiveHwdec);
    }
    emit readyChanged();
}

MpvController::~MpvController()
{
    releaseAudioFocus();

    if (!m_mpv) {
        return;
    }
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    if (!stopRequestThread()) {
        return;
    }
    mpv_terminate_destroy(m_mpv);
    m_mpv = nullptr;
    MM_LOG_I() << "mpv terminated";
}

void MpvController::setAudioFocus(IAudioFocus *focus)
{
    if (m_audioFocus == focus) {
        return;
    }

    if (m_audioFocus) {
        disconnect(m_audioFocus, nullptr, this, nullptr);
    }

    m_audioFocus = focus;
    if (!m_audioFocus) {
        return;
    }

    connect(m_audioFocus, &IAudioFocus::focusLost,
            this, &MpvController::onFocusLost, Qt::QueuedConnection);
    connect(m_audioFocus, &IAudioFocus::focusLostTransient,
            this, &MpvController::onFocusLostTransient, Qt::QueuedConnection);
    connect(m_audioFocus, &IAudioFocus::focusRegained,
            this, &MpvController::onFocusRegained, Qt::QueuedConnection);
    connect(m_audioFocus, &IAudioFocus::shouldDuck,
            this, &MpvController::onShouldDuck, Qt::QueuedConnection);
    connect(m_audioFocus, &IAudioFocus::becomingNoisy,
            this, &MpvController::onBecomingNoisy, Qt::QueuedConnection);
}

void MpvController::setTransportControls(ITransportControls *controls)
{
    if (m_transportControls == controls) {
        return;
    }

    if (m_transportControls) {
        disconnect(m_transportControls, nullptr, this, nullptr);
    }

    m_transportControls = controls;
    if (!m_transportControls) {
        return;
    }

    connect(m_transportControls, &ITransportControls::playRequested, this,
            [this]() { setPaused(false); }, Qt::QueuedConnection);
    connect(m_transportControls, &ITransportControls::pauseRequested, this,
            [this]() { setPaused(true); }, Qt::QueuedConnection);
    connect(m_transportControls, &ITransportControls::playPauseToggleRequested, this,
            [this]() { setPaused(!m_paused); }, Qt::QueuedConnection);
    connect(m_transportControls, &ITransportControls::stopRequested, this,
            [this]() { setPaused(true); }, Qt::QueuedConnection);
    connect(m_transportControls, &ITransportControls::seekRequested, this,
            [this](qint64 positionMs) { seekAbsolute(double(positionMs) / 1000.0); },
            Qt::QueuedConnection);
}

void MpvController::publishTransportMetadata()
{
    if (!m_transportControls) {
        return;
    }
    m_transportControls->updateMetadata(m_mediaTitle,
                                        QString(),
                                        qint64(m_duration * 1000.0));
}

void MpvController::publishTransportState()
{
    if (!m_transportControls) {
        return;
    }
    m_transportControls->updatePlaybackState(!m_paused,
                                             qint64(m_position * 1000.0),
                                             m_speed);
}

void MpvController::acquireAudioFocus()
{
    if (!m_audioFocus || m_holdsAudioFocus) {
        return;
    }
    m_holdsAudioFocus = m_audioFocus->requestFocus();
}

void MpvController::releaseAudioFocus()
{
    if (!m_audioFocus || !m_holdsAudioFocus) {
        return;
    }
    m_audioFocus->abandonFocus();
    m_holdsAudioFocus = false;
    m_pausedByFocusLoss = false;
    onShouldDuck(false);
}

void MpvController::onFocusLost()
{
    MM_LOG_I() << "pausing, audio focus lost for good";
    m_pausedByFocusLoss = false;
    setPaused(true);
    releaseAudioFocus();
}

void MpvController::onFocusLostTransient()
{
    if (m_paused) {
        return;
    }
    MM_LOG_I() << "pausing, audio focus lost temporarily";
    m_pausedByFocusLoss = true;
    setPaused(true);
}

void MpvController::onFocusRegained()
{
    if (!m_pausedByFocusLoss) {
        return;
    }
    MM_LOG_I() << "resuming, audio focus regained";
    m_pausedByFocusLoss = false;
    setPaused(false);
}

void MpvController::onShouldDuck(bool duck)
{
    if (duck == m_ducked) {
        return;
    }

    if (duck) {
        const int current = m_volume.load();
        m_volumeBeforeDuck = current;
        m_ducked = true;
        MM_LOG_D() << "ducking from" << current;
        setVolume(current * 30 / 100);
    } else {
        const int restored = m_volumeBeforeDuck.load();
        m_ducked = false;
        MM_LOG_D() << "unducking to" << restored;
        setVolume(restored);
    }
}

void MpvController::onBecomingNoisy()
{
    if (m_paused) {
        return;
    }
    MM_LOG_I() << "pausing, the audio output route was pulled";
    m_pausedByFocusLoss = false;
    setPaused(true);
}

void MpvController::pauseForBackground()
{
    m_pausedByFocusLoss = false;
    if (m_paused || !m_fileLoaded) {
        return;
    }
    MM_LOG_I() << "pausing, the app went to the background";
    setPaused(true);
}

mpv_handle *MpvController::handle() const
{
    return m_mpv;
}

void MpvController::observeProperties()
{
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "eof-reached", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "sub-text", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "hwdec-current", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "video-codec", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "audio-codec-name", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "file-format", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "width", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "height", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "dwidth", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "dheight", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "video-params/primaries", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "video-params/gamma", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "aid", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "sid", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "sub-delay", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "audio-delay", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "video-aspect-override", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "panscan", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "video-zoom", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "video-pan-x", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "video-pan-y", MPV_FORMAT_DOUBLE);
}

void MpvController::drainEvents()
{
    if (!m_mpv) {
        return;
    }

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (!event || event->event_id == MPV_EVENT_NONE) {
            break;
        }

        switch (event->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE:
            handlePropertyChange(event->data);
            break;
        case MPV_EVENT_HOOK: {
            const mpv_event_hook *hook = static_cast<mpv_event_hook *>(event->data);
            if (event->reply_userdata == kPreloadedHookId) {
                addPreloadedSubtitles();
            }
            const uint64_t hookId = hook->id;
            post([hookId](mpv_handle *mpv) { mpv_hook_continue(mpv, hookId); });
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            m_fileLoaded = true;
            m_openInFlight = false;
            m_endReached = false;
            MM_LOG_I() << "file loaded, duration" << m_duration << "title" << m_mediaTitle;
            post([counts = m_dropCounts](mpv_handle *) {
                counts->vo = 0;
                counts->decoder = 0;
            });
            m_healthTimer.start();
            emit fileLoadedChanged();
            emit endReachedChanged();
            setLoading(false);
            publishTransportMetadata();
            publishTransportState();
            pickPreloadedSubtitle();
            break;
        case MPV_EVENT_END_FILE: {
            mpv_event_end_file *endFile = static_cast<mpv_event_end_file *>(event->data);
            if (m_fileLoaded) {
                m_fileLoaded = false;
                emit fileLoadedChanged();
            }

            if (endFile->reason == MPV_END_FILE_REASON_ERROR) {
                m_openInFlight = false;
            }
            if (!m_openInFlight) {
                setLoading(false);
            }

            if (endFile->reason == MPV_END_FILE_REASON_ERROR) {
                QString reason = QString::fromUtf8(mpv_error_string(endFile->error));
                MM_LOG_E() << "playback ended with error:" << reason;

                if (m_videoChainFailed) {
                    reason = tr("this television's decoder will not take it");
                    MM_LOG_W() << "the decoder refused the file and there is "
                                  "no software fallback on a television";
                }
                emit playbackFailed(reason);
            } else {
                MM_LOG_I() << "playback ended, reason" << int(endFile->reason);
            }
            m_healthTimer.stop();
            if (m_openInFlight) {
                MM_LOG_I() << "a file ended while the next one is opening, so its "
                              "audio focus and transport controls are left alone";
                break;
            }

            m_startTimer.stop();
            releaseAudioFocus();
            if (m_transportControls) {
                m_transportControls->setActive(false);
            }
            break;
        }
        case MPV_EVENT_PLAYBACK_RESTART:
            publishTransportState();
            break;
        case MPV_EVENT_LOG_MESSAGE: {
            mpv_event_log_message *message = static_cast<mpv_event_log_message *>(event->data);
            const QString text = QString::fromUtf8(message->text).trimmed();

            QString key = QString::fromUtf8(message->prefix) + text;
            key.removeIf([](QChar c) { return c.isDigit(); });

            if (!m_mpvWindow.isValid() || m_mpvWindow.elapsed() > 3000) {
                flushMpvRepeats();
                m_mpvWindow.restart();
            }

            const auto seen = m_mpvSuppressed.find(key);
            if (seen != m_mpvSuppressed.end()) {
                ++seen.value();
                break;
            }
            m_mpvSuppressed.insert(key, 0);

            MM_LOG_D() << "mpv" << message->prefix << text;

            if (text.contains(QLatin1String("Could not initialize video chain"))) {
                m_videoChainFailed = true;
            }

            if (text.contains(QLatin1String("mediacodec"), Qt::CaseInsensitive)
                || text.contains(QLatin1String("Error while decoding"))) {
                m_decoderComplained = true;
            }
            break;
        }
        case MPV_EVENT_SHUTDOWN:
            MM_LOG_I() << "mpv shutdown event";
            return;
        default:
            break;
        }
    }
}

void MpvController::handlePropertyChange(void *eventProperty)
{
    mpv_event_property *prop = static_cast<mpv_event_property *>(eventProperty);
    if (!prop) {
        return;
    }

    const QString name = QString::fromUtf8(prop->name);

    if (!prop->data) {
        clearProperty(name);
        return;
    }

    if (name == QLatin1String("pause")) {
        const bool value = *static_cast<int *>(prop->data) != 0;
        if (m_paused != value) {
            m_paused = value;
            MM_LOG_D() << "paused ->" << value;
            if (!m_paused) {
                acquireAudioFocus();
            }
            publishTransportState();
            emit pausedChanged();
        }
    } else if (name == QLatin1String("time-pos")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_position, value)) {
            m_position = value;
            emit positionChanged();
        }
        if (value > 0.25) {
            m_startTimer.stop();
        }
    } else if (name == QLatin1String("duration")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_duration, value)) {
            m_duration = value;
            emit durationChanged();
        }
    } else if (name == QLatin1String("speed")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_speed, value)) {
            m_speed = value;
            publishTransportState();
            emit speedChanged();
        }
    } else if (name == QLatin1String("volume")) {
        const int value = static_cast<int>(*static_cast<double *>(prop->data));
        if (m_volume != value) {
            m_volume = value;
            emit volumeChanged();
        }
    } else if (name == QLatin1String("mute")) {
        const bool value = *static_cast<int *>(prop->data) != 0;
        if (m_muted != value) {
            m_muted = value;
            emit mutedChanged();
        }
    } else if (name == QLatin1String("paused-for-cache")) {
        const bool value = *static_cast<int *>(prop->data) != 0;
        if (m_buffering != value) {
            m_buffering = value;
            MM_LOG_D() << "buffering ->" << m_buffering;
            emit bufferingChanged();
        }
    } else if (name == QLatin1String("eof-reached")) {
        const bool value = *static_cast<int *>(prop->data) != 0;
        if (m_endReached != value) {
            m_endReached = value;
            MM_LOG_D() << "end reached ->" << m_endReached;
            emit endReachedChanged();
        }
    } else if (name == QLatin1String("media-title")) {
        const QString value = QString::fromUtf8(*static_cast<char **>(prop->data));
        if (m_mediaTitle != value) {
            m_mediaTitle = value;
            emit mediaTitleChanged();
        }
    } else if (name == QLatin1String("sub-text")) {
        const char **text = static_cast<const char **>(prop->data);
        const QString value = text && *text ? QString::fromUtf8(*text) : QString();
        if (m_subtitleText != value) {
            m_subtitleText = value;
            emit subtitleTextChanged();
        }
    } else if (name == QLatin1String("hwdec-current")) {
        const QString value = QString::fromUtf8(*static_cast<char **>(prop->data));
        if (m_hwdecActive != value) {
            m_hwdecActive = value;
            MM_LOG_I() << "active decoder ->" << m_hwdecActive;
            emit hwdecActiveChanged();
        }
    } else if (name == QLatin1String("video-codec")) {
        const QString value = QString::fromUtf8(*static_cast<char **>(prop->data));
        if (m_videoCodec != value) {
            m_videoCodec = value;
            MM_LOG_I() << "video codec ->" << m_videoCodec;
            emit videoCodecChanged();
        }
    } else if (name == QLatin1String("audio-codec-name")) {
        const QString value = QString::fromUtf8(*static_cast<char **>(prop->data));
        if (m_audioCodec != value) {
            m_audioCodec = value;
            emit audioCodecChanged();
        }
    } else if (name == QLatin1String("file-format")) {
        const QString value = QString::fromUtf8(*static_cast<char **>(prop->data));
        if (m_fileFormat != value) {
            m_fileFormat = value;
            MM_LOG_I() << "container ->" << m_fileFormat;
            emit fileFormatChanged();
        }
    } else if (name == QLatin1String("width")) {
        m_videoWidth = *static_cast<qint64 *>(prop->data);
        updateVideoResolution();
    } else if (name == QLatin1String("height")) {
        m_videoHeight = *static_cast<qint64 *>(prop->data);
        updateVideoResolution();
    } else if (name == QLatin1String("dwidth")) {
        m_displayWidth = *static_cast<qint64 *>(prop->data);
        updateVideoResolution();
    } else if (name == QLatin1String("dheight")) {
        m_displayHeight = *static_cast<qint64 *>(prop->data);
        updateVideoResolution();
    } else if (name == QLatin1String("video-params/primaries")) {
        m_videoPrimaries = QString::fromUtf8(*static_cast<char **>(prop->data));
        updateHdr();
    } else if (name == QLatin1String("video-params/gamma")) {
        m_videoGamma = QString::fromUtf8(*static_cast<char **>(prop->data));
        updateHdr();
    } else if (name == QLatin1String("track-list")) {
        rebuildTrackLists(nodeToVariant(static_cast<mpv_node *>(prop->data)));
    } else if (name == QLatin1String("aid")) {
        const qint64 value = *static_cast<qint64 *>(prop->data);
        if (m_audioTrackId != value) {
            m_audioTrackId = value;
            MM_LOG_D() << "audio track ->" << m_audioTrackId;
            emit audioTrackIdChanged();
        }
    } else if (name == QLatin1String("sid")) {
        const qint64 value = *static_cast<qint64 *>(prop->data);
        if (m_subtitleTrackId != value) {
            m_subtitleTrackId = value;
            MM_LOG_D() << "subtitle track ->" << m_subtitleTrackId;
            emit subtitleTrackIdChanged();
        }
    } else if (name == QLatin1String("sub-delay")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_subDelay, value)) {
            m_subDelay = value;
            emit subDelayChanged();
        }
    } else if (name == QLatin1String("audio-delay")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_audioDelay, value)) {
            m_audioDelay = value;
            emit audioDelayChanged();
        }
    } else if (name == QLatin1String("video-aspect-override")) {
        const QString value = QString::fromUtf8(*static_cast<char **>(prop->data));
        if (m_aspectOverride != value) {
            m_aspectOverride = value;
            emit aspectOverrideChanged();
        }
    } else if (name == QLatin1String("video-zoom")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_videoZoom + 1.0, value + 1.0)) {
            m_videoZoom = value;
            emit videoZoomChanged();
        }
    } else if (name == QLatin1String("video-pan-x")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_videoPanX + 1.0, value + 1.0)) {
            m_videoPanX = value;
            emit videoPanChanged();
        }
    } else if (name == QLatin1String("video-pan-y")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_videoPanY + 1.0, value + 1.0)) {
            m_videoPanY = value;
            emit videoPanChanged();
        }
    } else if (name == QLatin1String("panscan")) {
        const double value = *static_cast<double *>(prop->data);
        if (!qFuzzyCompare(m_panscan, value)) {
            m_panscan = value;
            emit panscanChanged();
        }
    }
}

void MpvController::clearProperty(const QString &name)
{
    if (name == QLatin1String("aid")) {
        if (m_audioTrackId != 0) {
            m_audioTrackId = 0;
            MM_LOG_D() << "audio track -> none";
            emit audioTrackIdChanged();
        }
    } else if (name == QLatin1String("sid")) {
        if (m_subtitleTrackId != 0) {
            m_subtitleTrackId = 0;
            MM_LOG_D() << "subtitle track -> none";
            emit subtitleTrackIdChanged();
        }
    } else if (name == QLatin1String("time-pos")) {
        if (!qFuzzyIsNull(m_position)) {
            m_position = 0.0;
            emit positionChanged();
        }
    } else if (name == QLatin1String("duration")) {
        if (!qFuzzyIsNull(m_duration)) {
            m_duration = 0.0;
            emit durationChanged();
        }
    } else if (name == QLatin1String("media-title")) {
        if (!m_mediaTitle.isEmpty()) {
            m_mediaTitle.clear();
            emit mediaTitleChanged();
        }
    } else if (name == QLatin1String("sub-text")) {
        if (!m_subtitleText.isEmpty()) {
            m_subtitleText.clear();
            emit subtitleTextChanged();
        }
    } else if (name == QLatin1String("hwdec-current")) {
        if (!m_hwdecActive.isEmpty()) {
            m_hwdecActive.clear();
            MM_LOG_I() << "active decoder -> none";
            emit hwdecActiveChanged();
        }
    } else if (name == QLatin1String("video-codec")) {
        if (!m_videoCodec.isEmpty()) {
            m_videoCodec.clear();
            emit videoCodecChanged();
        }
    } else if (name == QLatin1String("audio-codec-name")) {
        if (!m_audioCodec.isEmpty()) {
            m_audioCodec.clear();
            emit audioCodecChanged();
        }
    } else if (name == QLatin1String("file-format")) {
        if (!m_fileFormat.isEmpty()) {
            m_fileFormat.clear();
            emit fileFormatChanged();
        }
    } else if (name == QLatin1String("width")) {
        m_videoWidth = 0;
        updateVideoResolution();
    } else if (name == QLatin1String("height")) {
        m_videoHeight = 0;
        updateVideoResolution();
    } else if (name == QLatin1String("dwidth")) {
        m_displayWidth = 0;
        updateVideoResolution();
    } else if (name == QLatin1String("dheight")) {
        m_displayHeight = 0;
        updateVideoResolution();
    } else if (name == QLatin1String("video-params/primaries")) {
        m_videoPrimaries.clear();
        updateHdr();
    } else if (name == QLatin1String("video-params/gamma")) {
        m_videoGamma.clear();
        updateHdr();
    }
}

void MpvController::updateHdr()
{
    const bool value = VideoTraits::isHdr(m_videoPrimaries, m_videoGamma);

    if (!m_videoPrimaries.isEmpty() || !m_videoGamma.isEmpty()) {
        MM_LOG_D() << "colour: primaries" << m_videoPrimaries
                   << "gamma" << m_videoGamma << "-> hdr" << value;
    }

    if (m_hdr != value) {
        m_hdr = value;
        MM_LOG_I() << "hdr ->" << m_hdr;
        emit hdrChanged();
    }
}

void MpvController::flushMpvRepeats()
{
    int repeats = 0;
    for (const int count : std::as_const(m_mpvSuppressed)) {
        repeats += count;
    }
    if (repeats > 0) {
        MM_LOG_D() << "mpv repeated itself" << repeats << "more times across"
                   << m_mpvSuppressed.size() << "messages";
    }
    m_mpvSuppressed.clear();
}

void MpvController::giveUpOnOpening()
{
    if (!m_mpv || m_paused || m_position > 0.25) {
        return;
    }

    MM_LOG_E() << "nothing has been played in 20 s -"
               << (m_fileLoaded ? "the file loaded but never moved"
                                : "the file never finished opening")
               << (m_decoderComplained ? "and the decoder was complaining"
                                       : "and the decoder said nothing");

    command({QStringLiteral("stop")});

    m_openInFlight = false;
    setLoading(false);

    emit playbackFailed(m_decoderComplained
                        ? tr("this device's decoder will not take it")
                        : tr("nothing was played"));
}

void MpvController::logPlaybackHealth()
{
    if (!m_mpv || m_paused) {
        return;
    }

    post([counts = m_dropCounts](mpv_handle *mpv) {
        auto readDouble = [mpv](const char *name) {
            double value = 0.0;
            if (mpv_get_property(mpv, name, MPV_FORMAT_DOUBLE, &value) < 0) {
                return 0.0;
            }
            return value;
        };

        auto readCount = [mpv](const char *name) {
            qint64 value = 0;
            if (mpv_get_property(mpv, name, MPV_FORMAT_INT64, &value) < 0) {
                return qint64(0);
            }
            return value;
        };

        const qint64 voDrops = readCount("frame-drop-count");
        const qint64 decoderDrops = readCount("decoder-frame-drop-count");
        const qint64 newVoDrops = voDrops - counts->vo;
        const qint64 newDecoderDrops = decoderDrops - counts->decoder;
        counts->vo = voDrops;
        counts->decoder = decoderDrops;

        MM_LOG_I() << "playback health: avsync" << readDouble("avsync")
                   << "fps" << readDouble("estimated-vf-fps")
                   << "of" << readDouble("container-fps")
                   << "| dropped since last: output" << newVoDrops
                   << "decoder" << newDecoderDrops
                   << "| cache" << readDouble("demuxer-cache-duration") << "s";
    });
}

void MpvController::updateVideoResolution()
{
    const QString value = (m_videoWidth > 0 && m_videoHeight > 0)
        ? QStringLiteral("%1x%2").arg(m_videoWidth).arg(m_videoHeight)
        : QString();
    if (m_videoResolution != value) {
        m_videoResolution = value;
        MM_LOG_I() << "video resolution ->" << m_videoResolution;
        emit videoResolutionChanged();
    }

    if (m_videoWidth > kLargestSupportedWidth && !m_pictureJudged) {
        m_pictureJudged = true;
        if (m_allowUnsupportedPicture) {
            MM_LOG_W() << "letting a picture" << m_videoWidth
                       << "wide through on purpose - the developer switch is "
                          "on, and the 20 s give-up is what should end this";
        } else {
            MM_LOG_W() << "refusing a picture" << m_videoWidth << "wide -"
                       << kLargestSupportedWidth << "is as large as we go";
            m_startTimer.stop();
            command({QStringLiteral("stop")});
            m_openInFlight = false;
            setLoading(false);
            emit playbackFailed(tr("8K video is not supported"));
            return;
        }
    }

#if defined(Q_OS_ANDROID)
    if (usesVideoSurface()) {
        const QSize shape = (m_displayWidth > 0 && m_displayHeight > 0)
            ? QSize(int(m_displayWidth), int(m_displayHeight))
            : QSize(int(m_videoWidth), int(m_videoHeight));

        if (shape.width() > 0 && shape.height() > 0 && shape != m_surfaceSize) {
            m_surfaceSize = shape;
            MM_LOG_I() << "video surface shaped to" << shape;
            AndroidVideoSurface::instance()->setVideoSize(shape);
        }
    }
#endif
}

void MpvController::rebuildTrackLists(const QVariant &trackList)
{
    QVariantList audio;
    QVariantList subtitles;

    const QVariantList entries = trackList.toList();
    for (const QVariant &entry : entries) {
        const QVariantMap track = entry.toMap();
        const QString type = track.value(QStringLiteral("type")).toString();

        QVariantMap item;
        item.insert(QStringLiteral("id"), track.value(QStringLiteral("id")));
        item.insert(QStringLiteral("title"), track.value(QStringLiteral("title")));
        item.insert(QStringLiteral("lang"), track.value(QStringLiteral("lang")));
        item.insert(QStringLiteral("codec"), track.value(QStringLiteral("codec")));
        item.insert(QStringLiteral("selected"), track.value(QStringLiteral("selected")));
        item.insert(QStringLiteral("external"), track.value(QStringLiteral("external")));

        if (type == QLatin1String("audio")) {
            audio.append(item);
        } else if (type == QLatin1String("sub")) {
            subtitles.append(item);
        }
    }

    m_audioTracks->setTracks(audio);
    m_subtitleTracks->setTracks(subtitles);

    if (audio.isEmpty() && subtitles.isEmpty()) {
        MM_LOG_D() << "track list -> empty";
        return;
    }

    MM_LOG_I() << "track list ->" << audio.size() << "audio,"
               << subtitles.size() << "subtitle";
}

void MpvController::startRequestThread()
{
    m_requestThread = new QThread();
    m_requestThread->setObjectName(QStringLiteral("MpvRequests"));
    m_requestWorker = new QObject();
    m_requestWorker->moveToThread(m_requestThread);
    m_requestThread->start();
}

bool MpvController::stopRequestThread()
{
    if (!m_requestThread) {
        return true;
    }
    m_requestThread->quit();
    if (!m_requestThread->wait(2000)) {
        MM_LOG_W() << "mpv was still stuck on a request as the app closed, leaving it";
        return false;
    }
    delete m_requestWorker;
    m_requestWorker = nullptr;
    delete m_requestThread;
    m_requestThread = nullptr;
    return true;
}

void MpvController::post(std::function<void(mpv_handle *)> work)
{
    if (!m_mpv || !m_requestWorker) {
        return;
    }
    mpv_handle *mpv = m_mpv;
    QMetaObject::invokeMethod(m_requestWorker, [mpv, work = std::move(work)]() {
        QElapsedTimer took;
        took.start();
        work(mpv);
        if (took.elapsed() > 1000) {
            MM_LOG_W() << "mpv took" << took.elapsed() << "ms to answer a request";
        }
    }, Qt::QueuedConnection);
}

void MpvController::command(const QVariantList &args)
{
    QVector<QByteArray> storage;
    storage.reserve(args.size());
    for (const QVariant &arg : args) {
        storage.append(arg.toString().toUtf8());
    }

    QStringList words;
    for (const QVariant &arg : args) {
        words.append(arg.toString());
    }
    const QString logged = StreamProtocol::withoutToken(words.join(QLatin1Char(' ')));
    post([storage, logged](mpv_handle *mpv) {
        QVector<const char *> argv;
        argv.reserve(storage.size() + 1);
        for (const QByteArray &item : storage) {
            argv.append(item.constData());
        }
        argv.append(nullptr);

        const int rc = mpv_command(mpv, argv.data());
        if (rc < 0) {
            MM_LOG_E() << "mpv command failed:" << logged << mpv_error_string(rc);
        }
    });
}

void MpvController::setPropertyVariant(const QString &name, const QVariant &value)
{
    const QByteArray nameUtf8 = name.toUtf8();
    const QByteArray valueUtf8 = value.toString().toUtf8();
    post([nameUtf8, valueUtf8](mpv_handle *mpv) {
        const int rc = mpv_set_property_string(mpv, nameUtf8.constData(), valueUtf8.constData());
        if (rc < 0) {
            MM_LOG_E() << "mpv set property failed:" << nameUtf8 << valueUtf8
                       << mpv_error_string(rc);
        }
    });
}

bool MpvController::ready() const { return m_ready; }
bool MpvController::fileLoaded() const { return m_fileLoaded; }
bool MpvController::buffering() const { return m_buffering; }
bool MpvController::endReached() const { return m_endReached; }

bool MpvController::allowUnsupportedPicture() const
{
    return m_allowUnsupportedPicture;
}

void MpvController::setAllowUnsupportedPicture(bool allow)
{
    if (m_allowUnsupportedPicture == allow) {
        return;
    }
    m_allowUnsupportedPicture = allow;
    MM_LOG_W() << "a picture too large to play will"
               << (allow ? "be let through, for the watchdog to end"
                         : "be refused as usual");
    emit allowUnsupportedPictureChanged();
}

double MpvController::position() const { return m_position; }
double MpvController::duration() const { return m_duration; }
QString MpvController::mediaTitle() const { return m_mediaTitle; }
QString MpvController::subtitleText() const { return m_subtitleText; }
QString MpvController::hwdecActive() const { return m_hwdecActive; }
QString MpvController::videoCodec() const { return m_videoCodec; }
QString MpvController::audioCodec() const { return m_audioCodec; }
QString MpvController::videoResolution() const { return m_videoResolution; }

QString MpvController::fileFormat() const { return m_fileFormat; }

bool MpvController::hdr() const { return m_hdr; }
int MpvController::videoWidth() const { return static_cast<int>(m_videoWidth); }

int MpvController::videoHeight() const { return static_cast<int>(m_videoHeight); }
TrackListModel *MpvController::audioTracks() const { return m_audioTracks; }
TrackListModel *MpvController::subtitleTracks() const { return m_subtitleTracks; }
qint64 MpvController::audioTrackId() const { return m_audioTrackId; }
qint64 MpvController::subtitleTrackId() const { return m_subtitleTrackId; }
double MpvController::speed() const { return m_speed; }
int MpvController::volume() const { return m_volume; }
bool MpvController::muted() const { return m_muted; }
double MpvController::subDelay() const { return m_subDelay; }
double MpvController::audioDelay() const { return m_audioDelay; }
QString MpvController::aspectOverride() const { return m_aspectOverride; }
double MpvController::panscan() const { return m_panscan; }
double MpvController::videoZoom() const { return m_videoZoom; }
double MpvController::videoPanX() const { return m_videoPanX; }
double MpvController::videoPanY() const { return m_videoPanY; }
bool MpvController::paused() const { return m_paused; }

void MpvController::setPaused(bool paused)
{
    setPropertyVariant(QStringLiteral("pause"), paused ? QStringLiteral("yes")
                                                       : QStringLiteral("no"));
}

void MpvController::setSpeed(double speed)
{
    setPropertyVariant(QStringLiteral("speed"), speed);
}

void MpvController::setVolume(int volume)
{
    setPropertyVariant(QStringLiteral("volume"), qBound(0, volume, 130));
}

void MpvController::setMuted(bool muted)
{
    setPropertyVariant(QStringLiteral("mute"), muted ? QStringLiteral("yes")
                                                     : QStringLiteral("no"));
}

void MpvController::setAudioTrackId(qint64 id)
{
    setPropertyVariant(QStringLiteral("aid"), id <= 0 ? QStringLiteral("no")
                                                      : QString::number(id));
}

void MpvController::setSubtitleTrackId(qint64 id)
{
    setPropertyVariant(QStringLiteral("sid"), id <= 0 ? QStringLiteral("no")
                                                      : QString::number(id));
}

void MpvController::setSubDelay(double seconds)
{
    setPropertyVariant(QStringLiteral("sub-delay"), seconds);
}

void MpvController::setAudioDelay(double seconds)
{
    setPropertyVariant(QStringLiteral("audio-delay"), seconds);
}

void MpvController::setAspectOverride(const QString &aspect)
{
    MM_LOG_I() << "aspect override ->" << aspect;
    setPropertyVariant(QStringLiteral("video-aspect-override"), aspect);
}

void MpvController::setPanscan(double value)
{
    setPropertyVariant(QStringLiteral("panscan"), qBound(0.0, value, 1.0));
}

void MpvController::setVideoZoom(double value)
{
    setPropertyVariant(QStringLiteral("video-zoom"), qBound(-2.0, value, 2.0));
}

void MpvController::setVideoPanX(double value)
{
    setPropertyVariant(QStringLiteral("video-pan-x"), qBound(-1.0, value, 1.0));
}

void MpvController::setVideoPanY(double value)
{
    setPropertyVariant(QStringLiteral("video-pan-y"), qBound(-1.0, value, 1.0));
}

void MpvController::resetVideoZoom()
{
    setVideoZoom(0.0);
    setVideoPanX(0.0);
    setVideoPanY(0.0);
}

void MpvController::open(const QUrl &url)
{
    openPath(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void MpvController::setRenderContextActive(bool active)
{
    if (m_renderContextActive == active) {
        return;
    }

    m_renderContextActive = active;
    MM_LOG_I() << "render context" << (active ? "attached" : "detached");

    if (!active || m_pendingOpenPath.isEmpty()) {
        return;
    }

    const QString pending = m_pendingOpenPath;
    m_pendingOpenPath.clear();
    MM_LOG_I() << "flushing deferred open" << StreamProtocol::withoutToken(pending);
    openPath(pending);
}

bool MpvController::renderContextActive() const
{
    return m_renderContextActive;
}

void MpvController::setSurfaceAspect(double aspect, bool fill)
{
#if defined(Q_OS_ANDROID)
    if (usesVideoSurface()) {
        AndroidVideoSurface::instance()->setAspect(aspect, fill);
        return;
    }
#endif
    Q_UNUSED(aspect)
    Q_UNUSED(fill)
}

bool MpvController::attachVideoSurface()
{
#if defined(Q_OS_ANDROID)
    if (!m_mpv) {
        return false;
    }

    if (m_videoSurfaceAttached) {
        return true;
    }

    AndroidVideoSurface *surface = AndroidVideoSurface::instance();

    const qint64 handle = surface->windowHandle();
    if (handle == 0) {
        MM_LOG_E() << "no video surface to play into";
        return false;
    }

    post([handle](mpv_handle *mpv) {
        int64_t wid = handle;
        int rc = mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);
        if (rc < 0) {
            MM_LOG_E() << "mpv would not take the video surface:"
                       << mpv_error_string(rc);
            return;
        }

        rc = mpv_set_property_string(mpv, "vo", "mediacodec_embed");
        if (rc < 0) {
            MM_LOG_E() << "mpv would not switch to the embedded output:"
                       << mpv_error_string(rc);
        }
    });

    surface->setFillColour(QColor(Qt::transparent));

    m_videoSurfaceAttached = true;
    MM_LOG_I() << "video output attached to the system surface";
    return true;
#else
    return false;
#endif
}

void MpvController::detachVideoSurface()
{
#if defined(Q_OS_ANDROID)
    if (!m_videoSurfaceAttached) {
        return;
    }

    m_videoSurfaceAttached = false;

    if (!m_mpv || !m_requestWorker) {
        return;
    }
    AndroidVideoSurface *surface = AndroidVideoSurface::instance();
    surface->holdRelease();
    post([surface](mpv_handle *mpv) {
        mpv_set_property_string(mpv, "vo", "null");
        MM_LOG_I() << "video output detached from the system surface";
        QMetaObject::invokeMethod(surface, [surface]() { surface->letGo(); },
                                  Qt::QueuedConnection);
    });
#endif
}

void MpvController::openPath(const QString &path)
{
    if (!m_ready) {
        MM_LOG_E() << "open requested before mpv was ready:" << StreamProtocol::withoutToken(path);
        return;
    }

    if (m_fileLoaded) {
        MM_LOG_I() << "stopping the file already open before loading the next one";
        command({QStringLiteral("stop")});
        m_fileLoaded = false;
        emit fileLoadedChanged();
    }

    setLoading(true);
    m_openInFlight = true;
    m_videoChainFailed = false;
    m_decoderComplained = false;
    m_pictureJudged = false;
    m_pickPreloadedSubtitle = false;
    m_startTimer.start();

    if (usesVideoSurface()) {
        if (!attachVideoSurface()) {
            setLoading(false);
            m_openInFlight = false;
            emit playbackFailed(tr("The video surface is not available."));
            return;
        }
    } else if (!m_renderContextActive) {
        MM_LOG_I() << "no render context yet, deferring open of"
                   << StreamProtocol::withoutToken(path);
        m_pendingOpenPath = path;
        return;
    }

    MM_LOG_I() << "opening" << StreamProtocol::withoutToken(path);

    setPropertyVariant(QStringLiteral("sid"),
                       m_subtitlesOnByDefault ? QStringLiteral("auto")
                                              : QStringLiteral("no"));

    acquireAudioFocus();
    if (m_transportControls) {
        m_transportControls->setActive(true);
    }
    m_endReached = false;
    emit endReachedChanged();

    m_pausedByFocusLoss = false;
    setPaused(false);

    m_subtitlesForLoad = path == m_subtitlesPath ? m_pendingSubtitles : QVariantList();
    m_subtitlesPath.clear();
    m_pendingSubtitles.clear();

    applyCacheProfile(path);
    command({QStringLiteral("loadfile"), path});
}

void MpvController::applyCacheProfile(const QString &path)
{
    const bool network = path.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
                         || path.startsWith(QLatin1String("https://"), Qt::CaseInsensitive);
    if (network == m_networkCacheProfile) {
        return;
    }
    m_networkCacheProfile = network;

    if (network) {
        setPropertyVariant(QStringLiteral("cache"), QStringLiteral("yes"));
        setPropertyVariant(QStringLiteral("demuxer-max-bytes"), QStringLiteral("150MiB"));
        setPropertyVariant(QStringLiteral("demuxer-readahead-secs"), 20);
        setPropertyVariant(QStringLiteral("network-timeout"), 15);
        MM_LOG_I() << "network cache profile on: cache yes, 150 MiB, 20 s read-ahead";
    } else {
        setPropertyVariant(QStringLiteral("cache"), QStringLiteral("auto"));
        setPropertyVariant(QStringLiteral("demuxer-readahead-secs"), 1);
        setPropertyVariant(QStringLiteral("network-timeout"), 60);
        MM_LOG_I() << "network cache profile off: local playback settings";
    }
}

void MpvController::setSubtitlesForNextOpen(const QString &path,
                                            const QVariantList &subtitles)
{
    m_subtitlesPath = path;
    m_pendingSubtitles = subtitles;
}

void MpvController::addPreloadedSubtitles()
{
    if (m_subtitlesForLoad.isEmpty()) {
        return;
    }

    const QVariantList subtitles = m_subtitlesForLoad;
    m_subtitlesForLoad.clear();

    for (const QVariant &item : subtitles) {
        const QVariantMap subtitle = item.toMap();
        command({QStringLiteral("sub-add"),
                 subtitle.value(QStringLiteral("url")),
                 QStringLiteral("auto"),
                 subtitle.value(QStringLiteral("title")),
                 subtitle.value(QStringLiteral("language"))});
    }

    MM_LOG_I() << "added" << subtitles.size()
               << "subtitles found beside the video before tracks were chosen";

    m_firstPreloadedSubtitle =
        subtitles.first().toMap().value(QStringLiteral("url")).toString();
    m_pickPreloadedSubtitle = m_subtitlesOnByDefault;
}

void MpvController::pickPreloadedSubtitle()
{
    if (!m_mpv || !m_pickPreloadedSubtitle) {
        return;
    }
    m_pickPreloadedSubtitle = false;

    const QString firstPreloaded = m_firstPreloadedSubtitle;
    post([firstPreloaded](mpv_handle *mpv) {
        char *sid = mpv_get_property_string(mpv, "sid");
        const bool nothingShowing = sid == nullptr || qstrcmp(sid, "no") == 0;
        if (sid) {
            mpv_free(sid);
        }
        if (!nothingShowing) {
            return;
        }

        mpv_node node;
        if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) < 0) {
            MM_LOG_W() << "could not read the track list to switch a subtitle on";
            return;
        }
        const QVariantList tracks = nodeToVariant(&node).toList();
        mpv_free_node_contents(&node);

        qint64 chosen = -1;
        QString chosenTitle;
        for (const QVariant &item : tracks) {
            const QVariantMap track = item.toMap();
            if (track.value(QStringLiteral("type")).toString() != QLatin1String("sub")
                || !track.value(QStringLiteral("external")).toBool()) {
                continue;
            }
            const bool isFirst = track.value(QStringLiteral("external-filename")).toString()
                                 == firstPreloaded;
            if (chosen < 0 || isFirst) {
                chosen = track.value(QStringLiteral("id")).toLongLong();
                chosenTitle = track.value(QStringLiteral("title")).toString();
            }
            if (isFirst) {
                break;
            }
        }

        if (chosen < 0) {
            return;
        }
        MM_LOG_I() << "subtitles are on and none was showing - switching on" << chosenTitle
                   << "track" << chosen;
        const QByteArray value = QByteArray::number(chosen);
        mpv_set_property_string(mpv, "sid", value.constData());
    });
}

void MpvController::stop()
{
    m_pendingOpenPath.clear();
    m_subtitlesPath.clear();
    m_pendingSubtitles.clear();
    m_subtitlesForLoad.clear();
    m_openInFlight = false;
    setLoading(false);
    MM_LOG_I() << "stop requested at" << m_position;
    releaseAudioFocus();
    if (m_transportControls) {
        m_transportControls->setActive(false);
    }
    command({QStringLiteral("stop")});
}

bool MpvController::loading() const
{
    return m_loading;
}

void MpvController::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    MM_LOG_D() << "loading ->" << m_loading;
    emit loadingChanged();
}

void MpvController::togglePause()
{
    setPaused(!m_paused);
}

void MpvController::seekRelative(double seconds)
{
    MM_LOG_D() << "seek relative" << seconds << "from" << m_position;
    command({QStringLiteral("seek"), QString::number(seconds), QStringLiteral("relative")});
}

void MpvController::seekAbsolute(double seconds)
{
    MM_LOG_D() << "seek absolute" << seconds;
    command({QStringLiteral("seek"), QString::number(seconds), QStringLiteral("absolute")});
}

void MpvController::addSubtitleFile(const QString &path, const QString &title)
{
    MM_LOG_I() << "adding subtitle file" << StreamProtocol::withoutToken(path) << "as" << title;

    if (title.isEmpty()) {
        command({QStringLiteral("sub-add"), path, QStringLiteral("select")});
        return;
    }

    command({QStringLiteral("sub-add"), path, QStringLiteral("select"), title});
}

void MpvController::setPropertyFallback(const QString &name,
                                        const QString &legacyName,
                                        const QVariant &value)
{
    const QByteArray nameUtf8 = name.toUtf8();
    const QByteArray legacyUtf8 = legacyName.toUtf8();
    const QByteArray valueUtf8 = value.toString().toUtf8();

    post([nameUtf8, legacyUtf8, valueUtf8](mpv_handle *mpv) {
        if (mpv_set_property_string(mpv, nameUtf8.constData(), valueUtf8.constData()) >= 0) {
            return;
        }

        if (mpv_set_property_string(mpv, legacyUtf8.constData(), valueUtf8.constData()) < 0) {
            MM_LOG_W() << "neither" << nameUtf8 << "nor" << legacyUtf8
                       << "accepted the value" << valueUtf8
                       << "- this build of libmpv knows neither";
        }
    });
}

void MpvController::applySubtitleStyle(const QVariantMap &options)
{
    if (!m_mpv) {
        return;
    }

    const int scalePercent = options.value(QStringLiteral("scalePercent"), 100).toInt();
    const QString edgeStyle = options.value(QStringLiteral("edgeStyle")).toString();
    const QString position = options.value(QStringLiteral("position")).toString();
    const QString color = options.value(QStringLiteral("color")).toString();
    const bool bold = options.value(QStringLiteral("bold"), false).toBool();
    const QString subLanguage = options.value(QStringLiteral("subtitleLanguage")).toString();
    const QString audioLanguage = options.value(QStringLiteral("audioLanguage")).toString();
    const bool subtitlesOn =
        options.value(QStringLiteral("subtitlesOnByDefault"), true).toBool();

    const double scale = qBound(10, scalePercent, 400) / 100.0;
    setPropertyVariant(QStringLiteral("sub-scale"), scale);

    if (edgeStyle == QLatin1String("none")) {
        setPropertyVariant(QStringLiteral("sub-border-style"),
                           QStringLiteral("outline-and-shadow"));
        setPropertyVariant(QStringLiteral("sub-border-size"), 0.0);
        setPropertyVariant(QStringLiteral("sub-shadow-offset"), 0.0);
    } else if (edgeStyle == QLatin1String("shadow")) {
        setPropertyVariant(QStringLiteral("sub-border-style"),
                           QStringLiteral("outline-and-shadow"));
        setPropertyVariant(QStringLiteral("sub-border-size"), 0.8);
        setPropertyVariant(QStringLiteral("sub-shadow-offset"), 4.0);
    } else if (edgeStyle == QLatin1String("box")) {
        setPropertyVariant(QStringLiteral("sub-border-style"),
                           QStringLiteral("background-box"));
        setPropertyVariant(QStringLiteral("sub-back-color"),
                           QStringLiteral("#A0000000"));
        setPropertyVariant(QStringLiteral("sub-shadow-offset"), 0.0);
    } else {
        setPropertyVariant(QStringLiteral("sub-border-style"),
                           QStringLiteral("outline-and-shadow"));
        setPropertyVariant(QStringLiteral("sub-border-size"), 3.5);
        setPropertyVariant(QStringLiteral("sub-shadow-offset"), 0.0);
    }

    int subPos = 100;
    if (position == QLatin1String("raised")) {
        subPos = 85;
    } else if (position == QLatin1String("top")) {
        subPos = 5;
    }
    setPropertyVariant(QStringLiteral("sub-pos"), subPos);

    setPropertyFallback(QStringLiteral("sub-ass-override"),
                        QStringLiteral("ass-style-override"),
                        QStringLiteral("no"));

    if (!color.isEmpty()) {
        setPropertyVariant(QStringLiteral("sub-color"), color);
    }

    setPropertyVariant(QStringLiteral("sub-bold"),
                       bold ? QStringLiteral("yes") : QStringLiteral("no"));

    setPropertyVariant(QStringLiteral("slang"),
                       subLanguage.isEmpty() ? QStringLiteral("") : subLanguage);
    setPropertyVariant(QStringLiteral("alang"),
                       audioLanguage.isEmpty() ? QStringLiteral("") : audioLanguage);

    m_subtitlesOnByDefault = subtitlesOn;

    MM_LOG_I() << "subtitle style applied: scale" << scale
               << "edge" << edgeStyle
               << "sub-pos" << subPos
               << "colour" << color
               << "bold" << bold
               << "slang" << (subLanguage.isEmpty() ? QStringLiteral("any") : subLanguage)
               << "alang" << (audioLanguage.isEmpty() ? QStringLiteral("any") : audioLanguage)
               << "subs on" << subtitlesOn;
}

void MpvController::setForceHardwareDecoding(bool force)
{
    if (!m_mpv) {
        return;
    }

#if defined(Q_OS_ANDROID)
    Q_UNUSED(force)
#else
    const QString value = force ? QStringLiteral("auto")
                                : QString::fromLatin1(defaultHwdec());

    setPropertyVariant(QStringLiteral("hwdec"), value);
    MM_LOG_I() << "hwdec ->" << value << (force ? "(forced)" : "(default)");
#endif
}

void MpvController::cycleAudioTrack()
{
    command({QStringLiteral("cycle"), QStringLiteral("audio")});
}

void MpvController::cycleSubtitleTrack()
{
    command({QStringLiteral("cycle"), QStringLiteral("sub")});
}
