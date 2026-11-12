#include "skia_surface_android.h"
#include "picture_raster_worker.h"
#include "oneui/frame_paint_trace.h"
#include "oneui/mobile/render_statistics.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPictureRecorder.h"
#include "platform/shared/skia_canvas.h"
#include <cmath>

namespace oneui::mobile {
namespace {
thread_local std::weak_ptr<PictureRasterWorker> observedWorker;
}

struct SkiaSurface::State {
    std::shared_ptr<PictureRasterWorker> worker =
        std::make_shared<PictureRasterWorker>(makeEglPictureBackend());
    std::unique_ptr<SkPictureRecorder> recorder;
    FramePaintTrace trace;
    std::unique_ptr<FramePaintScope> scope;
    std::uint64_t generation = 0;
    int width = 0, height = 0;
    std::string renderer, error;
};

SkiaSurface::SkiaSurface() : state_(std::make_unique<State>()) {}
SkiaSurface::~SkiaSurface() { reset(); }

void SkiaSurface::reset() {
    auto& s = *state_;
    s.scope.reset();
    s.recorder.reset();
    if (observedWorker.lock() == s.worker) observedWorker.reset();
    s.worker->reset();
    s.width = s.height = 0;
}

bool SkiaSurface::attach(ANativeWindow* acquiredWindow, int width, int height) {
    auto& s = *state_;
    s.scope.reset();
    s.recorder.reset();
    if (!s.worker->attach(acquiredWindow, width, height)) return false;
    s.width = width;
    s.height = height;
    return true;
}

std::unique_ptr<Canvas> SkiaSurface::begin(float scale) {
    auto& s = *state_;
    const auto status = s.worker->statistics();
    if (!status.attached || !status.error.empty() || s.recorder ||
        !std::isfinite(scale) || scale <= 0 || s.width <= 0 || s.height <= 0) return {};
    s.generation = status.generation;
    s.trace = {};
    s.recorder = std::make_unique<SkPictureRecorder>();
    auto* canvas = s.recorder->beginRecording(s.width, s.height);
    canvas->clear(SK_ColorWHITE);
    canvas->scale(scale, scale);
    s.scope = std::make_unique<FramePaintScope>(s.trace);
    observedWorker = s.worker;
    return rendering::makeSkiaCanvas(*canvas, nullptr,
        Rect{0, 0, s.width / scale, s.height / scale});
}

bool SkiaSurface::present() {
    auto& s = *state_;
    if (!s.recorder) return false;
    s.scope.reset();
    auto picture = s.recorder->finishRecordingAsPicture();
    s.recorder.reset();
    return s.worker->submit({std::move(picture), s.trace, s.generation});
}

const std::string& SkiaSurface::renderer() const {
    state_->renderer = state_->worker->statistics().renderer;
    return state_->renderer;
}
const std::string& SkiaSurface::error() const {
    state_->error = state_->worker->statistics().error;
    return state_->error;
}
std::uint64_t SkiaSurface::contextCount() const { return state_->worker->statistics().contexts; }
std::uint64_t SkiaSurface::resizeCount() const { return state_->worker->statistics().resizes; }

std::optional<RenderStatistics> latestThreadRenderStatistics() {
    if (auto worker = observedWorker.lock()) return worker->statistics();
    return std::nullopt;
}

} // namespace oneui::mobile
