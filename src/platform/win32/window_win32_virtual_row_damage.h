#pragma once

#include "internal/scroll_trace.h"
#include "internal/virtual_row_paint.h"

#include <atomic>
#include <memory>
#include <windows.h>

namespace oneui::win32 {

class VirtualRowDamage {
public:
    static bool enabled() {
        static const bool value = [] {
            wchar_t flag[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_FOLD_FIRST_VIRTUAL_ROW_DAMAGE", flag, 2) == 1
                && flag[0] == L'1';
        }();
        return value;
    }

    VirtualRowDamage(HWND window, HDC dc, RECT client, std::shared_ptr<std::atomic<bool>> alive)
        : scope_(internal::VirtualRowPaint::fullClientPaint, true), previous_(std::exchange(active_, this)),
          window_(window), dc_(dc), client_(client), alive_(std::move(alive)) {}

    ~VirtualRowDamage() {
        active_ = previous_;
        if (held_ && !presented_ && alive_->load(std::memory_order_acquire)) {
            InvalidateRect(window_, nullptr, FALSE);
        }
    }

    VirtualRowDamage(const VirtualRowDamage&) = delete;
    VirtualRowDamage& operator=(const VirtualRowDamage&) = delete;

    void presented() { presented_ = true; }

    static bool hold(const void* owner, HWND window) {
        if (internal::VirtualRowPaint::damageOwner != owner || !active_ || active_->window_ != window) return false;
        auto& frame = *active_;
        RECT clip{};
        // A bounding update rectangle alone allows holes in a complex GDI clip.
        const auto region = GetClipBox(frame.dc_, &clip);
        const bool full = region == SIMPLEREGION && EqualRect(&clip, &frame.client_);
        if (internal::scrollTraceEnabled()) {
            internal::ScrollTraceEvent event{"virtual_rows", full ? "held_damage" : "preserved_damage",
                reinterpret_cast<std::uintptr_t>(window)};
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
    bool held_ = false;
    bool presented_ = false;
};

} // namespace oneui::win32
