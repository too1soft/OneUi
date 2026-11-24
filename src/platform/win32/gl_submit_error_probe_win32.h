#pragma once

#include <windows.h>

namespace oneui::internal {

// Experimental attribution only. Mode 1 controls for the interface wrapper.
// Mode 2 omits the post-submit query and can delay detection of a GL error.
// Normal rendering keeps Ganesh's error checks intact.
enum class Win32GlProbeMode { Off, Observe, Omit };

inline Win32GlProbeMode readWin32GlProbeMode(const wchar_t* key) {
    wchar_t value[2]{};
    if (GetEnvironmentVariableW(key, value, 2) != 1)
        return Win32GlProbeMode::Off;
    if (value[0] == L'1') return Win32GlProbeMode::Observe;
    if (value[0] == L'2') return Win32GlProbeMode::Omit;
    return Win32GlProbeMode::Off;
}

inline Win32GlProbeMode win32GlSubmitProbeMode() {
    static const auto mode = readWin32GlProbeMode(L"ONEUI_GL_PROBE_SUBMIT_ERRORS");
    return mode;
}

inline Win32GlProbeMode win32GlTextureLabelProbeMode() {
    static const auto mode = readWin32GlProbeMode(L"ONEUI_GL_PROBE_TEXTURE_LABELS");
    return mode;
}

class Win32GlSubmitErrorProbe {
    static inline thread_local bool omitQuery_ = false;
    const bool active_;
    const bool previous_;

public:
    explicit Win32GlSubmitErrorProbe(bool nvidia)
        : active_(nvidia && win32GlSubmitProbeMode() == Win32GlProbeMode::Omit),
          previous_(active_ && omitQuery_) {
        if (active_) omitQuery_ = true;
    }
    ~Win32GlSubmitErrorProbe() { if (active_) omitQuery_ = previous_; }
    Win32GlSubmitErrorProbe(const Win32GlSubmitErrorProbe&) = delete;
    Win32GlSubmitErrorProbe& operator=(const Win32GlSubmitErrorProbe&) = delete;

    static bool omitQuery() { return omitQuery_; }
};

} // namespace oneui::internal
