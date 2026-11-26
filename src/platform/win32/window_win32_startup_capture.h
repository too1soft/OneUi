#pragma once

#include "include/core/SkCanvas.h"
#include "include/core/SkPicture.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <cstring>
#include <windows.h>

namespace oneui::win32 {

enum class WglStartupCapture { ReplayPicture, FreezeAfterSecond, DrawAndRecord };

inline WglStartupCapture wglStartupCapture() noexcept {
    static const auto mode = [] {
        wchar_t flag[2]{};
        if (GetEnvironmentVariableW(L"ONEUI_WIN32_WGL_STARTUP_CAPTURE", flag, 2) == 1) {
            if (flag[0] == L'1') return WglStartupCapture::FreezeAfterSecond;
            if (flag[0] == L'2') return WglStartupCapture::DrawAndRecord;
        }
        return WglStartupCapture::ReplayPicture;
    }();
    return mode;
}

inline bool verifyWglStartupCaptureEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t flag[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_VERIFY_STARTUP_FANOUT", flag, 2) == 1 &&
            flag[0] == L'1';
    }();
    return enabled;
}

inline bool matchesStartupReplay(SkSurface& actual, const SkPicture& picture) {
    auto replay = SkSurfaces::Raster(actual.imageInfo(), &actual.props());
    if (!replay) return false;
    replay->getCanvas()->drawPicture(&picture);
    SkPixmap a, b;
    if (!actual.peekPixels(&a) || !replay->peekPixels(&b)) return false;
    const auto bytes = a.info().minRowBytes();
    for (int row = 0; row < a.height(); ++row) {
        if (std::memcmp(a.addr(0, row), b.addr(0, row), bytes) != 0) return false;
    }
    return true;
}

} // namespace oneui::win32
