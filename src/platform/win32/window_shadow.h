#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace oneui::win32 {

// Physical-pixel geometry, with coordinates relative to the main window.
// Kept independent of HWND/DIB allocation so edge coverage is testable.
class RoundedWindowShadow {
public:
    RoundedWindowShadow(int width, int height, float radius, float spread,
                        float drop, int maxAlpha)
        : halfWidth_(width * 0.5f), halfHeight_(height * 0.5f),
          radius_(std::max(0.0f, std::min(radius, std::min(halfWidth_, halfHeight_)))),
          spread_(spread), drop_(drop), maxAlpha_(std::clamp(maxAlpha, 0, 255)) {}

    uint8_t alphaAt(float x, float y) const {
        // The transparent cutout follows the real window, never the dropped
        // shadow. Moving both creates a strip of bare desktop below the window.
        if (spread_ <= 0.0f || distance(x, y) <= 0.0f) return 0;
        const float shadowDistance = std::max(0.0f, distance(x, y - drop_));
        if (shadowDistance >= spread_) return 0;
        const float fade = 1.0f - shadowDistance / spread_;
        return static_cast<uint8_t>(maxAlpha_ * fade * fade + 0.5f);
    }

private:
    float distance(float x, float y) const {
        const float qx = std::fabs(x - halfWidth_) - (halfWidth_ - radius_);
        const float qy = std::fabs(y - halfHeight_) - (halfHeight_ - radius_);
        const float ax = std::max(qx, 0.0f);
        const float ay = std::max(qy, 0.0f);
        return std::sqrt(ax * ax + ay * ay) + std::min(std::max(qx, qy), 0.0f) - radius_;
    }

    float halfWidth_, halfHeight_, radius_, spread_, drop_;
    int maxAlpha_;
};

} // namespace oneui::win32
