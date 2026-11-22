#pragma once

#include "oneui/geometry.h"

#include <cstdint>

namespace oneui::internal {

inline bool hasAccessibleFrame(Rect frame) {
    return !(frame.width <= 0 || frame.height <= 0);
}

// Frame assignment can change enumeration without invalidating measurement.
// A UI thread shares this revision across its windows. Moving or resizing an
// already allocated widget keeps its child ID and live bounds.
inline thread_local std::uint64_t accessibilityFrameRevision = 0;

} // namespace oneui::internal
