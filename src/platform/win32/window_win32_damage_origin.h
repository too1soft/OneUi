#pragma once

#include "internal/scroll_trace.h"

#include <windows.h>

namespace oneui::internal {

inline bool win32DamageOriginEnabled() {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_TRACE_DAMAGE_ORIGIN", value, 2) == 1 &&
               value[0] == L'1';
    }();
    return enabled && scrollTraceEnabled();
}

// Stack walking and symbolizable addresses are enabled only for a diagnostic run.
inline void traceWin32DamageOrigin(HWND window, const char* phase, const RECT* damage) {
    if (!win32DamageOriginEnabled()) return;
    static thread_local std::uint64_t sequence = 0;
    ScrollTraceEvent event{"damage", phase, reinterpret_cast<std::uintptr_t>(window)};
    event.detailA = static_cast<double>(++sequence);
    event.detailB = damage ? 0.0 : 1.0;
    if (damage) {
        event.inputDelta = damage->left;
        event.offset = damage->top;
        event.target = damage->right;
        event.velocity = damage->bottom;
    }
    writeScrollTrace(event);

    // A nested declarative route can spend most of a shallow stack relaying
    // the same rectangle through intervening containers.
    PVOID pcs[64]{};
    const auto frames = CaptureStackBackTrace(0, 64, pcs, nullptr);
    const auto app = GetModuleHandleW(nullptr);
    const auto sdk = GetModuleHandleW(L"oneui.dll");
    for (unsigned depth = 0; depth < frames; ++depth) {
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(pcs[depth], &region, sizeof(region))) continue;
        const auto base = reinterpret_cast<std::uintptr_t>(region.AllocationBase);
        const auto pc = reinterpret_cast<std::uintptr_t>(pcs[depth]);
        event.component = region.AllocationBase == app ? "app" :
                          region.AllocationBase == sdk ? "oneui" : "system";
        event.phase = "damage_origin_stack";
        event.inputDelta = static_cast<double>(pc - base);
        event.offset = 0.0;
        event.target = 0.0;
        event.velocity = 0.0;
        event.detailB = static_cast<double>(depth);
        writeScrollTrace(event);
    }
}

} // namespace oneui::internal
