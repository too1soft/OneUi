#pragma once

#include <optional>
#include <windows.h>

namespace oneui::win32 {

enum class WglStartupRehearsal { Twice, Once, OnceAfterSlowCensus };

inline WglStartupRehearsal wglStartupRehearsal() noexcept {
    static const auto policy = [] {
        wchar_t value[2]{};
        if (GetEnvironmentVariableW(L"ONEUI_WIN32_WGL_REHEARSAL_POLICY", value, 2) == 1) {
            if (value[0] == L'1') return WglStartupRehearsal::Once;
            if (value[0] == L'2') return WglStartupRehearsal::OnceAfterSlowCensus;
        }
        return WglStartupRehearsal::Twice;
    }();
    return policy;
}

inline bool wglPromotesOnDamage() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_WGL_PROMOTE_ON_DAMAGE", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

inline bool wglUsesGpuForResizeHandoff() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_WGL_RESIZE_GPU_ONLY", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

inline bool wglTracksResizePresentation() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_WGL_RESIZE_PRESENT_ACK", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

struct WglWorkerOptions {
    bool enabled;
    bool afterFirstFrame;
    bool prewarm;
    bool deferCensus;
    bool scratch;
    bool retainDamage;
    bool presentLatest;
    bool softwareHandoff;
    bool rehearsePresents;
    bool foldVirtualRowDamage;

    static WglWorkerOptions fromEnvironment(bool allowStartupDefault) noexcept {
        const auto worker = readFlag(L"ONEUI_WIN32_GL_WORKER");
        // An explicit worker selection keeps the caller's original flag defaults.
        // Deferred WGL and ANGLE are separate startup paths.
        const bool startupDefault = allowStartupDefault && !worker.has_value() &&
            flag(L"ONEUI_WIN32_WGL_STARTUP_WORKER") && !flag(L"ONEUI_WIN32_DEFER_OPENGL");
        return {
            worker.value_or(startupDefault),
            flag(L"ONEUI_WIN32_GL_WORKER_AFTER_FIRST_FRAME", startupDefault),
            flag(L"ONEUI_WIN32_GL_WORKER_PREWARM", startupDefault),
            flag(L"ONEUI_WIN32_GL_WORKER_DEFER_CENSUS", startupDefault),
            flag(L"ONEUI_WIN32_GL_WORKER_SCRATCH", startupDefault),
            flag(L"ONEUI_WIN32_GL_WORKER_DAMAGE", startupDefault),
            flag(L"ONEUI_WIN32_GL_WORKER_PRESENT_LATEST", startupDefault),
            flag(L"ONEUI_WIN32_WGL_SOFTWARE_HANDOFF", startupDefault),
            flag(L"ONEUI_WIN32_GL_WORKER_REHEARSE_PRESENTS", startupDefault),
            flag(L"ONEUI_WIN32_FOLD_FIRST_VIRTUAL_ROW_DAMAGE_WORKER", startupDefault),
        };
    }

private:
    static std::optional<bool> readFlag(const wchar_t* name) noexcept {
        wchar_t value[2]{};
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetEnvironmentVariableW(name, value, 2);
        if (length == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) return std::nullopt;
        return length == 1 && value[0] == L'1';
    }

    static bool flag(const wchar_t* name, bool fallback = false) noexcept {
        return readFlag(name).value_or(fallback);
    }
};

} // namespace oneui::win32
