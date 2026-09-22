#include "platform/win32/compat_win32.h"

#include <cwchar>
#include <string>

namespace oneui::win32 {

HMODULE loadSystemLibrary(const wchar_t* name) {
    if (!name || !*name || std::wcspbrk(name, L"\\/:") != nullptr ||
        std::wcscmp(name, L".") == 0 || std::wcscmp(name, L"..") == 0) {
        return nullptr;
    }
    wchar_t directory[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return nullptr;
    }
    const std::wstring path = std::wstring(directory, length) + L"\\" + name;
    return LoadLibraryW(path.c_str());
}

UINT systemDpi() {
    HDC dc = GetDC(nullptr);
    if (!dc) {
        return 96;
    }
    const int value = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return value > 0 ? static_cast<UINT>(value) : 96;
}

UINT monitorDpiOrSystem(HMONITOR monitor, MonitorDpiQuery query, UINT fallbackDpi) {
    UINT x = 0, y = 0;
    if (query && SUCCEEDED(query(monitor, 0 /* MDT_EFFECTIVE_DPI */, &x, &y)) && x > 0) {
        return x;
    }
    return fallbackDpi > 0 ? fallbackDpi : 96;
}

UINT monitorDpi(HMONITOR monitor) {
    HMODULE module = loadSystemLibrary(L"Shcore.dll");
    const auto query = module
        ? reinterpret_cast<MonitorDpiQuery>(GetProcAddress(module, "GetDpiForMonitor"))
        : nullptr;
    const UINT dpi = monitorDpiOrSystem(monitor, query, systemDpi());
    if (module) {
        FreeLibrary(module);
    }
    return dpi;
}

} // namespace oneui::win32
