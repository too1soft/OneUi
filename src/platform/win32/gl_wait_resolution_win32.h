#pragma once

#include "compat_win32.h"

namespace oneui::win32 {

enum class GlWaitResolution { Default, Frame, WorkerLifetime };

inline GlWaitResolution experimentalGlWaitResolution() noexcept {
    static const auto policy = [] {
        wchar_t value[2]{};
        if (GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WAIT_RESOLUTION", value, 2) != 1)
            return GlWaitResolution::Default;
        if (value[0] == L'1') return GlWaitResolution::Frame;
        if (value[0] == L'2') return GlWaitResolution::WorkerLifetime;
        return GlWaitResolution::Default;
    }();
    return policy;
}

class ScopedGlWaitResolution {
    using Request = UINT(WINAPI*)(UINT);
    struct Api { Request begin = nullptr, end = nullptr; };

    static const Api& timerApi() noexcept {
        static const auto api = [] {
            using VersionQuery = LONG(WINAPI*)(OSVERSIONINFOW*);
            const auto query = reinterpret_cast<VersionQuery>(
                GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
            OSVERSIONINFOW version{};
            version.dwOSVersionInfoSize = sizeof(version);
            // Before Windows 10 version 2004 the request changes all processes.
            if (!query || query(&version) != 0 || version.dwMajorVersion < 10 ||
                (version.dwMajorVersion == 10 && version.dwBuildNumber < 19041)) return Api{};
            const auto module = loadSystemLibrary(L"winmm.dll");
            if (!module) return Api{};
            // Keep the system DLL mapped for outstanding scopes during teardown.
            return Api{reinterpret_cast<Request>(GetProcAddress(module, "timeBeginPeriod")),
                       reinterpret_cast<Request>(GetProcAddress(module, "timeEndPeriod"))};
        }();
        return api;
    }

    Request release_ = nullptr;

public:
    explicit ScopedGlWaitResolution(bool enabled) noexcept {
        if (!enabled) return;
        const auto& api = timerApi();
        if (api.begin && api.end && api.begin(1) == 0) release_ = api.end;
    }
    ~ScopedGlWaitResolution() { if (release_) release_(1); }
    ScopedGlWaitResolution(const ScopedGlWaitResolution&) = delete;
    ScopedGlWaitResolution& operator=(const ScopedGlWaitResolution&) = delete;
    bool acquired() const noexcept { return release_ != nullptr; }
};

} // namespace oneui::win32
