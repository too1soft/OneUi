#pragma once

#include "oneui/canvas.h"

class SkPath;

namespace oneui::rendering {

SkPath toSkPath(const CanvasPath &path);

} // namespace oneui::rendering
