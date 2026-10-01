#pragma once

#include <QObject>
#include <QString>

class ITransportControls : public QObject
{
    Q_OBJECT

public:
    explicit ITransportControls(QObject *parent = nullptr) : QObject(parent) {}
    ~ITransportControls() override = default;

    virtual void setActive(bool active) = 0;
    virtual void updateMetadata(const QString &title, const QString &subtitle, qint64 durationMs) = 0;
    virtual void updatePlaybackState(bool playing, qint64 positionMs, double speed) = 0;

signals:
    void playRequested();
    void pauseRequested();
    void playPauseToggleRequested();
    void stopRequested();
    void nextRequested();
    void previousRequested();
    void seekRequested(qint64 positionMs);
};
