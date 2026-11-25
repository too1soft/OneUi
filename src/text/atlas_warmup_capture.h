#pragma once

#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkTextBlob.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace oneui::text {

struct AtlasWarmupRun {
    sk_sp<SkTextBlob> blob;
    SkScalar scaleX;
    SkScalar scaleY;
    SkScalar verticalPhase;
    SkPaint paint;
};

class AtlasWarmupCapture {
public:
    using Runs = std::vector<AtlasWarmupRun>;

    explicit AtlasWarmupCapture(Runs* runs) : previous_(active_) { active_ = runs; }
    ~AtlasWarmupCapture() { active_ = previous_; }
    AtlasWarmupCapture(const AtlasWarmupCapture&) = delete;
    AtlasWarmupCapture& operator=(const AtlasWarmupCapture&) = delete;

    static void observe(SkCanvas& canvas, const sk_sp<SkTextBlob>& blob, SkScalar x,
                        SkScalar y, const SkPaint& paint) {
        auto* runs = active_;
        if (!runs || runs->size() >= 96 || !blob || paint.getStyle() != SkPaint::kFill_Style ||
            paint.getMaskFilter() || paint.getPathEffect() || paint.getImageFilter() || paint.getShader() ||
            paint.getColorFilter() || paint.getBlender()) return;
        auto bounds = blob->bounds();
        bounds.offset(x, y);
        if (canvas.quickReject(bounds)) return;
        const auto matrix = canvas.getLocalToDeviceAs3x3();
        if (!matrix.isScaleTranslate() || matrix.getScaleX() <= 0 || matrix.getScaleY() <= 0) return;
        const auto device = matrix.mapPoint({x, y});
        const auto phase = device.y() - std::floor(device.y());
        const auto scaleX = matrix.getScaleX();
        const auto scaleY = matrix.getScaleY();
        if (std::any_of(runs->begin(), runs->end(), [&](const AtlasWarmupRun& run) {
                return run.blob == blob && run.scaleX == scaleX && run.scaleY == scaleY &&
                    run.verticalPhase == phase && run.paint == paint;
            })) return;
        runs->push_back({blob, scaleX, scaleY, phase, paint});
    }

private:
    inline static thread_local Runs* active_ = nullptr;
    Runs* previous_;
};

} // namespace oneui::text
