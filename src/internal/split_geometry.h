#pragma once
#include "oneui/geometry.h"
#include <algorithm>
#include <array>

namespace oneui::detail {
enum class SplitShortage { PreserveRatio, MinimumShare };
inline float splitAmount(float available, float ratio, float a, float b,
                         SplitShortage shortage = SplitShortage::PreserveRatio) {
    if (a + b > available)
        return shortage == SplitShortage::MinimumShare && a + b > 0 ? available * (a / (a + b))
                                                                    : available * ratio;
    return std::clamp(available * ratio, a, available - b);
}
inline Size splitMinimum(Size a, Size b, float gap, bool horizontal) {
    return horizontal ? Size{a.width + b.width + gap, std::max(a.height, b.height)}
                      : Size{std::max(a.width, b.width), a.height + b.height + gap};
}
struct SplitRects {
    Rect first, second, divider;
};
inline SplitRects splitRects(Rect bounds, float gap, float amount, bool horizontal) {
    SplitRects r{bounds, bounds, bounds};
    if (horizontal) {
        r.first.width = amount;
        r.second.x += amount + gap;
        r.second.width -= amount + gap;
        r.divider.x += amount;
        r.divider.width = gap;
    } else {
        r.first.height = amount;
        r.second.y += amount + gap;
        r.second.height -= amount + gap;
        r.divider.y += amount;
        r.divider.height = gap;
    }
    return r;
}
// Stable L/R/T/B tie break; used by pane and tab-out edge placement.
inline size_t nearestNormalizedEdge(float x, float y) {
    const std::array<float, 4> distance{x, 1 - x, y, 1 - y};
    return size_t(std::min_element(distance.begin(), distance.end()) - distance.begin());
}
} // namespace oneui::detail
