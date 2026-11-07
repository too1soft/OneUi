#pragma once
#ifdef _WIN32
#include <shobjidl.h>
#include <string>
#include <windows.h>

namespace oneui::platform::windows {
struct FileSelection {
    int status = -1; // selected=1, cancelled=0, failed=-1
    std::wstring path;
    HRESULT error = S_OK;
};
// Run on a dedicated STA helper. The owner process controls cancellation and
// lifetime, including shell initialization; no renderer event loop is nested.
inline FileSelection pickFile(bool folder, const std::wstring &title,
                              const std::wstring &initialDirectory) {
    FileSelection result;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        result.error = hr;
        return result;
    }
    struct Apartment {
        ~Apartment() { CoUninitialize(); }
    } apartment;
    IFileOpenDialog *dialog = nullptr;
    hr = CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) {
        result.error = hr;
        return result;
    }
    struct Release {
        IFileOpenDialog *value;
        ~Release() { value->Release(); }
    } release{dialog};
    FILEOPENDIALOGOPTIONS options{};
    hr = dialog->GetOptions(&options);
    if (SUCCEEDED(hr))
        hr = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                                FOS_NOCHANGEDIR | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
    if (SUCCEEDED(hr))
        hr = dialog->SetTitle(title.c_str());
    if (SUCCEEDED(hr) && !initialDirectory.empty()) {
        IShellItem *initial = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialDirectory.c_str(), nullptr,
                                                  IID_PPV_ARGS(&initial)))) {
            hr = dialog->SetFolder(initial);
            initial->Release();
        }
    }
    if (FAILED(hr)) {
        result.error = hr;
        return result;
    }
    hr = dialog->Show(nullptr);
    result.error = hr;
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        result.status = 0;
        return result;
    }
    if (FAILED(hr))
        return result;
    IShellItem *item = nullptr;
    hr = dialog->GetResult(&item);
    if (FAILED(hr)) {
        result.error = hr;
        return result;
    }
    PWSTR path = nullptr;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
    item->Release();
    if (SUCCEEDED(hr) && path) {
        result.path = path;
        result.status = 1;
    }
    if (path)
        CoTaskMemFree(path);
    result.error = hr;
    return result;
}
} // namespace oneui::platform::windows
#endif
