#include "window_operations_macos.h"
#import <AppKit/AppKit.h>
#include <cmath>

namespace oneui::macos {
WindowState nativeWindowState(void *handle, WindowState value) {
    NSWindow *window = (__bridge NSWindow *)handle;
    if (!window)
        return value;
    value.knownFields = 127;
    value.visible = window.visible;
    value.active = window.keyWindow;
    value.minimized = window.miniaturized;
    value.maximized = window.zoomed;
    value.fullscreen = bool(window.styleMask & NSWindowStyleMaskFullScreen);
    value.topmost = window.level >= NSFloatingWindowLevel;
    value.resizable = bool(window.styleMask & NSWindowStyleMaskResizable);
    return value;
}
bool restoreWindow(void *handle) {
    NSWindow *window = (__bridge NSWindow *)handle;
    if (!window)
        return false;
    if (window.miniaturized)
        [window deminiaturize:nil];
    if (window.styleMask & NSWindowStyleMaskFullScreen)
        [window toggleFullScreen:nil];
    else if (window.zoomed)
        [window zoom:nil];
    [window makeKeyAndOrderFront:nil];
    return true;
}
bool maximizeWindow(void *handle) {
    NSWindow *window = (__bridge NSWindow *)handle;
    if (!window)
        return false;
    if (window.miniaturized)
        [window deminiaturize:nil];
    if (!window.zoomed)
        [window zoom:nil];
    return true;
}
bool topmostWindow(void *handle, bool enabled) {
    NSWindow *window = (__bridge NSWindow *)handle;
    if (!window)
        return false;
    window.level = enabled ? NSFloatingWindowLevel : NSNormalWindowLevel;
    return true;
}
bool resizeWindow(void *handle, Size size, float scale) {
    NSWindow *window = (__bridge NSWindow *)handle;
    if (!window || !std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0 ||
        size.height <= 0 || size.width > 32767 || size.height > 32767)
        return false;
    window.contentSize = NSMakeSize(size.width * scale, size.height * scale);
    return true;
}
bool constrainWindow(void *handle, Size size, float scale) {
    NSWindow *window = (__bridge NSWindow *)handle;
    if (!window || !std::isfinite(size.width) || !std::isfinite(size.height))
        return false;
    window.contentMaxSize = NSMakeSize(size.width > 0 ? size.width * scale : CGFLOAT_MAX,
                                       size.height > 0 ? size.height * scale : CGFLOAT_MAX);
    return true;
}
bool ownerWindow(void *handle, void *owner) {
    NSWindow *window = (__bridge NSWindow *)handle;
    NSWindow *parent = (__bridge NSWindow *)owner;
    if (!window || parent == window)
        return false;
    for (NSWindow *ancestor = parent; ancestor; ancestor = ancestor.parentWindow)
        if (ancestor == window)
            return false;
    if (window.parentWindow)
        [window.parentWindow removeChildWindow:window];
    if (parent)
        [parent addChildWindow:window ordered:NSWindowAbove];
    return true;
}
bool windowPlacement(void *handle, WindowPlacement &p) {
    NSWindow *window = (__bridge NSWindow *)handle;
    if (!window)
        return false;
    NSRect r = window.frame;
    p = {int(r.origin.x), int(r.origin.y), int(r.size.width), int(r.size.height),
         bool(window.zoomed)};
    return true;
}
bool restorePlacement(void *handle, const WindowPlacement &p) {
    NSWindow *window = (__bridge NSWindow *)handle;
    if (!window || p.width <= 0 || p.height <= 0)
        return false;
    NSRect frame = NSMakeRect(p.x, p.y, p.width, p.height);
    bool intersects = false;
    for (NSScreen *screen in NSScreen.screens)
        intersects |= NSIntersectsRect(frame, screen.visibleFrame);
    if (!intersects)
        return false;
    [window setFrame:frame display:YES];
    if (bool(window.zoomed) != p.maximized)
        [window zoom:nil];
    return true;
}
} // namespace oneui::macos
