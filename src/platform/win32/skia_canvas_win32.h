#pragma once

#include "oneui/canvas.h"

class SkPath;

namespace oneui::win32 {

SkPath toSkPath(const CanvasPath& path);

} // namespace oneui::win32
