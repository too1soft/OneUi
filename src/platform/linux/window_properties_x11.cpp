#include "window_properties_x11.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <stdexcept>
#undef None
#undef Status

namespace oneui::linux_platform {
WindowState queryX11State(Display *display, ::Window window, WindowState value) {
    if (!window)
        return value;
    XWindowAttributes attributes{};
    if (!XGetWindowAttributes(display, window, &attributes))
        return value;
    value.knownFields = 127;
    value.visible = attributes.map_state != IsUnmapped;
    ::Window focus = 0;
    int revert = 0;
    XGetInputFocus(display, &focus, &revert);
    value.active = focus == window;
    Atom type = 0;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char *bytes = nullptr;
    if (XGetWindowProperty(display, window, XInternAtom(display, "_NET_WM_STATE", False), 0, 128,
                           False, XA_ATOM, &type, &format, &count, &after, &bytes) == Success &&
        bytes && format == 32) {
        auto states = reinterpret_cast<Atom *>(bytes);
        const auto has = [&](const char *name) {
            return std::find(states, states + count, XInternAtom(display, name, False)) !=
                   states + count;
        };
        value.fullscreen = has("_NET_WM_STATE_FULLSCREEN");
        value.maximized =
            has("_NET_WM_STATE_MAXIMIZED_HORZ") && has("_NET_WM_STATE_MAXIMIZED_VERT");
        value.topmost = has("_NET_WM_STATE_ABOVE");
        value.minimized = has("_NET_WM_STATE_HIDDEN");
    }
    if (bytes)
        XFree(bytes);
    XSizeHints hints{};
    long supplied = 0;
    if (XGetWMNormalHints(display, window, &hints, &supplied))
        value.resizable = !(hints.flags & PMinSize) || !(hints.flags & PMaxSize) ||
                          hints.min_width != hints.max_width ||
                          hints.min_height != hints.max_height;
    return value;
}
bool queryX11Placement(Display *display, ::Window window, bool maximized, WindowPlacement &p) {
    if (!window)
        return false;
    XWindowAttributes a{};
    if (!XGetWindowAttributes(display, window, &a))
        return false;
    int x = 0, y = 0;
    ::Window child = 0;
    if (!XTranslateCoordinates(display, window, DefaultRootWindow(display), 0, 0, &x, &y, &child))
        return false;
    p = {x, y, a.width, a.height, maximized};
    return true;
}
void setX11SizeConstraints(Display *display, ::Window window, Size minimum, Size maximum,
                           Size current, float scale, bool resizable) {
    if (!window)
        return;
    XSizeHints hints{};
    hints.flags = PMinSize;
    hints.min_width = std::max(1, int(minimum.width * scale));
    hints.min_height = std::max(1, int(minimum.height * scale));
    if (maximum.width > 0 || maximum.height > 0) {
        hints.flags |= PMaxSize;
        hints.max_width =
            std::max(hints.min_width, maximum.width > 0 ? int(maximum.width * scale) : 32767);
        hints.max_height =
            std::max(hints.min_height, maximum.height > 0 ? int(maximum.height * scale) : 32767);
    }
    if (!resizable) {
        hints.flags |= PMaxSize;
        hints.min_width = hints.max_width = int(current.width);
        hints.min_height = hints.max_height = int(current.height);
    }
    XSetWMNormalHints(display, window, &hints);
}
void paintX11Pixels(Display *display, ::Window window, GC gc,
                    const std::vector<std::uint32_t> &pixels, Size size) {
    auto *image = XCreateImage(display, DefaultVisual(display, DefaultScreen(display)),
                               DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0,
                               reinterpret_cast<char *>(const_cast<std::uint32_t *>(pixels.data())),
                               size.width, size.height, 32, int(size.width) * 4);
    if (!image)
        throw std::runtime_error("OneUI XCreateImage failed");
    if (image->bits_per_pixel != 32 || image->byte_order != LSBFirst ||
        image->red_mask != 0xFF0000 || image->green_mask != 0xFF00 || image->blue_mask != 0xFF) {
        image->data = nullptr;
        XDestroyImage(image);
        throw std::runtime_error("OneUI X11 requires a 32-bit BGRA-compatible TrueColor visual");
    }
    XPutImage(display, window, gc, image, 0, 0, 0, 0, size.width, size.height);
    image->data = nullptr;
    XDestroyImage(image);
}
} // namespace oneui::linux_platform
