#include "Platform/PlatformServices.h"

#include "MmLog.h"
#include "Platform/IAudioFocus.h"
#include "Platform/IFolderPicker.h"
#include "Platform/IMediaSource.h"
#include "Platform/IScanner.h"
#include "Platform/ITransportControls.h"
#include "Platform/MediaFileInfo.h"

#include "Platform/FilesystemScanner.h"

#ifdef Q_OS_WIN
#  include "Platform/Windows/WindowsAudioFocus.h"
#  include "Platform/Windows/WindowsFolderPicker.h"
#  include "Platform/Windows/WindowsMediaSource.h"
#  include "Platform/Windows/WindowsTransportControls.h"
#endif

#ifdef Q_OS_ANDROID
#  include "Platform/Android/AndroidAudioFocus.h"
#  include "Platform/Android/AndroidMediaSource.h"
#  include "Platform/Android/AndroidTransportControls.h"
#endif

PlatformServices::PlatformServices(QObject *parent)
    : QObject(parent)
{
#if defined(Q_OS_WIN)
    m_mediaSource.reset(new WindowsMediaSource());
    m_folderPicker = new WindowsFolderPicker(this);
    m_scanner = new FilesystemScanner(m_mediaSource.data(), this);
    m_audioFocus = new WindowsAudioFocus(this);
    m_transportControls = new WindowsTransportControls(this);
    MM_LOG_I() << "platform services ready: windows";
#elif defined(Q_OS_ANDROID)
    m_mediaSource.reset(new AndroidMediaSource());
    m_scanner = new FilesystemScanner(m_mediaSource.data(), this);
    m_audioFocus = new AndroidAudioFocus(this);
    m_transportControls = new AndroidTransportControls(this);
    MM_LOG_I() << "platform services ready: android";
#else
    MM_LOG_E() << "platform services are not implemented for this platform yet;"
               << "folder picking, scanning and media resolution are unavailable";
#endif
}

PlatformServices::~PlatformServices() = default;

bool PlatformServices::available() const
{
    return !m_mediaSource.isNull() && m_scanner;
}

IMediaSource *PlatformServices::mediaSource() const
{
    return m_mediaSourceOverride ? m_mediaSourceOverride : m_mediaSource.data();
}

IMediaSource *PlatformServices::localMediaSource() const
{
    return m_mediaSource.data();
}

void PlatformServices::setMediaSourceOverride(IMediaSource *source)
{
    if (m_mediaSourceOverride == source) {
        return;
    }
    m_mediaSourceOverride = source;
    MM_LOG_I() << "media source is now" << (source ? "the streaming PC" : "this device");
}

IFolderPicker *PlatformServices::folderPicker() const
{
    return m_folderPicker;
}

IScanner *PlatformServices::scanner() const
{
    return m_scanner;
}

IAudioFocus *PlatformServices::audioFocus() const
{
    return m_audioFocus;
}

void PlatformServices::simulateAudioFocusEvent(const QString &event)
{
    if (!m_audioFocus) {
        MM_LOG_W() << "no audio focus implementation to simulate" << event << "on";
        return;
    }
    MM_LOG_I() << "simulating audio focus event" << event;
    m_audioFocus->simulate(event);
}

ITransportControls *PlatformServices::transportControls() const
{
    return m_transportControls;
}

QString PlatformServices::audioFocusState() const
{
    if (!m_audioFocus) {
        return QStringLiteral("no implementation on this platform");
    }
    return m_audioFocus->holdsFocus() ? QStringLiteral("held")
                                      : QStringLiteral("NOT held");
}
