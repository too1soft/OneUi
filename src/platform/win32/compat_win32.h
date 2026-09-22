#pragma once

#include <windows.h>

namespace oneui::win32 {

using MonitorDpiQuery = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);

// Load optional OS components from System32 only. Unlike
// LOAD_LIBRARY_SEARCH_SYSTEM32, this also works on unpatched Windows 7 SP1.
// The caller owns the returned module reference and must FreeLibrary it.
HMODULE loadSystemLibrary(const wchar_t* name);
UINT systemDpi();
UINT monitorDpi(HMONITOR monitor);

// Kept separate so absence, failure and invalid results from a modern API can
// be tested without changing the desktop's DPI or requiring a newer OS API.
UINT monitorDpiOrSystem(HMONITOR monitor, MonitorDpiQuery query, UINT fallbackDpi);

} // namespace oneui::win32
