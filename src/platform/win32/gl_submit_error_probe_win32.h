#pragma once

#include <windows.h>

namespace oneui::internal {

// Experimental attribution only. Mode 1 controls for the interface wrapper.
// Mode 2 omits the post-submit query and can delay detection of a GL error.
// Normal rendering keeps Ganesh's error checks intact.
enum class Win32GlSubmitProbeMode { Off, Observe, Omit };

inline Win32GlSubmitProbeMode win32GlSubmitProbeMode() {
    static const auto mode = [] {
        wchar_t value[2]{};
        if (GetEnvironmentVariableW(L"ONEUI_GL_PROBE_SUBMIT_ERRORS", value, 2) != 1)
            return Win32GlSubmitProbeMode::Off;
        if (value[0] == L'1') return Win32GlSubmitProbeMode::Observe;
        if (value[0] == L'2') return Win32GlSubmitProbeMode::Omit;
        return Win32GlSubmitProbeMode::Off;
    }();
    return mode;
}

class Win32GlSubmitErrorProbe {
    static inline thread_local bool omitQuery_ = false;
    const bool active_;
    const bool previous_;

public:
    explicit Win32GlSubmitErrorProbe(bool nvidia)
        : active_(nvidia && win32GlSubmitProbeMode() == Win32GlSubmitProbeMode::Omit),
          previous_(active_ && omitQuery_) {
        if (active_) omitQuery_ = true;
    }
    ~Win32GlSubmitErrorProbe() { if (active_) omitQuery_ = previous_; }
    Win32GlSubmitErrorProbe(const Win32GlSubmitErrorProbe&) = delete;
    Win32GlSubmitErrorProbe& operator=(const Win32GlSubmitErrorProbe&) = delete;

    static bool omitQuery() { return omitQuery_; }
};

} // namespace oneui::internal
