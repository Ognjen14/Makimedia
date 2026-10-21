#include "Platform/Windows/WindowsStartup.h"

#include "MmLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include <windows.h>
#include <appmodel.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.Foundation.h>

#include <thread>

namespace {

const wchar_t kTaskId[] = L"MakimediaStartup";

const QString kRunKey =
    QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString kApprovedKey = QStringLiteral(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
    "\\StartupApproved\\Run");
const QString kValueName = QStringLiteral("Makimedia");
const QString kSignInArgument = QStringLiteral("--startup");

template <typename Work>
void offTheUiThread(Work work)
{
    std::thread worker([&work]() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        try {
            work();
        } catch (const winrt::hresult_error &error) {
            MM_LOG_W() << "windows startup task:"
                       << QString::fromWCharArray(error.message().c_str());
        }
        winrt::uninit_apartment();
    });
    worker.join();
}

WindowsStartup::State fromTask(winrt::Windows::ApplicationModel::StartupTaskState state)
{
    using winrt::Windows::ApplicationModel::StartupTaskState;
    switch (state) {
    case StartupTaskState::Enabled:
    case StartupTaskState::EnabledByPolicy:
        return WindowsStartup::State::On;
    case StartupTaskState::DisabledByUser:
        return WindowsStartup::State::OffByUser;
    case StartupTaskState::DisabledByPolicy:
        return WindowsStartup::State::OffByPolicy;
    case StartupTaskState::Disabled:
    default:
        return WindowsStartup::State::Off;
    }
}

WindowsStartup::State taskState(bool change, bool enable)
{
    WindowsStartup::State result = WindowsStartup::State::Unavailable;
    offTheUiThread([&]() {
        using namespace winrt::Windows::ApplicationModel;
        const StartupTask task = StartupTask::GetAsync(kTaskId).get();
        StartupTaskState state = task.State();
        if (change && enable && state == StartupTaskState::Disabled) {
            state = task.RequestEnableAsync().get();
        } else if (change && !enable
                   && (state == StartupTaskState::Enabled)) {
            task.Disable();
            state = task.State();
        }
        result = fromTask(state);
    });
    return result;
}

QString runCommand()
{
    return QLatin1Char('"')
           + QDir::toNativeSeparators(QCoreApplication::applicationFilePath())
           + QStringLiteral("\" ") + kSignInArgument;
}

bool approvedByUser()
{
    const QByteArray flags =
        QSettings(kApprovedKey, QSettings::NativeFormat).value(kValueName).toByteArray();
    return flags.isEmpty() || (static_cast<unsigned char>(flags.at(0)) & 1) == 0;
}

}

namespace WindowsStartup {

bool packaged()
{
    UINT32 length = 0;
    return GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
}

State state()
{
    if (packaged()) {
        return taskState(false, false);
    }

    const QSettings run(kRunKey, QSettings::NativeFormat);
    if (!run.contains(kValueName)) {
        return State::Off;
    }
    return approvedByUser() ? State::On : State::OffByUser;
}

State setEnabled(bool enabled)
{
    if (packaged()) {
        const State now = taskState(true, enabled);
        MM_LOG_I() << "start with windows" << (enabled ? "asked on" : "asked off")
                   << "- startup task is now" << int(now);
        return now;
    }

    QSettings run(kRunKey, QSettings::NativeFormat);
    if (enabled) {
        run.setValue(kValueName, runCommand());
        QSettings(kApprovedKey, QSettings::NativeFormat).remove(kValueName);
    } else {
        run.remove(kValueName);
    }
    run.sync();
    MM_LOG_I() << "start with windows" << (enabled ? "on through" : "off, removed from")
               << "the Run key";
    return state();
}

bool launchedAtSignIn()
{
    if (!packaged()) {
        return QCoreApplication::arguments().contains(kSignInArgument);
    }

    bool atSignIn = false;
    offTheUiThread([&]() {
        using namespace winrt::Windows::ApplicationModel;
        const auto args = AppInstance::GetActivatedEventArgs();
        atSignIn = args
                   && args.Kind() == Activation::ActivationKind::StartupTask;
    });
    return atSignIn;
}

}
