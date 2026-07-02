#include "oneui/platform/monitor.h"

#include <windows.h>

#include <algorithm>

namespace oneui {
namespace {

using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);

constexpr int kMdtEffectiveDpi = 0;
constexpr float kDefaultDpi = 96.0f;

Rect rectFromWinRect(const RECT& rect) {
    return Rect{
        static_cast<float>(rect.left),
        static_cast<float>(rect.top),
        static_cast<float>(rect.right - rect.left),
        static_cast<float>(rect.bottom - rect.top)
    };
}

float scaleForMonitor(HMONITOR monitor) {
    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (!shcore) {
        return 1.0f;
    }

    auto getDpiForMonitor = reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"));
    UINT dpiX = static_cast<UINT>(kDefaultDpi);
    UINT dpiY = static_cast<UINT>(kDefaultDpi);
    const bool ok = getDpiForMonitor && SUCCEEDED(getDpiForMonitor(monitor, kMdtEffectiveDpi, &dpiX, &dpiY));
    FreeLibrary(shcore);

    if (!ok || dpiX == 0) {
        return 1.0f;
    }
    return std::max(0.25f, static_cast<float>(dpiX) / kDefaultDpi);
}

MonitorInfo monitorFromHandle(HMONITOR monitor, int index) {
    MONITORINFOEXW info;
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        MonitorInfo fallback;
        fallback.index = index;
        return fallback;
    }

    MonitorInfo result;
    result.index = index;
    result.bounds = rectFromWinRect(info.rcMonitor);
    result.workArea = rectFromWinRect(info.rcWork);
    result.scale = scaleForMonitor(monitor);
    result.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    result.name = info.szDevice;
    return result;
}

BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM userData) {
    auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(userData);
    monitors->push_back(monitorFromHandle(monitor, static_cast<int>(monitors->size())));
    return TRUE;
}

MonitorInfo virtualScreenFallback() {
    MonitorInfo fallback;
    fallback.index = 0;
    fallback.primary = true;
    fallback.scale = 1.0f;
    fallback.name = L"Virtual screen";
    fallback.bounds = Rect{
        static_cast<float>(GetSystemMetrics(SM_XVIRTUALSCREEN)),
        static_cast<float>(GetSystemMetrics(SM_YVIRTUALSCREEN)),
        static_cast<float>(GetSystemMetrics(SM_CXVIRTUALSCREEN)),
        static_cast<float>(GetSystemMetrics(SM_CYVIRTUALSCREEN))
    };
    fallback.workArea = fallback.bounds;
    return fallback;
}

} // namespace

std::vector<MonitorInfo> enumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&monitors));

    if (monitors.empty()) {
        monitors.push_back(virtualScreenFallback());
    }
    return monitors;
}

} // namespace oneui
