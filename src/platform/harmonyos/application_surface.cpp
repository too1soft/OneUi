#include "platform/harmonyos/application_surface.h"
#include "platform/shared/skia_canvas.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include <chrono>
#include <cmath>

namespace oneui::mobile::harmony {
uint64_t clockNs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
ApplicationSurface::ApplicationSurface(const ApplicationHost::Factory &factory) : core(factory) {
    core.lifecycle(ONEUI_MOBILE_ACTIVE);
}
bool ApplicationSurface::viewport(const OneUiMobileViewportV1 &viewport) {
    if (!std::isfinite(viewport.width_px) || !std::isfinite(viewport.height_px) || viewport.width_px < 1 || viewport.height_px < 1 ||
        viewport.width_px > 8192 || viewport.height_px > 8192 || viewport.width_px * viewport.height_px > 16000000) return false;
    const int width = static_cast<int>(std::ceil(viewport.width_px)), height = static_cast<int>(std::ceil(viewport.height_px));
    std::vector<unsigned char> next;
    sk_sp<SkSurface> surface;
    if (width != width_ || height != height_) {
        next.resize(static_cast<size_t>(width) * height * 4);
        surface = SkSurfaces::WrapPixels(SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType), next.data(), width * 4);
        if (!surface) return false;
    }
    if (core.viewport(viewport) != ONEUI_MOBILE_OK) return false;
    if (surface) {
        core.surface(false); surface_.reset(); pixels_.swap(next); surface_ = std::move(surface);
        width_ = width; height_ = height;
    }
    scale_ = viewport.scale; core.surface(surface_ != nullptr); core.runtime().invalidate(); return true;
}
bool ApplicationSurface::paint() {
    if (!surface_) return false;
    SkAutoCanvasRestore restore(surface_->getCanvas(), false);
    std::unique_ptr<oneui::Canvas> canvas;
    auto result = core.paint([&] {
        auto *target = surface_->getCanvas(); target->save(); target->clear(SK_ColorTRANSPARENT); target->scale(scale_, scale_);
        canvas = oneui::rendering::makeSkiaCanvas(*target); return canvas.get();
    }, clockNs());
    return result == ONEUI_MOBILE_OK;
}
}
