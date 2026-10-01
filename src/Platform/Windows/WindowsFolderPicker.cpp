#include "Platform/Windows/WindowsFolderPicker.h"

#include "MmLog.h"

#include <QDir>
#include <QStringList>
#include <QTimer>

#include <utility>

#include <windows.h>
#include <shobjidl.h>

WindowsFolderPicker::WindowsFolderPicker(QObject *parent)
    : IFolderPicker(parent)
{
}

WindowsFolderPicker::~WindowsFolderPicker() = default;

void WindowsFolderPicker::requestFolder()
{
    QTimer::singleShot(0, this, [this]() {
        const HRESULT initResult =
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninitialise =
            SUCCEEDED(initResult) && initResult != S_FALSE;

        IFileOpenDialog *dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog) {
            MM_LOG_E() << "could not create the folder picker, hresult" << hr;
            emit folderPickFailed(tr("The system folder picker is unavailable."));
            if (shouldUninitialise) {
                CoUninitialize();
            }
            return;
        }

        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options))) {
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM
                               | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT);
        }

        hr = dialog->Show(nullptr);

        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            MM_LOG_I() << "folder pick cancelled";
            emit folderPickCancelled();
            dialog->Release();
            if (shouldUninitialise) {
                CoUninitialize();
            }
            return;
        }

        QStringList picked;
        if (SUCCEEDED(hr)) {
            IShellItemArray *chosen = nullptr;
            if (SUCCEEDED(dialog->GetResults(&chosen)) && chosen) {
                DWORD count = 0;
                if (SUCCEEDED(chosen->GetCount(&count))) {
                    for (DWORD i = 0; i < count; ++i) {
                        IShellItem *item = nullptr;
                        if (FAILED(chosen->GetItemAt(i, &item)) || !item) {
                            continue;
                        }
                        PWSTR nativePath = nullptr;
                        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,
                                                           &nativePath))
                            && nativePath) {
                            picked.append(QString::fromWCharArray(nativePath));
                            CoTaskMemFree(nativePath);
                        }
                        item->Release();
                    }
                }
                chosen->Release();
            }
        }

        dialog->Release();
        if (shouldUninitialise) {
            CoUninitialize();
        }

        if (picked.isEmpty()) {
            MM_LOG_E() << "folder pick returned no usable path, hresult" << hr;
            emit folderPickFailed(tr("That location cannot be used as a scan folder."));
            return;
        }

        MM_LOG_I() << "folder pick returned" << picked.size() << "folder(s)";

        for (const QString &path : std::as_const(picked)) {
            const QString normalised = QDir::fromNativeSeparators(path);
            QString label = QDir(normalised).dirName();
            if (label.isEmpty()) {
                label = normalised;
            }
            MM_LOG_I() << "folder picked" << normalised << "shown as" << label;
            emit folderPicked(normalised, label);
        }
    });
}
