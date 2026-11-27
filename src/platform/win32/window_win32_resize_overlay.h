#pragma once

#include <windows.h>
#include <algorithm>

namespace oneui::win32 {

enum class ResizeOverlayGeometry { Exact, ExactWithoutRedraw, ReuseCapacity };

inline ResizeOverlayGeometry resizeOverlayGeometry() noexcept {
    static const auto geometry = [] {
        wchar_t value[2]{};
        if (GetEnvironmentVariableW(L"ONEUI_WIN32_WGL_RESIZE_OVERLAY_GEOMETRY", value, 2) == 1) {
            if (value[0] == L'0') return ResizeOverlayGeometry::Exact;
            if (value[0] == L'1') return ResizeOverlayGeometry::ExactWithoutRedraw;
        }
        return ResizeOverlayGeometry::ReuseCapacity;
    }();
    return geometry;
}

// GDI and WGL use separate window drawables during interactive resizing.
class WglResizeOverlay {
public:
    struct Target {
        HWND window = nullptr;
        bool positioned = false;
    };

    ~WglResizeOverlay() { close(); }
    WglResizeOverlay() = default;
    WglResizeOverlay(const WglResizeOverlay&) = delete;
    WglResizeOverlay& operator=(const WglResizeOverlay&) = delete;

    Target prepare(HWND parent, int width, int height, ResizeOverlayGeometry geometry) {
        if (geometry == ResizeOverlayGeometry::ReuseCapacity) {
            // A child is clipped to its parent's client area. The blit still
            // covers the exact viewport, independent of the child's allocation.
            width = std::max(width_, width);
            height = std::max(height_, height);
        }
        if (!window_) {
            if (!registered()) return {};
            window_ = CreateWindowExW(0, className_, L"", WS_CHILD | WS_DISABLED | WS_CLIPSIBLINGS,
                                      0, 0, width, height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (!window_) return {};
            parent_ = parent;
        }
        if (width != width_ || height != height_) {
            UINT flags = SWP_NOACTIVATE | SWP_SHOWWINDOW;
            // The child covers the whole client area and is redrawn by the
            // caller immediately. A native redraw would also dirty the parent.
            if (geometry != ResizeOverlayGeometry::Exact) flags |= SWP_NOREDRAW;
            if (!SetWindowPos(window_, HWND_TOP, 0, 0, width, height, flags)) {
                close();
                return {};
            }
            width_ = width;
            height_ = height;
            return {window_, true};
        }
        return {window_, false};
    }

    void close() {
        if (window_ && GetParent(window_) == parent_) DestroyWindow(window_);
        window_ = nullptr;
        parent_ = nullptr;
        width_ = 0;
        height_ = 0;
    }

private:
    inline static constexpr auto className_ = L"OneUI.WglResizeOverlay";

    static bool registered() {
        static const bool ok = [] {
            WNDCLASSW cls{};
            cls.lpfnWndProc = procedure;
            cls.hInstance = GetModuleHandleW(nullptr);
            cls.lpszClassName = className_;
            return RegisterClassW(&cls) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }();
        return ok;
    }

    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCHITTEST) return HTTRANSPARENT;
        if (message == WM_ERASEBKGND) return 1;
        if (message == WM_PAINT) {
            ValidateRect(window, nullptr);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    HWND window_ = nullptr;
    HWND parent_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace oneui::win32
