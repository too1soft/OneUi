#pragma once
#include <algorithm>
namespace oneui::platform {
// Native window frame or monitor work area, in the backend's own units
// (Win32 physical pixels, AppKit points). Only extents and offsets matter.
struct WindowFitRect {
    double x = 0, y = 0, width = 0, height = 0;
};

// Shrinks extents that exceed the work area, then slides the origin inside it.
inline WindowFitRect fitWindowRect(WindowFitRect frame, const WindowFitRect& area) {
    if (area.width <= 0 || area.height <= 0) return frame;
    frame.width = std::clamp(frame.width, 1.0, area.width);
    frame.height = std::clamp(frame.height, 1.0, area.height);
    frame.x = std::clamp(frame.x, area.x, area.x + area.width - frame.width);
    frame.y = std::clamp(frame.y, area.y, area.y + area.height - frame.height);
    return frame;
}

inline bool windowExceedsArea(const WindowFitRect& frame, const WindowFitRect& area) {
    return area.width > 0 && area.height > 0 && (frame.width > area.width || frame.height > area.height);
}

// A requested first-show size larger than the screen (1600x1000 on a 1440x900
// point MacBook) would open partly off screen. Oversized extents shrink to a
// share of the work area so the result does not read as maximized; extents
// that already fit are kept. The frame is centered in the work area.
inline WindowFitRect initialWindowRect(WindowFitRect frame, const WindowFitRect& area) {
    constexpr double share = 0.9;
    if (!windowExceedsArea(frame, area)) return frame;
    if (frame.width > area.width) frame.width = std::max(1.0, area.width * share);
    if (frame.height > area.height) frame.height = std::max(1.0, area.height * share);
    frame.x = area.x + (area.width - frame.width) / 2;
    frame.y = area.y + (area.height - frame.height) / 2;
    return frame;
}
} // namespace oneui::platform
