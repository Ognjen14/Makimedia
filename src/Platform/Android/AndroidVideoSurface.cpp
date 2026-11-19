#include "Platform/Android/AndroidVideoSurface.h"

#include "MmLog.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QSemaphore>
#include <QThread>

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <cstdint>
#include <memory>

namespace {

const char *kSurfaceClass = "com/topicdev/makimedia/org/MakimediaVideoSurface";
constexpr int kMainKind = 0;
constexpr int kPreviewKind = 1;
constexpr int kSurfaceReleaseWaitMs = 500;

}

AndroidVideoSurface *AndroidVideoSurface::instance()
{
    static AndroidVideoSurface surface(kMainKind);
    return &surface;
}

AndroidVideoSurface *AndroidVideoSurface::preview()
{
    static AndroidVideoSurface surface(kPreviewKind);
    return &surface;
}

AndroidVideoSurface::AndroidVideoSurface(int kind, QObject *parent)
    : QObject(parent)
    , m_kind(kind)
{
    if (QCoreApplication *app = QCoreApplication::instance()) {
        moveToThread(app->thread());
    }
}

namespace {

struct NativeWindowLayout
{
    int magic;
    int version;
    void *reserved[4];
    void (*incRef)(void *);
    void (*decRef)(void *);
    const uint32_t flags;
    const int minSwapInterval;
    const int maxSwapInterval;
    const float xdpi;
    const float ydpi;
    intptr_t oem[4];
    int (*setSwapInterval)(ANativeWindow *, int);
    int (*dequeueBufferDeprecated)(ANativeWindow *, void **);
    int (*lockBufferDeprecated)(ANativeWindow *, void *);
    int (*queueBufferDeprecated)(ANativeWindow *, void *);
    int (*query)(const ANativeWindow *, int, int *);
    int (*perform)(ANativeWindow *, int, ...);
};

constexpr int kNativeWindowMagic = ('_' << 24) | ('w' << 16) | ('n' << 8) | 'd';
constexpr int kPerformApiDisconnect = 14;
constexpr int kApiMedia = 3;

}

void AndroidVideoSurface::cutOffDeadDecoder()
{
    QJniObject surface = QJniObject::callStaticObjectMethod(
        kSurfaceClass, "surface", "(I)Landroid/view/Surface;", jint(m_kind));
    if (!surface.isValid()) {
        return;
    }

    QJniEnvironment env;
    ANativeWindow *window = ANativeWindow_fromSurface(env.jniEnv(), surface.object());
    if (!window) {
        return;
    }

    auto *layout = reinterpret_cast<NativeWindowLayout *>(window);
    if (layout->magic != kNativeWindowMagic || !layout->perform) {
        MM_LOG_W() << "the" << name() << "surface is not laid out as expected,"
                   << "leaving it as it is";
        ANativeWindow_release(window);
        return;
    }

    const int rc = layout->perform(window, kPerformApiDisconnect, kApiMedia);
    ANativeWindow_release(window);
    if (rc == 0) {
        MM_LOG_I() << "the" << name() << "surface is free of the ended decoder";
    } else {
        MM_LOG_D() << "the" << name() << "surface had no decoder to cut off, rc" << rc;
    }
}

const char *AndroidVideoSurface::name() const
{
    return m_kind == kPreviewKind ? "preview" : "playback";
}

AndroidVideoSurface::~AndroidVideoSurface()
{
    releaseWindowHandle();
}

void AndroidVideoSurface::create()
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "create", "()V");
}

void AndroidVideoSurface::destroy()
{
    releaseWindowHandle();
    QJniObject::callStaticMethod<void>(kSurfaceClass, "destroy", "()V");
}

bool AndroidVideoSurface::isReady() const
{
    return m_ready;
}

void AndroidVideoSurface::setVideoSize(const QSize &size, const QSize &pixels)
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "setVideoSize", "(IIIII)V",
                                       jint(m_kind),
                                       jint(size.width()), jint(size.height()),
                                       jint(pixels.width()), jint(pixels.height()));
}

void AndroidVideoSurface::setAspect(double aspect, bool fill)
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "setAspect", "(IDZ)V",
                                       jint(m_kind), jdouble(aspect), jboolean(fill));
}

void AndroidVideoSurface::show(const QRect &pixels)
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "show", "(IIIII)V",
                                       jint(m_kind), jint(pixels.x()), jint(pixels.y()),
                                       jint(pixels.width()), jint(pixels.height()));
}

void AndroidVideoSurface::hide()
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "hide", "(I)V", jint(m_kind));
}

