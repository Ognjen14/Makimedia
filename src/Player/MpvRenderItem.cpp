#include "Player/MpvRenderItem.h"

#include "MmLog.h"
#include "Player/MpvController.h"
#include "SystemBridge.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>

#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace {

void *getProcAddress(void *ctx, const char *name)
{
    Q_UNUSED(ctx)
    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    if (!glContext) {
        return nullptr;
    }
    return reinterpret_cast<void *>(glContext->getProcAddress(QByteArray(name)));
}

QSize renderSize(const QSize &surface, const QSize &video)
{
    if (surface.isEmpty()) {
        return QSize();
    }

    if (video.isEmpty()) {
        return surface;
    }

    const double scale = qMin(double(surface.width()) / double(video.width()),
                              double(surface.height()) / double(video.height()));
    if (scale <= 1.0) {
        return surface;
    }

    return QSize(qMax(16, qRound(surface.width() / scale)),
                 qMax(16, qRound(surface.height() / scale)));
}

}

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvRenderItem *item)
        : m_item(item)
        , m_redrawTarget(item->m_redrawTarget)
    {
    }

    ~MpvRenderer() override
    {
        QObject::disconnect(m_beginConnection);
        QObject::disconnect(m_endConnection);

        if (m_renderContext) {
            mpv_render_context_set_update_callback(m_renderContext, nullptr, nullptr);
            mpv_render_context_free(m_renderContext);
            m_renderContext = nullptr;
            MM_LOG_I() << "mpv render context freed";
        }

        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "setRenderContextActive",
                                      Qt::QueuedConnection, Q_ARG(bool, false));
            m_controller = nullptr;
        }
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        attachFrameProbe(item->window());

        if (item->textureFollowsItemSize()) {
            return;
        }

        QSize surface;
        if (QQuickWindow *window = item->window()) {
            const qreal ratio = window->effectiveDevicePixelRatio();
            surface = QSize(qRound(item->width() * ratio),
                            qRound(item->height() * ratio));
        }

        MpvController *controller =
            static_cast<MpvRenderItem *>(item)->controller();
        const QSize video = controller
            ? QSize(controller->videoWidth(), controller->videoHeight())
            : QSize();

        const QSize wanted = renderSize(surface, video);
        if (!wanted.isEmpty() && wanted != m_fboSize) {
            m_fboSize = wanted;
            invalidateFramebufferObject();
        }
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        const QSize target = m_fboSize.isEmpty() ? size : m_fboSize;
        MM_LOG_I() << "video framebuffer" << target.width() << "x"
                   << target.height() << "on a surface of" << size.width()
                   << "x" << size.height();

        return new QOpenGLFramebufferObject(target,
                                            QOpenGLFramebufferObjectFormat());
    }

    void render() override
    {
        QOpenGLFramebufferObject *fbo = framebufferObject();
        if (!fbo) {
            return;
        }

        if (!m_renderContext && !m_createFailed) {
            createRenderContext();
        }

        if (m_renderContext) {
            QElapsedTimer frame;
            frame.start();

            mpv_opengl_fbo mpvFbo{static_cast<int>(fbo->handle()),
                                  fbo->width(), fbo->height(), 0};
            int flipY = 0;

            int blockForTargetTime = m_blockForTargetTime;

            mpv_render_param params[]{
                {MPV_RENDER_PARAM_OPENGL_FBO, &mpvFbo},
                {MPV_RENDER_PARAM_FLIP_Y, &flipY},
                {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &blockForTargetTime},
                {MPV_RENDER_PARAM_INVALID, nullptr}
            };
            mpv_render_context_render(m_renderContext, params);

            recordFrame(frame.nsecsElapsed() / 1000);
        }
    }

