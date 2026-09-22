#include "platform/win32/compat_win32.h"

#include <cstdio>
#include <initializer_list>

namespace {
int failures = 0;
void check(bool value, const char* description) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); ++failures; }
}
HRESULT WINAPI modernDpi(HMONITOR, int, UINT* x, UINT* y) { *x = *y = 192; return S_OK; }
HRESULT WINAPI failedDpi(HMONITOR, int, UINT* x, UINT* y) { *x = *y = 192; return E_FAIL; }
HRESULT WINAPI zeroDpi(HMONITOR, int, UINT* x, UINT* y) { *x = *y = 0; return S_OK; }
}

int main() {
    using namespace oneui::win32;
    for (const UINT dpi : {96u, 120u, 144u, 192u}) {
        check(monitorDpiOrSystem(nullptr, nullptr, dpi) == dpi, "absent modern API preserves system DPI");
        check(monitorDpiOrSystem(nullptr, failedDpi, dpi) == dpi, "failed modern API preserves system DPI");
        check(monitorDpiOrSystem(nullptr, zeroDpi, dpi) == dpi, "zero modern DPI preserves system DPI");
        check(monitorDpiOrSystem(nullptr, modernDpi, dpi) == 192, "modern per-monitor DPI remains preferred");
    }
    check(monitorDpiOrSystem(nullptr, nullptr, 0) == 96, "unavailable system DPI has a safe default");
    check(systemDpi() > 0, "system DPI query");
    check(!loadSystemLibrary(L"..\\shcore.dll"), "reject relative library traversal");
    check(!loadSystemLibrary(L"C:\\shcore.dll"), "reject arbitrary absolute library");
    check(!loadSystemLibrary(nullptr), "reject null library");
    check(!loadSystemLibrary(L"oneui-nonexistent-os-library.dll"), "optional library may be absent");
    HMODULE module = loadSystemLibrary(L"kernel32.dll");
    check(module != nullptr, "system library loads without modern loader flags");
    if (module) { FreeLibrary(module); }
    return failures == 0 ? 0 : 1;
}
