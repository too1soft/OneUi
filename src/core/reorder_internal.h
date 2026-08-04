#pragma once

#include <algorithm>
#include <cmath>

namespace oneui::detail {

inline constexpr float kReorderDragThreshold = 4.0f;

inline bool exceedsReorderDragThreshold(float deltaX, float deltaY) {
    return deltaX * deltaX + deltaY * deltaY
        >= kReorderDragThreshold * kReorderDragThreshold;
}

inline int reorderInsertionIndex(
    float pointY,
    float frameY,
    float scrollOffset,
    float rowHeight,
    int itemCount) {
    if (itemCount <= 0 || rowHeight <= 0.0f) {
        return 0;
    }
    const float rowPosition = (pointY - frameY + scrollOffset) / rowHeight;
    const int insertion = static_cast<int>(std::floor(rowPosition + 0.5f));
    return std::clamp(insertion, 0, itemCount);
}

inline int reorderTargetIndex(int sourceIndex, int insertionIndex, int itemCount) {
    if (sourceIndex < 0 || sourceIndex >= itemCount || itemCount <= 0) {
        return -1;
    }
    int target = std::clamp(insertionIndex, 0, itemCount);
    if (target > sourceIndex) {
        --target;
    }
    return std::clamp(target, 0, itemCount - 1);
}

} // namespace oneui::detail
