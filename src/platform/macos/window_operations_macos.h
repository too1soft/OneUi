#pragma once
#include "oneui/platform/window.h"
namespace oneui::macos {
WindowState nativeWindowState(void *window, WindowState dimensions);
bool restoreWindow(void *window);
bool maximizeWindow(void *window);
bool topmostWindow(void *window, bool enabled);
bool resizeWindow(void *window, Size size, float contentScale);
bool constrainWindow(void *window, Size maximum, float contentScale);
bool ownerWindow(void *window, void *parent);
bool windowPlacement(void *window, WindowPlacement &placement);
bool restorePlacement(void *window, const WindowPlacement &placement);
} // namespace oneui::macos
