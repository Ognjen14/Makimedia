#include "AppUpdates.h"

#include "MmLog.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QWindow>

#if defined(Q_OS_ANDROID)
#  include <QCoreApplication>
#  include <QJniObject>
#  include <jni.h>
#endif

#if defined(Q_OS_WIN)
#  include "Platform/Windows/WindowsStartup.h"

#  include <windows.h>
#  include <unknwn.h>
#  include <shobjidl_core.h>

#  include <winrt/Windows.Foundation.h>
#  include <winrt/Windows.Foundation.Collections.h>
#  include <winrt/Windows.Services.Store.h>

#  include <thread>
#endif

namespace {

#if defined(Q_OS_ANDROID)
const char *kUpdatesClass = "com/topicdev/makimedia/org/MakimediaUpdates";
#endif

#if defined(Q_OS_WIN)
HWND mainWindowHandle()
{
    const QWindowList windows = QGuiApplication::topLevelWindows();
    for (QWindow *window : windows) {
        if (window->isVisible()) {
            return reinterpret_cast<HWND>(window->winId());
        }
    }
    return windows.isEmpty() ? nullptr : reinterpret_cast<HWND>(windows.first()->winId());
}

template <typename Work>
void onStoreThread(HWND window, Work work)
{
    std::thread([window, work]() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        try {
            using namespace winrt::Windows::Services::Store;
            StoreContext context = StoreContext::GetDefault();
            if (window) {
                context.as<IInitializeWithWindow>()->Initialize(window);
            }
            work(context);
        } catch (const winrt::hresult_error &error) {
            MM_LOG_W() << "microsoft store:"
                       << QString::fromWCharArray(error.message().c_str());
        }
        winrt::uninit_apartment();
    }).detach();
}
#endif

}

AppUpdates *AppUpdates::instance()
{
    static AppUpdates updates;
    return &updates;
}

AppUpdates::AppUpdates(QObject *parent)
    : QObject(parent)
{
}

int AppUpdates::state() const
{
    return m_state;
}

void AppUpdates::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

void AppUpdates::markAvailable()
{
    MM_LOG_I() << "a newer version is waiting in the store";
    setState(Available);
}

void AppUpdates::markReady()
{
    MM_LOG_I() << "the update has downloaded and installs on restart";
    setState(Ready);
}

void AppUpdates::check()
{
    if (m_checking || m_state != Idle) {
        return;
    }
    m_checking = true;

#if defined(Q_OS_ANDROID)
    MM_LOG_I() << "asking google play for an update";
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>(kUpdatesClass, "check", "(Landroid/app/Activity;)V",
                                       activity.object());
#elif defined(Q_OS_WIN)
    if (!WindowsStartup::packaged()) {
        MM_LOG_I() << "not installed from the store, so no store update to look for";
        return;
    }
    MM_LOG_I() << "asking the microsoft store for an update";
    onStoreThread(mainWindowHandle(), [this](auto context) {
        const auto updates = context.GetAppAndOptionalStorePackageUpdatesAsync().get();
        const bool found = updates.Size() > 0;
        MM_LOG_I() << "microsoft store:" << (found ? "an update is available" : "up to date");
        if (found) {
            QMetaObject::invokeMethod(this, "markAvailable", Qt::QueuedConnection);
        }
    });
#endif
}

void AppUpdates::install()
{
#if defined(Q_OS_ANDROID)
    if (m_state != Ready) {
        return;
    }
    MM_LOG_I() << "restarting to finish the update";
    QJniObject::callStaticMethod<void>(kUpdatesClass, "complete", "()V");
#elif defined(Q_OS_WIN)
    if (m_state != Available) {
        return;
    }
    MM_LOG_I() << "handing the update to the microsoft store";
    onStoreThread(mainWindowHandle(), [](auto context) {
        const auto updates = context.GetAppAndOptionalStorePackageUpdatesAsync().get();
        if (updates.Size() == 0) {
            return;
        }
        const auto result =
            context.RequestDownloadAndInstallStorePackageUpdatesAsync(updates).get();
        MM_LOG_I() << "microsoft store update finished with state" << int(result.OverallState());
    });
#endif
}

#if defined(Q_OS_ANDROID)
extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_org_MakimediaUpdates_nativeUpdateReady(JNIEnv *, jclass)
{
    QMetaObject::invokeMethod(AppUpdates::instance(), "markReady", Qt::QueuedConnection);
}
#endif
