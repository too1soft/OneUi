#pragma once

#include "include/core/SkRefCnt.h"
#include "oneui/canvas.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <string>

class SkCanvas;
class SkFontMgr;

namespace oneui::rendering {
struct PrimitivePaintTrace {
    std::uint64_t textCalls = 0;
    std::uint64_t textMeasureCalls = 0;
    std::uint64_t shadowCalls = 0;
    std::uint64_t gradientCalls = 0;
    double textMs = 0.0;
    double textMeasureMs = 0.0;
    double shadowMs = 0.0;
    double gradientMs = 0.0;
};

extern thread_local PrimitivePaintTrace g_primitivePaintTrace;

// Private renderer boundary. No native window or platform font types escape it.
std::unique_ptr<Canvas> makeSkiaCanvas(SkCanvas &canvas, const std::wstring *defaultFontFamily = nullptr,
                                       std::optional<Rect> viewport = std::nullopt);
sk_sp<SkFontMgr> makePlatformFontManager();
// Process-local provider populated by registerFontFromMemory(). Application
// fonts should be registered before the first window is created.
sk_sp<SkFontMgr> makeEmbeddedFontManager();
// Copies font bytes and exposes weight instances under familyAlias without
// installing anything into the operating system font directory.
bool registerFontFromMemory(const void* data, std::size_t size, const std::string& familyAlias);
} // namespace oneui::rendering
