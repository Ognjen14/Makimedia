#include "Platform/Windows/WindowsTrayIcon.h"

#include "MmLog.h"

#include <QMetaObject>

#include <string>

#include <windows.h>
#include <shellapi.h>

namespace {

const wchar_t *const kWindowClass = L"MakimediaTrayWindow";
constexpr UINT kCallbackMessage = WM_APP + 1;
constexpr UINT kIconId = 1;
constexpr UINT kMenuOpen = 1;
constexpr UINT kMenuStopStreaming = 2;
constexpr UINT kMenuQuit = 3;

UINT taskbarCreatedMessage()
{
    static const UINT message = RegisterWindowMessageW(L"TaskbarCreated");
    return message;
}

void copyInto(wchar_t *field, int capacity, const QString &text)
{
    const std::wstring value = text.toStdWString();
    lstrcpynW(field, value.c_str(), capacity);
}

LRESULT CALLBACK trayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW *created = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(created->lpCreateParams));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    WindowsTrayIcon *tray = reinterpret_cast<WindowsTrayIcon *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (tray) {
        bool handled = false;
        const qintptr result = tray->handleMessage(message, quintptr(wParam),
                                                   qintptr(lParam), &handled);
        if (handled) {
            return LRESULT(result);
        }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}

bool WindowsTrayIcon::supported()
{
    return true;
}

WindowsTrayIcon::WindowsTrayIcon(QObject *parent)
    : QObject(parent)
{
}

WindowsTrayIcon::~WindowsTrayIcon()
{
    hideIcon();

    if (m_window != 0) {
        DestroyWindow(HWND(m_window));
        m_window = 0;
    }

    if (m_icon != 0 && !m_sharedIcon) {
        DestroyIcon(HICON(m_icon));
    }
    m_icon = 0;
}

bool WindowsTrayIcon::visible() const
{
    return m_visible;
}

bool WindowsTrayIcon::ensureWindow()
{
    if (m_window != 0) {
        return true;
    }

    const HINSTANCE instance = GetModuleHandleW(nullptr);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW description{};
        description.cbSize = sizeof(description);
        description.lpfnWndProc = trayWindowProc;
        description.hInstance = instance;
        description.lpszClassName = kWindowClass;

        if (RegisterClassExW(&description) == 0) {
            MM_LOG_E() << "tray: the message window class could not be registered,"
                       << "error" << int(GetLastError());
            return false;
        }
        registered = true;
    }

    const HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, kWindowClass,
                                        WS_POPUP, 0, 0, 0, 0,
                                        nullptr, nullptr, instance, this);
    if (!window) {
        MM_LOG_E() << "tray: the message window could not be created, error"
                   << int(GetLastError());
        return false;
    }

    ChangeWindowMessageFilterEx(window, taskbarCreatedMessage(), MSGFLT_ALLOW, nullptr);

    m_window = quintptr(window);
    MM_LOG_D() << "tray: the message window is ready";
    return true;
}

bool WindowsTrayIcon::addIcon()
{
    if (m_icon == 0) {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        const int width = GetSystemMetrics(SM_CXSMICON);
        const int height = GetSystemMetrics(SM_CYSMICON);

        HICON icon = HICON(LoadImageW(instance, L"IDI_ICON1", IMAGE_ICON,
                                      width, height, LR_DEFAULTCOLOR));
        if (!icon) {
            icon = HICON(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                    width, height, LR_DEFAULTCOLOR));
        }
        if (!icon) {
            MM_LOG_W() << "tray: the app icon is not in the executable,"
                       << "showing the system one instead";
            icon = LoadIconW(nullptr, IDI_APPLICATION);
            m_sharedIcon = true;
        }
        m_icon = quintptr(icon);
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = HWND(m_window);
    data.uID = kIconId;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = kCallbackMessage;
    data.hIcon = HICON(m_icon);
    copyInto(data.szTip, int(sizeof(data.szTip) / sizeof(wchar_t)), m_tooltip);

    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        MM_LOG_E() << "tray: the icon could not be added, error"
                   << int(GetLastError());
        return false;
    }

    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

bool WindowsTrayIcon::showIcon(const QString &tooltip)
{
    if (!tooltip.isEmpty()) {
        m_tooltip = tooltip;
    }
    if (m_tooltip.isEmpty()) {
        m_tooltip = QStringLiteral("Makimedia");
    }

    if (!ensureWindow()) {
        return false;
    }

    if (m_visible) {
        setTooltip(m_tooltip);
        return true;
    }

    if (!addIcon()) {
        return false;
    }

    m_visible = true;
    MM_LOG_I() << "tray: Makimedia is in the notification area as" << m_tooltip;
    return true;
}

