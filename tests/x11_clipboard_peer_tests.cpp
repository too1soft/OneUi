#include "platform/linux/x11_error_scope.h"
#include <X11/Xatom.h>
#include <iostream>

namespace {
int forwarded = 0;
int applicationHandler(Display*, XErrorEvent*) { ++forwarded; return 0; }
}
int main() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) return 1;
    const auto peer = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
    XDestroyWindow(display, peer);
    XSync(display, False);
    const auto previous = XSetErrorHandler(applicationHandler);
    bool caught = false;
    {
        oneui::linux_platform::ClipboardPeerErrorScope errors(display, peer);
        XDeleteProperty(display, peer, XA_PRIMARY);
        caught = errors.failed();
    }
    // After the trap, the original application handler must receive errors.
    XDeleteProperty(display, peer, XA_PRIMARY);
    XSync(display, False);
    XSetErrorHandler(previous);
    XCloseDisplay(display);
    if (!caught || forwarded != 1) {
        std::cerr << "Clipboard peer closure or X11 error-handler restoration failed\n";
        return 1;
    }
    return 0;
}
