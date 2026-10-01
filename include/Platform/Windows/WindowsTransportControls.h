#pragma once

#include "Platform/ITransportControls.h"

#include <QScopedPointer>

class WindowsTransportControls : public ITransportControls
{
    Q_OBJECT

public:
    explicit WindowsTransportControls(QObject *parent = nullptr);
    ~WindowsTransportControls() override;

    void setActive(bool active) override;
    void updateMetadata(const QString &title, const QString &subtitle, qint64 durationMs) override;
    void updatePlaybackState(bool playing, qint64 positionMs, double speed) override;

private:
    bool ensureControls();

    struct Private;
    QScopedPointer<Private> d;
};