void WindowsTrayIcon::hideIcon()
{
    if (!m_visible) {
        return;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = HWND(m_window);
    data.uID = kIconId;

    if (!Shell_NotifyIconW(NIM_DELETE, &data)) {
        MM_LOG_W() << "tray: the icon could not be removed, error"
                   << int(GetLastError());
    }

    m_visible = false;
    MM_LOG_I() << "tray: Makimedia has left the notification area";
}

void WindowsTrayIcon::setTooltip(const QString &tooltip)
{
    if (tooltip.isEmpty() || tooltip == m_tooltip) {
        return;
    }
    m_tooltip = tooltip;

    if (!m_visible) {
        return;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = HWND(m_window);
    data.uID = kIconId;
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    copyInto(data.szTip, int(sizeof(data.szTip) / sizeof(wchar_t)), m_tooltip);

    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void WindowsTrayIcon::setStreaming(bool streaming)
{
    if (m_streaming == streaming) {
        return;
    }
    m_streaming = streaming;
    MM_LOG_D() << "tray: the menu" << (streaming ? "offers" : "no longer offers")
               << "stopping the stream";
}

void WindowsTrayIcon::notify(const QString &title, const QString &text)
{
    if (!m_visible) {
        MM_LOG_W() << "tray: nothing to show a message on -" << title;
        return;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = HWND(m_window);
    data.uID = kIconId;
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_NONE;
    copyInto(data.szInfoTitle, int(sizeof(data.szInfoTitle) / sizeof(wchar_t)), title);
    copyInto(data.szInfo, int(sizeof(data.szInfo) / sizeof(wchar_t)), text);

    if (!Shell_NotifyIconW(NIM_MODIFY, &data)) {
        MM_LOG_W() << "tray: the message could not be shown, error"
                   << int(GetLastError());
        return;
    }

    MM_LOG_I() << "tray: told the viewer" << title;
}

void WindowsTrayIcon::showMenu(int x, int y)
{
    const HWND window = HWND(m_window);

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        MM_LOG_W() << "tray: the menu could not be built, error"
                   << int(GetLastError());
        return;
    }

    const std::wstring open = tr("Open Makimedia").toStdWString();
    const std::wstring stop = tr("Stop streaming").toStdWString();
    const std::wstring quit = tr("Quit Makimedia").toStdWString();

    AppendMenuW(menu, MF_STRING, kMenuOpen, open.c_str());
    if (m_streaming) {
        AppendMenuW(menu, MF_STRING, kMenuStopStreaming, stop.c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuQuit, quit.c_str());

    UINT flags = TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY;
    flags |= (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) ? TPM_RIGHTALIGN
                                                           : TPM_LEFTALIGN;

    SetForegroundWindow(window);
    const int chosen = int(TrackPopupMenu(menu, flags, x, y, 0, window, nullptr));
    PostMessageW(window, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (chosen) {
    case kMenuOpen:
        MM_LOG_I() << "tray: the menu asked for the window back";
        QMetaObject::invokeMethod(this, [this]() { emit openRequested(); },
                                  Qt::QueuedConnection);
        break;
    case kMenuStopStreaming:
        MM_LOG_I() << "tray: the menu asked to stop streaming";
        QMetaObject::invokeMethod(this, [this]() { emit stopStreamingRequested(); },
                                  Qt::QueuedConnection);
        break;
    case kMenuQuit:
        MM_LOG_I() << "tray: the menu asked to quit";
        QMetaObject::invokeMethod(this, [this]() { emit quitRequested(); },
                                  Qt::QueuedConnection);
        break;
    default:
        break;
    }
}

qintptr WindowsTrayIcon::handleMessage(unsigned int message, quintptr wParam,
                                       qintptr lParam, bool *handled)
{
    if (message == taskbarCreatedMessage()) {
        if (m_visible) {
            MM_LOG_I() << "tray: Explorer restarted, putting the icon back";
            m_visible = false;
            m_visible = addIcon();
        }
        *handled = true;
        return 0;
    }

    if (message != kCallbackMessage) {
        return 0;
    }

    *handled = true;

    const unsigned int event = LOWORD(lParam);
    const int x = int(short(LOWORD(wParam)));
    const int y = int(short(HIWORD(wParam)));

    switch (event) {
    case NIN_SELECT:
    case NIN_KEYSELECT:
    case WM_LBUTTONDBLCLK:
        MM_LOG_I() << "tray: the icon was clicked";
        QMetaObject::invokeMethod(this, [this]() { emit openRequested(); },
                                  Qt::QueuedConnection);
        break;
    case WM_CONTEXTMENU:
        showMenu(x, y);
        break;
    default:
        break;
    }

    return 0;
}
