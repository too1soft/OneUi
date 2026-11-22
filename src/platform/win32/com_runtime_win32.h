#pragma once
#include <windows.h>
#include <objbase.h>

namespace oneui::win32 {
// OpenGL drivers can keep message-loop threads alive after the last HWND is
// destroyed. UIA hooks execute on those threads, which have no explicit COM
// apartment. Keep one process-wide implicit MTA available for their callbacks.
inline HRESULT ensureBackgroundComRuntime() {
    struct ProcessUsage {
        // Win7 headers intentionally omit the Win8 cookie and declaration.
        // The documented cookie is an opaque PVOID; resolve the entry point
        // without requiring the newer SDK declarations at compile time.
        using IncrementUsage = HRESULT(WINAPI*)(void**);
        void* cookie = nullptr;
        HRESULT result = S_FALSE;
        ProcessUsage() {
            const auto ole = GetModuleHandleW(L"ole32.dll");
            const auto increment = ole ? reinterpret_cast<IncrementUsage>(
                GetProcAddress(ole, "CoIncrementMTAUsage")) : nullptr;
            // Dynamic lookup preserves Windows 7 compatibility.
            if (increment) result = increment(&cookie);
        }
    };
    // One bounded lease for the process, not one per window. Driver-owned
    // threads cannot be joined by OneUI and may still receive UIA callbacks
    // during CRT/DLL teardown. COM forbids CoDecrementMTAUsage at that point;
    // leave this lease to process termination rather than tearing MTA down
    // beneath foreign threads. No OneUI callback or worker is retained.
    static const ProcessUsage usage;
    return usage.result;
}
} // namespace oneui::win32
