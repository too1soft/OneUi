#pragma once
// Private experiment bridge. Only the performance-lab build enables/exports it.
#include "oneui/canvas.h"
namespace oneui::rendering::experimental {
struct CircleMeshResult {
    bool drawn = false;
    const char* reason = "unsupported-canvas";
    std::size_t vertices = 0;
};
ONEUI_API CircleMeshResult tryCircleMesh(Canvas&, const RoundedRectFill*, std::size_t);
}
