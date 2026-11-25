#pragma once

#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace oneui::win32 {

inline constexpr int paintSurfaceAlignment = 128;

struct RetainedGpuBacking {
    sk_sp<SkSurface> surface;
    bool reused = false;
};

inline bool resizeBackingReuseEnabled() {
    static const bool enabled = [] {
        const auto* value = std::getenv("ONEUI_WIN32_REUSE_RESIZE_TARGET");
        return !value || std::string_view(value) != "0";
    }();
    return enabled;
}

inline int alignedGpuPaintExtent(int requested, int limit) {
    if (requested > limit) return requested;
    const auto blocks = (static_cast<std::int64_t>(requested) + paintSurfaceAlignment - 1) /
        paintSurfaceAlignment;
    return static_cast<int>(std::min<std::int64_t>(limit, blocks * paintSurfaceAlignment));
}

inline RetainedGpuBacking resizeGpuBacking(GrDirectContext& context, SkSurface* previous,
                                           int width, int height, bool interactive) {
    const bool liveReuse = interactive && resizeBackingReuseEnabled();
    const bool fits = previous && previous->width() >= width && previous->height() >= height;
    // Bound retained capacity after a large shrink. The window drawable still
    // uses the exact client size in both renderers.
    const auto area = static_cast<std::uint64_t>(width) * height;
    if (liveReuse && fits && static_cast<std::uint64_t>(previous->width()) * previous->height() <= 2 * area) {
        return {sk_ref_sp(previous), true};
    }

    const int limit = liveReuse && !fits ? context.maxRenderTargetSize() : 0;
    const int backingWidth = alignedGpuPaintExtent(width, limit);
    const int backingHeight = alignedGpuPaintExtent(height, limit);
    const auto allocate = [&](int w, int h) {
        const auto info = SkImageInfo::Make(w, h, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        return SkSurfaces::RenderTarget(&context, skgpu::Budgeted::kYes, info, 0,
            kTopLeft_GrSurfaceOrigin, nullptr);
    };
    auto surface = allocate(backingWidth, backingHeight);
    if (!surface && (backingWidth != width || backingHeight != height)) surface = allocate(width, height);
    return {std::move(surface), false};
}

} // namespace oneui::win32
