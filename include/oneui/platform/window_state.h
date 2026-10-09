#pragma once
#include "oneui/geometry.h"
#include <cstdint>
#include <functional>

namespace oneui {
enum class WindowStateField : std::uint32_t {
    Visible = 1,
    Active = 2,
    Minimized = 4,
    Maximized = 8,
    Fullscreen = 16,
    Topmost = 32,
    Resizable = 64
};
struct WindowState {
    // Fields absent from knownFields are unknown, not false.
    std::uint32_t knownFields = 0;
    bool visible = false, active = false, minimized = false, maximized = false;
    bool fullscreen = false, topmost = false, resizable = true;
    Size clientSize{}, pixelSize{};
    float dpiScale = 1;
};
} // namespace oneui
