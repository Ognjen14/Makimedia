#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0A00
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include "Platform/Windows/WindowsTransportControls.h"

#include "MmLog.h"

#include <QGuiApplication>
#include <QWindow>

#include <windows.h>

#include <systemmediatransportcontrolsinterop.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>

#include <chrono>

using namespace winrt::Windows::Media;

namespace {

winrt::hstring toHString(const QString &value)
{
    return winrt::hstring(reinterpret_cast<const wchar_t *>(value.utf16()),
                          static_cast<uint32_t>(value.size()));
}

HWND mainWindowHandle()
{
    const QList<QWindow *> windows = QGuiApplication::topLevelWindows();
    for (QWindow *window : windows) {
        if (window && window->handle()) {
            return reinterpret_cast<HWND>(window->winId());
        }
    }
    return nullptr;
}

}

struct WindowsTransportControls::Private
{
    SystemMediaTransportControls controls{nullptr};
    winrt::event_token buttonToken{};
    qint64 durationMs = 0;
    bool failed = false;
    bool active = false;
};

WindowsTransportControls::WindowsTransportControls(QObject *parent)
    : ITransportControls(parent)
    , d(new Private)
{
}

WindowsTransportControls::~WindowsTransportControls()
{
    if (!d->controls) {
        return;
    }

    try {
        if (d->buttonToken) {
            d->controls.ButtonPressed(d->buttonToken);
        }
        d->controls.IsEnabled(false);
    } catch (const winrt::hresult_error &e) {
        MM_LOG_W() << "tearing down the transport controls failed:"
                   << QString::fromWCharArray(e.message().c_str());
    }
}

bool WindowsTransportControls::ensureControls()
{
    if (d->controls) {
        return true;
    }
    if (d->failed) {
        return false;
    }

    HWND hwnd = mainWindowHandle();
    if (!hwnd) {
        MM_LOG_W() << "no native window yet, media keys stay unregistered";
        return false;
    }

    try {
        auto factory = winrt::get_activation_factory<SystemMediaTransportControls>();
        auto interop = factory.as<ISystemMediaTransportControlsInterop>();

        SystemMediaTransportControls smtc{nullptr};
        winrt::check_hresult(interop->GetForWindow(
            hwnd,
            winrt::guid_of<SystemMediaTransportControls>(),
            winrt::put_abi(smtc)));

        d->controls = smtc;

        d->controls.IsPlayEnabled(true);
        d->controls.IsPauseEnabled(true);
        d->controls.IsStopEnabled(true);
        d->controls.IsNextEnabled(true);
        d->controls.IsPreviousEnabled(true);

        d->buttonToken = d->controls.ButtonPressed(
            [this](const SystemMediaTransportControls &,
                   const SystemMediaTransportControlsButtonPressedEventArgs &args) {
                switch (args.Button()) {
                case SystemMediaTransportControlsButton::Play:
                    MM_LOG_I() << "media key: play";
                    emit playRequested();
                    break;
                case SystemMediaTransportControlsButton::Pause:
                    MM_LOG_I() << "media key: pause";
                    emit pauseRequested();
                    break;
                case SystemMediaTransportControlsButton::Stop:
                    MM_LOG_I() << "media key: stop";
                    emit stopRequested();
                    break;
                case SystemMediaTransportControlsButton::Next:
                    MM_LOG_I() << "media key: next";
                    emit nextRequested();
                    break;
                case SystemMediaTransportControlsButton::Previous:
                    MM_LOG_I() << "media key: previous";
                    emit previousRequested();
                    break;
                default:
                    MM_LOG_W() << "unhandled media key"
                               << static_cast<int>(args.Button());
                    break;
                }
            });

        MM_LOG_I() << "system media transport controls registered";
        return true;
    } catch (const winrt::hresult_error &e) {
        d->failed = true;
        MM_LOG_E() << "system media transport controls unavailable:"
                   << QString::fromWCharArray(e.message().c_str());
        return false;
    }
}

void WindowsTransportControls::setActive(bool active)
{
    if (active == d->active) {
        return;
    }
    if (active && !ensureControls()) {
        return;
    }
    if (!d->controls) {
        return;
    }

    d->active = active;

    try {
        MM_LOG_I() << "media transport controls ->" << (active ? "active" : "inactive");
        d->controls.IsEnabled(active);
        if (!active) {
            d->controls.PlaybackStatus(MediaPlaybackStatus::Closed);
        }
    } catch (const winrt::hresult_error &e) {
        MM_LOG_W() << "could not change the transport control state:"
                   << QString::fromWCharArray(e.message().c_str());
    }
}

void WindowsTransportControls::updateMetadata(const QString &title,
                                              const QString &subtitle,
                                              qint64 durationMs)
{
    if (!d->controls) {
        return;
    }

    d->durationMs = durationMs;

    try {
        auto updater = d->controls.DisplayUpdater();
        updater.Type(MediaPlaybackType::Video);
        updater.VideoProperties().Title(toHString(title));
        updater.VideoProperties().Subtitle(toHString(subtitle));
        updater.Update();
    } catch (const winrt::hresult_error &e) {
        MM_LOG_W() << "could not publish media metadata:"
                   << QString::fromWCharArray(e.message().c_str());
    }
}

void WindowsTransportControls::updatePlaybackState(bool playing, qint64 positionMs,
                                                   double speed)
{
    if (!d->controls) {
        return;
    }

    try {
        d->controls.PlaybackStatus(playing ? MediaPlaybackStatus::Playing
                                           : MediaPlaybackStatus::Paused);
        d->controls.PlaybackRate(speed);

        SystemMediaTransportControlsTimelineProperties timeline;
        timeline.StartTime(std::chrono::milliseconds(0));
        timeline.MinSeekTime(std::chrono::milliseconds(0));
        timeline.Position(std::chrono::milliseconds(positionMs));
        timeline.MaxSeekTime(std::chrono::milliseconds(d->durationMs));
        timeline.EndTime(std::chrono::milliseconds(d->durationMs));
        d->controls.UpdateTimelineProperties(timeline);
    } catch (const winrt::hresult_error &e) {
        MM_LOG_W() << "could not publish the playback state:"
                   << QString::fromWCharArray(e.message().c_str());
    }
}
