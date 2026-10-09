#include "oneui/platform/window.h"
#include "platform/shared/desktop_window.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#undef None
#undef Status
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) { if (!ok) { ++failures; std::cerr << message << '\n'; } };
    Display* display = XOpenDisplay(nullptr);
    if (!display) return 77;
    auto atom = [&](const char* name) { return XInternAtom(display, name, False); };
    ::Window peer = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
    auto window = oneui::Window::create(L"XDND regression", 320, 200);
    window->initialize(); window->show(); window->setContentScale(2);
    auto* desktop = dynamic_cast<oneui::platform::DesktopWindow*>(window.get());
    const auto target = reinterpret_cast<std::uintptr_t>(window->nativeHandle());
    std::vector<std::wstring> received;
    oneui::Point point;
    window->setFileDropHandler([&](auto paths, auto position) { received = std::move(paths); point = position; });
    auto send = [&](const char* name, long a, long b, long c, long d) {
        XEvent event{}; event.xclient.type = ClientMessage; event.xclient.display = display;
        event.xclient.window = target; event.xclient.message_type = atom(name); event.xclient.format = 32;
        event.xclient.data.l[0] = peer; event.xclient.data.l[1] = a; event.xclient.data.l[2] = b;
        event.xclient.data.l[3] = c; event.xclient.data.l[4] = d;
        XSendEvent(display, target, False, NoEventMask, &event); XFlush(display);
    };
    auto transfer = [&](bool incremental) {
        received.clear();
        const auto settle = oneui::platform::DesktopWindow::nowMs() + 100;
        while (oneui::platform::DesktopWindow::nowMs() < settle) desktop->pumpNativeEvents(10);
        XSetSelectionOwner(display, atom("XdndSelection"), peer, CurrentTime);
        send("XdndEnter", 5L << 24, atom("text/uri-list"), 0, 0);
        int rootX = 0, rootY = 0; ::Window child = 0;
        XTranslateCoordinates(display, target, DefaultRootWindow(display), 40, 60, &rootX, &rootY, &child);
        send("XdndPosition", 0, (static_cast<unsigned long>(static_cast<unsigned short>(rootX)) << 16) |
            static_cast<unsigned short>(rootY), CurrentTime, atom("XdndActionCopy"));
        bool finished = false, started = false, chunk = false;
        const std::string data = "file:///tmp/a%20b.txt\r\nfile:///tmp/%E4%B8%AD.txt\r\n";
        const auto deadline = oneui::platform::DesktopWindow::nowMs() + 3000;
        while (!finished && oneui::platform::DesktopWindow::nowMs() < deadline) {
            desktop->pumpNativeEvents(1);
            while (XPending(display)) {
                XEvent event{}; XNextEvent(display, &event);
                if (event.type == ClientMessage && event.xclient.message_type == atom("XdndStatus")) {
                    check(event.xclient.data.l[1] & 1, "Destination rejected URI copy");
                    if (!started) { started = true; send("XdndDrop", 0, CurrentTime, 0, 0); }
                } else if (event.type == SelectionRequest) {
                    auto& request = event.xselectionrequest;
                    XSelectInput(display, target, PropertyChangeMask);
                    if (incremental) {
                        unsigned long bytes = data.size();
                        XChangeProperty(display, target, request.property, atom("INCR"), 32, PropModeReplace,
                            reinterpret_cast<const unsigned char*>(&bytes), 1);
                    } else XChangeProperty(display, target, request.property, atom("text/uri-list"), 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(data.data()), data.size());
                    XEvent reply{}; reply.xselection = {SelectionNotify, 0, False, display, target,
                        request.selection, request.target, request.property, request.time};
                    XSendEvent(display, target, False, NoEventMask, &reply); XFlush(display);
                } else if (incremental && event.type == PropertyNotify && event.xproperty.atom == atom("ONEUI_DROP") && event.xproperty.state == PropertyDelete) {
                    XChangeProperty(display, target, atom("ONEUI_DROP"), atom("text/uri-list"), 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(data.data()), chunk ? 0 : data.size());
                    chunk = true; XFlush(display);
                } else if (event.type == ClientMessage && event.xclient.message_type == atom("XdndFinished")) {
                    finished = true; check(event.xclient.data.l[1] & 1, "Destination did not acknowledge successful drop");
                }
            }
        }
        check(finished, "XDND transfer timed out");
        check(received == std::vector<std::wstring>{L"/tmp/a b.txt", L"/tmp/\u4e2d.txt"}, "XDND paths were corrupted");
        int localX = 0, localY = 0;
        XTranslateCoordinates(display, DefaultRootWindow(display), target, rootX, rootY, &localX, &localY, &child);
        check(std::abs(point.x - localX / window->dpiScale()) < 1 && std::abs(point.y - localY / window->dpiScale()) < 1,
            "Drop coordinates ignored content zoom");
    };
    transfer(false); transfer(true);
    window->setFileDropHandler({});
    Atom type = 0; int format = 0; unsigned long count = 0, remaining = 0; unsigned char* property = nullptr;
    XSync(display, False); desktop->pumpNativeEvents(0);
    XGetWindowProperty(display, target, atom("XdndAware"), 0, 1, False, AnyPropertyType, &type, &format, &count, &remaining, &property);
    check(type == 0, "Clearing the drop handler left XdndAware enabled");
    if (property) XFree(property);
    window->close(); XDestroyWindow(display, peer); XCloseDisplay(display);
    return failures ? 1 : 0;
}
