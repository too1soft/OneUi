#pragma once

#include <windows.h>
#include <cmath>
#include <cstdint>

namespace oneui::win32 {

inline bool handoffInputMessage(UINT message) {
    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
    case WM_IME_COMPOSITION:
    case WM_PASTE:
    case WM_DROPFILES:
        return true;
    default:
        return false;
    }
}

class HandoffInputScope {
public:
    static bool enabled() noexcept {
        static const bool value = [] {
            wchar_t flag[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_ANGLE_DIRTY_INPUT_HANDOFF", flag, 2) == 1 && flag[0] == L'1';
        }();
        return value;
    }

    HandoffInputScope(HWND hwnd, UINT message) noexcept : previous_(active_) {
        // Nested messages get their own context, even on the same HWND.
        // No window member is touched on unwind if a callback closed it.
        active_ = hwnd && (handoffInputMessage(message) || message == WM_LBUTTONDBLCLK ||
            message == WM_SYSKEYUP || message == WM_UNICHAR || message == WM_IME_ENDCOMPOSITION ||
            message == WM_MOUSEMOVE || message == WM_MOUSELEAVE || message == WM_COMMAND) ? hwnd : nullptr;
    }
    ~HandoffInputScope() { active_ = previous_; }
    HandoffInputScope(const HandoffInputScope&) = delete;
    HandoffInputScope& operator=(const HandoffInputScope&) = delete;

    static bool active(HWND window) { return window && window == active_; }

private:
    static inline thread_local HWND active_ = nullptr;
    HWND previous_;
};

// Keeps the last software frame visible during a short burst of startup
// invalidation. Any unpainted native damage is restored before the next
// software paint if the GPU has yet to take over.
class SoftwareHandoffRefresh {
public:
    static constexpr UINT_PTR timer = 0x4f13;

    bool defer(HWND hwnd, SIZE size, float scale, double now) {
        if (inputSeen_ || requestedFeedback_ != paintedFeedback_ || !matches(size, scale)) return false;
        const double remaining = lastBlitMs_ + maximumFrameAgeMs_ - now;
        if (remaining <= 0.0) return false;
        if (!timerArmed_) {
            timerArmed_ = SetTimer(hwnd, timer, static_cast<UINT>(std::ceil(remaining)), nullptr) != 0;
        }
        return timerArmed_;
    }

    std::uint64_t feedbackEpoch() const { return requestedFeedback_; }
    void requestFeedback() { ++requestedFeedback_; }

    void painted(SIZE size, float scale, bool complete, double now, std::uint64_t feedbackEpoch) {
        if (!complete && !matches(size, scale)) return;
        size_ = size;
        scale_ = scale;
        lastBlitMs_ = now;
        if (feedbackEpoch > paintedFeedback_) paintedFeedback_ = feedbackEpoch;
    }

    void retainDamage(const RECT& damage) { UnionRect(&damage_, &damage_, &damage); }

    void synchronize(HWND hwnd, bool softwareStillVisible) {
        if (timerArmed_) KillTimer(hwnd, timer);
        timerArmed_ = false;
        if (softwareStillVisible && !IsRectEmpty(&damage_)) InvalidateRect(hwnd, &damage_, FALSE);
        damage_ = {};
    }

    void observeInput(HWND hwnd, UINT message) {
        if (inputSeen_ || !handoffInputMessage(message)) return;
        inputSeen_ = true;
        synchronize(hwnd, true);
    }

private:
    bool matches(SIZE size, float scale) const {
        return lastBlitMs_ != 0.0 && size_.cx == size.cx && size_.cy == size.cy && scale_ == scale;
    }

    static constexpr double maximumFrameAgeMs_ = 20.0;
    SIZE size_{};
    float scale_ = 0.0f;
    double lastBlitMs_ = 0.0;
    std::uint64_t requestedFeedback_ = 0;
    std::uint64_t paintedFeedback_ = 0;
    RECT damage_{};
    bool timerArmed_ = false;
    bool inputSeen_ = false;
};

} // namespace oneui::win32
