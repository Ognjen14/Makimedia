#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QString>

class IAudioFocus;
class IFolderPicker;
class IMediaSource;
class IScanner;
class ITransportControls;

class PlatformServices : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ available CONSTANT FINAL)

public:
    explicit PlatformServices(QObject *parent = nullptr);
    ~PlatformServices() override;

    bool available() const;

    IMediaSource *mediaSource() const;
    IMediaSource *localMediaSource() const;
    void setMediaSourceOverride(IMediaSource *source);
    IFolderPicker *folderPicker() const;
    IScanner *scanner() const;
    IAudioFocus *audioFocus() const;
    ITransportControls *transportControls() const;

    Q_INVOKABLE void simulateAudioFocusEvent(const QString &event);
    Q_INVOKABLE QString audioFocusState() const;

private:
    QScopedPointer<IMediaSource> m_mediaSource;
    IMediaSource *m_mediaSourceOverride = nullptr;
    IFolderPicker *m_folderPicker = nullptr;
    IScanner *m_scanner = nullptr;
    IAudioFocus *m_audioFocus = nullptr;
    ITransportControls *m_transportControls = nullptr;
};
