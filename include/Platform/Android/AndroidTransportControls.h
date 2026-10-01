#pragma once

#include "Platform/ITransportControls.h"

class AndroidTransportControls : public ITransportControls
{
    Q_OBJECT

public:
    explicit AndroidTransportControls(QObject *parent = nullptr);
    ~AndroidTransportControls() override;

    void setActive(bool active) override;
    void updateMetadata(const QString &title, const QString &subtitle, qint64 durationMs) override;
    void updatePlaybackState(bool playing, qint64 positionMs, double speed) override;

    void onTransportCommand(int command, qint64 argument);
};
