#include "Platform/Android/AndroidTransportControls.h"

#include "MmLog.h"

#include <QJniObject>

namespace {

constexpr const char *kSessionClass = "com/topicdev/makimedia/org/MakimediaMediaSession";

constexpr int kCommandPlay = 1;
constexpr int kCommandPause = 2;
constexpr int kCommandToggle = 3;
constexpr int kCommandStop = 4;
constexpr int kCommandNext = 5;
constexpr int kCommandPrevious = 6;
constexpr int kCommandSeek = 7;

AndroidTransportControls *g_transportControls = nullptr;

}

AndroidTransportControls::AndroidTransportControls(QObject *parent)
    : ITransportControls(parent)
{
    g_transportControls = this;
}

AndroidTransportControls::~AndroidTransportControls()
{
    setActive(false);
    if (g_transportControls == this) {
        g_transportControls = nullptr;
    }
}

void AndroidTransportControls::setActive(bool active)
{
    MM_LOG_I() << "media session ->" << (active ? "active" : "inactive");
    QJniObject::callStaticMethod<void>(kSessionClass, "setActive", "(Z)V",
                                       static_cast<jboolean>(active));
}

void AndroidTransportControls::updateMetadata(const QString &title,
                                              const QString &subtitle,
                                              qint64 durationMs)
{
    QJniObject::callStaticMethod<void>(
        kSessionClass,
        "updateMetadata",
        "(Ljava/lang/String;Ljava/lang/String;J)V",
        QJniObject::fromString(title).object<jstring>(),
        QJniObject::fromString(subtitle).object<jstring>(),
        static_cast<jlong>(durationMs));
}

void AndroidTransportControls::updatePlaybackState(bool playing, qint64 positionMs,
                                                   double speed)
{
    MM_LOG_D() << "media session state: playing" << playing
               << "at" << positionMs << "ms, speed" << speed;
    QJniObject::callStaticMethod<void>(kSessionClass, "updatePlaybackState", "(ZJD)V",
                                       static_cast<jboolean>(playing),
                                       static_cast<jlong>(positionMs),
                                       static_cast<jdouble>(speed));
}

void AndroidTransportControls::onTransportCommand(int command, qint64 argument)
{
    switch (command) {
    case kCommandPlay:
        emit playRequested();
        break;
    case kCommandPause:
        emit pauseRequested();
        break;
    case kCommandToggle:
        emit playPauseToggleRequested();
        break;
    case kCommandStop:
        emit stopRequested();
        break;
    case kCommandNext:
        emit nextRequested();
        break;
    case kCommandPrevious:
        emit previousRequested();
        break;
    case kCommandSeek:
        emit seekRequested(argument);
        break;
    default:
        MM_LOG_W() << "unhandled transport command" << command;
        break;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaMediaSession_nativeTransportCommand(JNIEnv *, jclass,
                                                                        jint command,
                                                                        jlong argument)
{
    if (g_transportControls) {
        g_transportControls->onTransportCommand(static_cast<int>(command),
                                                static_cast<qint64>(argument));
    }
}
