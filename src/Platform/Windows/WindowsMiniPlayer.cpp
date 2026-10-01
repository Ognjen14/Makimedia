#include "Platform/Windows/WindowsMiniPlayer.h"

#include "MmLog.h"

#include <windows.h>

namespace {

const int kMiniWidth = 480;
const int kMiniMargin = 24;

struct SavedWindow
{
    bool valid = false;
    LONG_PTR style = 0;
    LONG_PTR exStyle = 0;
    RECT frame{};
    bool maximized = false;
};

SavedWindow g_saved;

int miniHeightFor(int videoWidth, int videoHeight)
{
    if (videoWidth <= 0 || videoHeight <= 0) {
        return kMiniWidth * 9 / 16;
    }
    return qMax(120, int(qint64(kMiniWidth) * videoHeight / videoWidth));
}

}

bool WindowsMiniPlayer::supported()
{
    return true;
}

bool WindowsMiniPlayer::active()
{
    return g_saved.valid;
}

bool WindowsMiniPlayer::enter(quintptr windowHandle, int videoWidth, int videoHeight)
{
    HWND hwnd = reinterpret_cast<HWND>(windowHandle);
    if (!hwnd || g_saved.valid) {
        return false;
    }

    g_saved.style = GetWindowLongPtr(hwnd, GWL_STYLE);
    g_saved.exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    g_saved.maximized = IsZoomed(hwnd) != 0;

    if (g_saved.maximized) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    GetWindowRect(hwnd, &g_saved.frame);
    g_saved.valid = true;

    const LONG_PTR miniStyle =
        g_saved.style & ~(WS_CAPTION | WS_MINIMIZEBOX
                          | WS_MAXIMIZEBOX | WS_SYSMENU);
    SetWindowLongPtr(hwnd, GWL_STYLE, miniStyle);

    const int width = kMiniWidth;
    const int height = miniHeightFor(videoWidth, videoHeight);

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = work.right - width - kMiniMargin;
    const int y = work.bottom - height - kMiniMargin;

    SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    MM_LOG_I() << "mini player entered at" << width << "x" << height;
    return true;
}

bool WindowsMiniPlayer::leave(quintptr windowHandle)
{
    HWND hwnd = reinterpret_cast<HWND>(windowHandle);
    if (!hwnd || !g_saved.valid) {
        return false;
    }

    SetWindowLongPtr(hwnd, GWL_STYLE, g_saved.style);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, g_saved.exStyle);

    SetWindowPos(hwnd, HWND_NOTOPMOST,
                 g_saved.frame.left, g_saved.frame.top,
                 g_saved.frame.right - g_saved.frame.left,
                 g_saved.frame.bottom - g_saved.frame.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    if (g_saved.maximized) {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }

    g_saved = SavedWindow();
    MM_LOG_I() << "mini player left";
    return true;
}
