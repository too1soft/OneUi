#include "oneui/platform/window.h"

#include <windows.h>

#include <cstring>
#include <mutex>

namespace oneui {
namespace {

LRESULT CALLBACK clipboardOwnerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

HWND clipboardOwnerWindow() {
    static std::once_flag once;
    static HWND owner = nullptr;
    std::call_once(once, [] {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        constexpr wchar_t className[] = L"OneUIClipboardOwner";
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = &clipboardOwnerProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        RegisterClassW(&windowClass);
        owner = CreateWindowExW(
            0, className, L"", WS_OVERLAPPED, 0, 0, 0, 0,
            nullptr, nullptr, instance, nullptr);
    });
    return owner;
}

class ClipboardGuard {
public:
    ClipboardGuard() {
        constexpr int kMaxAttempts = 8;
        constexpr DWORD kRetryDelayMs = 5;
        const HWND owner = clipboardOwnerWindow();
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            if (OpenClipboard(owner) != FALSE) {
                open_ = true;
                return;
            }
            if (attempt + 1 < kMaxAttempts) {
                Sleep(kRetryDelayMs);
            }
        }
    }

    ~ClipboardGuard() {
        if (open_) {
            CloseClipboard();
        }
    }

    bool isOpen() const { return open_; }

private:
    bool open_ = false;
};

} // namespace

void SystemClipboard::setText(std::wstring text) {
    ClipboardGuard guard;
    if (!guard.isOpen() || !EmptyClipboard()) {
        return;
    }

    const SIZE_T byteCount = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, byteCount);
    if (!memory) {
        return;
    }
    void* locked = GlobalLock(memory);
    if (!locked) {
        GlobalFree(memory);
        return;
    }
    std::memcpy(locked, text.c_str(), byteCount);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
    }
}

std::wstring SystemClipboard::text() const {
    ClipboardGuard guard;
    if (!guard.isOpen()) {
        return {};
    }
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) {
        return {};
    }
    const wchar_t* locked = static_cast<const wchar_t*>(GlobalLock(handle));
    if (!locked) {
        return {};
    }
    std::wstring result(locked);
    GlobalUnlock(handle);
    return result;
}

} // namespace oneui
