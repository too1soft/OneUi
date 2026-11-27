#pragma once

#include <windows.h>

namespace oneui::win32 {

// GDI and WGL use separate window drawables during interactive resizing.
class WglResizeOverlay {
public:
    ~WglResizeOverlay() { close(); }
    WglResizeOverlay() = default;
    WglResizeOverlay(const WglResizeOverlay&) = delete;
    WglResizeOverlay& operator=(const WglResizeOverlay&) = delete;

    HWND prepare(HWND parent, int width, int height) {
        if (!window_) {
            if (!registered()) return nullptr;
            window_ = CreateWindowExW(0, className_, L"", WS_CHILD | WS_DISABLED | WS_CLIPSIBLINGS,
                                      0, 0, width, height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (!window_) return nullptr;
            parent_ = parent;
        }
        if (width != width_ || height != height_) {
            if (!SetWindowPos(window_, HWND_TOP, 0, 0, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW)) {
                close();
                return nullptr;
            }
            width_ = width;
            height_ = height;
        }
        return window_;
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
