#include "Platform/Android/AndroidVideoSurface.h"

#include "MmLog.h"

#include <QElapsedTimer>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QSemaphore>
#include <QThread>

#include <jni.h>

#include <memory>

namespace {

const char *kSurfaceClass = "com/topicdev/makimedia/org/MakimediaVideoSurface";
constexpr int kSurfaceReleaseWaitMs = 500;

}

AndroidVideoSurface *AndroidVideoSurface::instance()
{
    static AndroidVideoSurface surface;
    return &surface;
}

AndroidVideoSurface::AndroidVideoSurface(QObject *parent)
    : QObject(parent)
{
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

void AndroidVideoSurface::setVideoSize(const QSize &size)
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "setVideoSize", "(II)V",
                                       jint(size.width()), jint(size.height()));
}

void AndroidVideoSurface::setAspect(double aspect, bool fill)
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "setAspect", "(DZ)V",
                                       jdouble(aspect), jboolean(fill));
}

void AndroidVideoSurface::setFillColour(const QColor &colour)
{
    QJniObject::callStaticMethod<void>(kSurfaceClass, "setFillColour", "(I)V",
                                       jint(colour.rgba()));
}

qint64 AndroidVideoSurface::windowHandle()
{
    if (m_globalSurface) {
        return qint64(reinterpret_cast<intptr_t>(m_globalSurface));
    }

    QJniObject surface = QJniObject::callStaticObjectMethod(
        kSurfaceClass, "surface", "()Landroid/view/Surface;");
    if (!surface.isValid()) {
        MM_LOG_W() << "no video surface to hand over yet";
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
    MM_LOG_I() << "video surface ready";
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
        MM_LOG_I() << "video surface lost";
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

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaVideoSurface_nativeVideoSurfaceReady(JNIEnv *,
                                                                         jclass)
{
    QMetaObject::invokeMethod(AndroidVideoSurface::instance(),
                              "handleSurfaceReady", Qt::QueuedConnection);
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_MakimediaVideoSurface_nativeVideoSurfaceLost(JNIEnv *,
                                                                        jclass)
{
    AndroidVideoSurface *surface = AndroidVideoSurface::instance();
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
