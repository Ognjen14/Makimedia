#pragma once

#include <QQuickFramebufferObject>

#include <atomic>
#include <memory>
#include <mutex>

class MpvController;
class MpvRenderItem;

struct MpvRedrawTarget
{
    std::mutex lock;
    MpvRenderItem *item = nullptr;
    std::atomic<int> requests{0};
};

class MpvRenderItem : public QQuickFramebufferObject
{
    Q_OBJECT

    Q_PROPERTY(MpvController *controller READ controller WRITE setController NOTIFY controllerChanged FINAL)

public:
    explicit MpvRenderItem(QQuickItem *parent = nullptr);
    ~MpvRenderItem() override;

    MpvController *controller() const;
    void setController(MpvController *controller);

    Renderer *createRenderer() const override;

signals:
    void controllerChanged();

private slots:
    void doUpdate();

private:
    static void onMpvRedraw(void *ctx);

    MpvController *m_controller = nullptr;

    std::shared_ptr<MpvRedrawTarget> m_redrawTarget;

    friend class MpvRenderer;
};
