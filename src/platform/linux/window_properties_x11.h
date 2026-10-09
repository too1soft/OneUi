#pragma once
#include "oneui/platform/window.h"
// Forward declarations avoid leaking Xlib's None/Status macros into SDK headers.
struct _XDisplay;
struct _XGC;
using Display = _XDisplay;
using GC = _XGC *;
using Window = unsigned long;
namespace oneui::linux_platform {
WindowState queryX11State(Display *, ::Window, WindowState dimensions);
bool queryX11Placement(Display *, ::Window, bool maximized, WindowPlacement &);
void setX11SizeConstraints(Display *, ::Window, Size minimum, Size maximum, Size current,
                           float scale, bool resizable);
void paintX11Pixels(Display *, ::Window, GC, const std::vector<std::uint32_t> &pixels, Size size);
} // namespace oneui::linux_platform