private:
    void attachFrameProbe(QQuickWindow *window)
    {
        if (m_probeAttached || !window) {
            return;
        }
        m_probeAttached = true;

        m_beginConnection = QObject::connect(
            window, &QQuickWindow::beforeFrameBegin, window,
            [this]() { m_sceneClock.start(); }, Qt::DirectConnection);

        m_endConnection = QObject::connect(
            window, &QQuickWindow::afterFrameEnd, window,
            [this]() {
                if (m_sceneClock.isValid()) {
                    m_sceneMicroseconds += m_sceneClock.nsecsElapsed() / 1000;
                    ++m_sceneFrames;
                }
            },
            Qt::DirectConnection);
    }

    void recordFrame(qint64 microseconds)
    {
        if (!m_window.isValid()) {
            m_window.start();
            m_sinceLastDraw.start();
        } else {
            m_worstGapMicroseconds =
                qMax(m_worstGapMicroseconds, m_sinceLastDraw.nsecsElapsed() / 1000);
            m_sinceLastDraw.restart();
        }

        ++m_frames;
        m_totalMicroseconds += microseconds;
        m_worstMicroseconds = qMax(m_worstMicroseconds, microseconds);

        const qint64 elapsed = m_window.elapsed();
        if (elapsed < 3000) {
            return;
        }

        const int requested = m_redrawTarget ? m_redrawTarget->requests.exchange(0) : 0;
        MM_LOG_I() << "render loop:" << m_frames << "draws in" << elapsed
                   << "ms, mpv callbacks" << requested << ", mpv draw average"
                   << (m_totalMicroseconds / qMax(1, m_frames)) / 1000.0
                   << "ms worst" << m_worstMicroseconds / 1000.0
                   << "ms | scene graph" << m_sceneFrames << "frames, average"
                   << (m_sceneMicroseconds / qMax(1, m_sceneFrames)) / 1000.0
                   << "ms | longest gap between draws"
                   << m_worstGapMicroseconds / 1000.0 << "ms";

        m_frames = 0;
        m_totalMicroseconds = 0;
        m_worstMicroseconds = 0;
        m_worstGapMicroseconds = 0;
        m_sceneFrames = 0;
        m_sceneMicroseconds = 0;
        m_window.restart();
    }

    void createRenderContext()
    {
        MpvController *controller = m_item ? m_item->controller() : nullptr;
        if (!controller || !controller->handle()) {
            MM_LOG_E() << "render item has no mpv controller, video will not draw";
            m_createFailed = true;
            return;
        }

        mpv_opengl_init_params glInit{getProcAddress, nullptr};
        mpv_render_param params[]{
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        const int rc = mpv_render_context_create(&m_renderContext,
                                                 controller->handle(), params);
        if (rc < 0) {
            MM_LOG_E() << "mpv_render_context_create failed:" << mpv_error_string(rc);
            m_renderContext = nullptr;
            m_createFailed = true;
            return;
        }

        mpv_render_context_set_update_callback(m_renderContext,
                                               MpvRenderItem::onMpvRedraw,
                                               m_redrawTarget.get());
        MM_LOG_I() << "mpv render context created";

        m_controller = controller;
        QMetaObject::invokeMethod(controller, "setRenderContextActive",
                                  Qt::QueuedConnection, Q_ARG(bool, true));
    }

    MpvRenderItem *m_item = nullptr;
    std::shared_ptr<MpvRedrawTarget> m_redrawTarget;
    MpvController *m_controller = nullptr;
    mpv_render_context *m_renderContext = nullptr;
    QSize m_fboSize;
    bool m_createFailed = false;

    const int m_blockForTargetTime =
        SystemBridge::runningOnTelevision() ? 0 : 1;

    QElapsedTimer m_window;
    QElapsedTimer m_sinceLastDraw;
    int m_frames = 0;
    qint64 m_totalMicroseconds = 0;
    qint64 m_worstMicroseconds = 0;
    qint64 m_worstGapMicroseconds = 0;

    bool m_probeAttached = false;
    QMetaObject::Connection m_beginConnection;
    QMetaObject::Connection m_endConnection;
    QElapsedTimer m_sceneClock;
    qint64 m_sceneMicroseconds = 0;
    int m_sceneFrames = 0;
};

MpvRenderItem::MpvRenderItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
    , m_redrawTarget(std::make_shared<MpvRedrawTarget>())
{
    m_redrawTarget->item = this;

    setTextureFollowsItemSize(!SystemBridge::runningOnTelevision());

}

MpvRenderItem::~MpvRenderItem()
{
    const std::lock_guard<std::mutex> guard(m_redrawTarget->lock);
    m_redrawTarget->item = nullptr;
}

MpvController *MpvRenderItem::controller() const
{
    return m_controller;
}

void MpvRenderItem::setController(MpvController *controller)
{
    if (m_controller == controller) {
        return;
    }
    m_controller = controller;
    emit controllerChanged();
    update();
}

QQuickFramebufferObject::Renderer *MpvRenderItem::createRenderer() const
{
    return new MpvRenderer(const_cast<MpvRenderItem *>(this));
}

void MpvRenderItem::onMpvRedraw(void *ctx)
{
    MpvRedrawTarget *target = static_cast<MpvRedrawTarget *>(ctx);
    target->requests.fetch_add(1);

    const std::lock_guard<std::mutex> guard(target->lock);
    if (target->item) {
        QMetaObject::invokeMethod(target->item, &MpvRenderItem::doUpdate,
                                  Qt::QueuedConnection);
    }
}

void MpvRenderItem::doUpdate()
{
    update();
}
