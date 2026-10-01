#pragma once

#include <QColor>
#include <QObject>
#include <QSemaphore>
#include <QSize>

#include <memory>

class AndroidVideoSurface : public QObject
{
    Q_OBJECT

public:
    static AndroidVideoSurface *instance();

    void create();
    void destroy();

    bool isReady() const;

    void setVideoSize(const QSize &size);

    void setAspect(double aspect, bool fill);

    void setFillColour(const QColor &colour);

    qint64 windowHandle();

    Q_INVOKABLE void handleSurfaceReady();
    Q_INVOKABLE void handleSurfaceLost();
    void loseSurface(std::shared_ptr<QSemaphore> released);

    void holdRelease();
    void letGo();

signals:
    void readyChanged();

private:
    explicit AndroidVideoSurface(QObject *parent = nullptr);
    ~AndroidVideoSurface() override;

    void releaseWindowHandle();
    void finishReleaseWhenLetGo();

    bool m_ready = false;
    void *m_globalSurface = nullptr;
    int m_holds = 0;
    bool m_releasePending = false;
    std::shared_ptr<QSemaphore> m_released;
};
