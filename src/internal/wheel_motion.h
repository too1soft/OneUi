#pragma once

#include "oneui/animation.h"

#include <algorithm>
#include <cmath>

namespace oneui::internal {

// The spring retains its position and velocity for a following wheel packet.
// Once the remaining travel is below half a device pixel, showing the exact
// target avoids redrawing text at ever-changing fractions of the same pixel.
inline float wheelPresentationOffset(const SmoothScrollMotion& motion, float previous, float dpiScale) {
    const float scale = std::isfinite(dpiScale) && dpiScale > 0.0f ? dpiScale : 1.0f;
    const float remaining = motion.target() - motion.value();
    if (std::abs(remaining) * scale <= 0.5f) return motion.target();
    // An extending packet can arrive while the spring is still slightly
    // behind its presented position. Resume without moving backward.
    return remaining > 0.0f ? std::clamp(previous, motion.value(), motion.target())
                            : std::clamp(previous, motion.target(), motion.value());
}

} // namespace oneui::internal
