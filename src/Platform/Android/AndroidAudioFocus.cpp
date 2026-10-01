#include "Platform/Android/AndroidAudioFocus.h"

#include "MmLog.h"

#include <QJniObject>

namespace {

constexpr const char *kFocusClass = "com/topicdev/makimedia/MakimediaAudioFocus";

constexpr int kFocusGain = 1;
constexpr int kFocusLoss = -1;
constexpr int kFocusLossTransient = -2;
constexpr int kFocusLossTransientCanDuck = -3;

AndroidAudioFocus *g_audioFocus = nullptr;

}

AndroidAudioFocus::AndroidAudioFocus(QObject *parent)
    : IAudioFocus(parent)
{
    g_audioFocus = this;
}

AndroidAudioFocus::~AndroidAudioFocus()
{
    abandonFocus();
    if (g_audioFocus == this) {
        g_audioFocus = nullptr;
    }
}

bool AndroidAudioFocus::requestFocus()
{
    const bool granted = QJniObject::callStaticMethod<jboolean>(kFocusClass,
                                                                "requestFocus",
                                                                "()Z");
    MM_LOG_I() << "audio focus request" << (granted ? "granted" : "denied");
    return granted;
}

bool AndroidAudioFocus::holdsFocus() const
{
    return QJniObject::callStaticMethod<jboolean>(kFocusClass, "holdsFocus", "()Z");
}

void AndroidAudioFocus::abandonFocus()
{
    MM_LOG_D() << "abandoning audio focus";
    QJniObject::callStaticMethod<void>(kFocusClass, "abandonFocus", "()V");
}

void AndroidAudioFocus::onFocusChanged(int change)
{
    switch (change) {
    case kFocusGain:
        MM_LOG_I() << "audio focus regained";
        emit shouldDuck(false);
        emit focusRegained();
        break;
    case kFocusLoss:
        MM_LOG_I() << "audio focus lost permanently";
        emit focusLost();
        break;
    case kFocusLossTransient:
        MM_LOG_I() << "audio focus lost temporarily";
        emit focusLostTransient();
        break;
    case kFocusLossTransientCanDuck:
        MM_LOG_I() << "audio focus ducking";
        emit shouldDuck(true);
        break;
    default:
        MM_LOG_W() << "unhandled audio focus change" << change;
        break;
    }
}

void AndroidAudioFocus::onBecomingNoisy()
{
    MM_LOG_I() << "audio becoming noisy, output route was pulled";
    emit becomingNoisy();
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaAudioFocus_nativeFocusChanged(JNIEnv *, jclass, jint change)
{
    if (g_audioFocus) {
        g_audioFocus->onFocusChanged(static_cast<int>(change));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaAudioFocus_nativeBecomingNoisy(JNIEnv *, jclass)
{
    if (g_audioFocus) {
        g_audioFocus->onBecomingNoisy();
    }
}
