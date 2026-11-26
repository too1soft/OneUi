#pragma once

#include "internal/scroll_trace.h"
#include "internal/virtual_row_paint.h"

#include <atomic>
#include <memory>
#include <windows.h>

namespace oneui::win32 {

class VirtualRowDamage {
public:
    static bool softwareEnabled() {
        static const bool value = [] {
            wchar_t flag[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_FOLD_FIRST_VIRTUAL_ROW_DAMAGE", flag, 2) == 1
                && flag[0] == L'1';
        }();
        return value;
    }

    static bool directWglEnabled() {
        static const bool value = [] {
            wchar_t flag[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_FOLD_FIRST_VIRTUAL_ROW_DAMAGE_WGL", flag, 2) == 1
                && flag[0] == L'1';
        }();
        return value;
    }

    // An exact full GPU frame covers the client. Software presentation still
    // requires the actual GDI clip.
    VirtualRowDamage(HWND window, HDC dc, RECT client, std::shared_ptr<std::atomic<bool>> alive,
                     bool worker = false)
        : scope_(internal::VirtualRowPaint::fullClientPaint, true), previous_(std::exchange(active_, this)),
          window_(window), dc_(dc), client_(client), alive_(std::move(alive)), worker_(worker) {}

    ~VirtualRowDamage() {
        active_ = previous_;
        if (held_ && !covered_ && alive_->load(std::memory_order_acquire)) {
            const bool restored = InvalidateRect(window_, nullptr, FALSE) != FALSE;
            if (internal::scrollTraceEnabled()) {
                internal::ScrollTraceEvent event{"virtual_rows", "restored_damage",
                    reinterpret_cast<std::uintptr_t>(window_)};
                event.detailA = restored ? 1.0 : 0.0;
                internal::writeScrollTrace(event);
            }
        }
    }

    VirtualRowDamage(const VirtualRowDamage&) = delete;
    VirtualRowDamage& operator=(const VirtualRowDamage&) = delete;

    void covered() { covered_ = true; }

    static bool hold(const void* owner, HWND window) {
        if (internal::VirtualRowPaint::damageOwner != owner || !active_ || active_->window_ != window) return false;
        auto& frame = *active_;
        RECT clip{};
        // A bounding update rectangle alone allows holes in a complex GDI clip.
        const auto region = frame.dc_ ? GetClipBox(frame.dc_, &clip) : 0;
        const bool full = !frame.dc_ || (region == SIMPLEREGION && EqualRect(&clip, &frame.client_));
        if (internal::scrollTraceEnabled()) {
            const char* phase = frame.worker_ ? "held_worker_damage" :
                !frame.dc_ ? "held_wgl_damage" : full ? "held_damage" : "preserved_damage";
            internal::ScrollTraceEvent event{"virtual_rows", phase, reinterpret_cast<std::uintptr_t>(window)};
            event.detailA = region;
            internal::writeScrollTrace(event);
        }
        frame.held_ |= full;
        return full;
    }

private:
    static inline thread_local VirtualRowDamage* active_ = nullptr;
    internal::VirtualRowPaint::Scope<bool> scope_;
    VirtualRowDamage* previous_;
    HWND window_;
    HDC dc_;
    RECT client_;
    std::shared_ptr<std::atomic<bool>> alive_;
    bool worker_;
    bool held_ = false;
    bool covered_ = false;
};

} // namespace oneui::win32