bool AndroidVideoSurface::retire()
{
    const bool spare = QJniObject::callStaticMethod<jboolean>(kSurfaceClass, "retire", "(I)Z",
                                                              jint(m_kind));
    if (!spare) {
        MM_LOG_W() << "no spare" << name() << "surface left to swap in";
        return false;
    }

    MM_LOG_W() << "swapping in a spare" << name()
               << "surface, the old one is thrown away with the frozen decoder";
    m_holds = 0;
    m_releasePending = false;
    m_released.reset();
    releaseWindowHandle();
    if (m_ready) {
        m_ready = false;
        emit readyChanged();
    }
    return true;
}

void AndroidVideoSurface::setFillColour(const QColor &colour)
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "setFillColour", "(II)V",
                                       jint(m_kind), jint(colour.rgba()));
}

qint64 AndroidVideoSurface::windowHandle()
{
    if (m_globalSurface) {
        return qint64(reinterpret_cast<intptr_t>(m_globalSurface));
    }

    QJniObject surface = QJniObject::callStaticObjectMethod(
        kSurfaceClass, "surface", "(I)Landroid/view/Surface;", jint(m_kind));
    if (!surface.isValid()) {
        MM_LOG_W() << "no" << name() << "surface to hand over yet";
        return 0;
    }

    QJniEnvironment env;
    m_globalSurface = env->NewGlobalRef(surface.object());

    MM_LOG_I() << "video surface handle taken for playback";
    return qint64(reinterpret_cast<intptr_t>(m_globalSurface));
}

void AndroidVideoSurface::releaseWindowHandle()
{
    if (!m_globalSurface) {
        return;
    }

    QJniEnvironment env;
    env->DeleteGlobalRef(static_cast<jobject>(m_globalSurface));
    m_globalSurface = nullptr;
}

void AndroidVideoSurface::handleSurfaceReady()
{
    if (m_ready) {
        return;
    }

    if (m_releasePending) {
        MM_LOG_W() << "playback never let go of the last video surface, dropping it now";
        m_releasePending = false;
        m_holds = 0;
        m_released.reset();
        releaseWindowHandle();
    }

    m_ready = true;
    MM_LOG_I() << name() << "surface ready";
    emit readyChanged();
}

void AndroidVideoSurface::handleSurfaceLost()
{
    loseSurface(nullptr);
}

void AndroidVideoSurface::loseSurface(std::shared_ptr<QSemaphore> released)
{
    m_releasePending = true;
    m_released = std::move(released);

    if (m_ready) {
        m_ready = false;
        MM_LOG_I() << name() << "surface lost";
        emit readyChanged();
    }

    finishReleaseWhenLetGo();
}

void AndroidVideoSurface::holdRelease()
{
    ++m_holds;
}

void AndroidVideoSurface::letGo()
{
    if (m_holds > 0) {
        --m_holds;
    }
    finishReleaseWhenLetGo();
}

void AndroidVideoSurface::finishReleaseWhenLetGo()
{
    if (!m_releasePending || m_holds > 0) {
        return;
    }
    m_releasePending = false;
    releaseWindowHandle();
    if (m_released) {
        m_released->release();
        m_released.reset();
    }
}

namespace {

void surfaceReady(AndroidVideoSurface *surface)
{
    QMetaObject::invokeMethod(surface, "handleSurfaceReady", Qt::QueuedConnection);
}

void surfaceLost(AndroidVideoSurface *surface)
{
    if (QThread::currentThread() == surface->thread()) {
        surface->handleSurfaceLost();
        return;
    }

    QElapsedTimer waited;
    waited.start();

    const auto released = std::make_shared<QSemaphore>();
    QMetaObject::invokeMethod(surface, [surface, released]() {
        surface->loseSurface(released);
    }, Qt::QueuedConnection);

    if (released->tryAcquire(1, kSurfaceReleaseWaitMs)) {
        MM_LOG_I() << "playback let go of the video surface before Android freed it, in"
                   << waited.elapsed() << "ms";
    } else {
        MM_LOG_W() << "playback still held the video surface after"
                   << kSurfaceReleaseWaitMs << "ms, Android is freeing it anyway";
    }
}

}

static AndroidVideoSurface *surfaceOfKind(jint kind)
{
    return kind == kPreviewKind ? AndroidVideoSurface::preview()
                                : AndroidVideoSurface::instance();
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_org_MakimediaVideoSurface_nativeVideoSurfaceReady(JNIEnv *,
                                                                         jclass,
                                                                         jint kind)
{
    surfaceReady(surfaceOfKind(kind));
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_org_MakimediaVideoSurface_nativeVideoSurfaceLost(JNIEnv *,
                                                                        jclass,
                                                                        jint kind)
{
    surfaceLost(surfaceOfKind(kind));
}
