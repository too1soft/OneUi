#include "drag_drop_x11.h"
#include "x11_error_scope.h"
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <map>

namespace oneui::linux_platform {
using platform::NativeDragResult;
class X11DockDrag;
static std::map<Display *, std::shared_ptr<X11DockDrag>> active;
class X11DockDrag final : public platform::NativeDockDrag,
                          public std::enable_shared_from_this<X11DockDrag> {
  public:
    Display *display;
    ::Window id, target = 0;
    Window *window;
    std::shared_ptr<platform::DockDragCallbacks> callbacks;
    Window::EventSubscription closed;
    std::string payload;
    std::function<void(NativeDragResult, Point)> completion;
    bool accepted = false, waiting = false, released = false, dropping = false, moved = false;
    Point point{};
    Time timestamp = CurrentTime;
    double deadline = 0;
    Cursor cursor = 0;
    X11DockDrag(Window &w, Display *d, platform::DockDragCallbacks c)
        : display(d), id(reinterpret_cast<std::uintptr_t>(w.nativeHandle())), window(&w),
          callbacks(std::make_shared<platform::DockDragCallbacks>(std::move(c))) {}
    ~X11DockDrag() override {
        if (auto target = x11DropTarget(display, id))
            target->setDock({});
        if (cursor)
            XFreeCursor(display, cursor);
    }
    Atom atom(const char *name) { return XInternAtom(display, name, False); }
    bool send(const char *name, long a = 0, long b = 0, long c = 0, long e = 0) {
        if (!target)
            return false;
        ClipboardPeerErrorScope errors(display, target);
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.display = display;
        event.xclient.window = target;
        event.xclient.format = 32;
        event.xclient.message_type = atom(name);
        event.xclient.data.l[0] = id;
        event.xclient.data.l[1] = a;
        event.xclient.data.l[2] = b;
        event.xclient.data.l[3] = c;
        event.xclient.data.l[4] = e;
        XSendEvent(display, target, False, NoEventMask, &event);
        return !errors.failed();
    }
    bool start(std::string data, std::function<void(NativeDragResult, Point)> done) override {
        if (!window || active.count(display) || data.empty() || data.size() > 1024)
            return false;
        ::Window root = 0, child = 0;
        int rx = 0, ry = 0, x = 0, y = 0;
        unsigned int mask = 0;
        if (!XQueryPointer(display, id, &root, &child, &rx, &ry, &x, &y, &mask) ||
            !(mask & Button1Mask))
            return false;
        if (!cursor)
            cursor = XCreateFontCursor(display, XC_fleur);
        if (XGrabPointer(display, id, False,
                         PointerMotionMask | ButtonReleaseMask | ButtonPressMask, GrabModeAsync,
                         GrabModeAsync, 0, cursor, CurrentTime) != GrabSuccess)
            return false;
        if (XGrabKeyboard(display, id, False, GrabModeAsync, GrabModeAsync, CurrentTime) !=
            GrabSuccess) {
            XUngrabPointer(display, CurrentTime);
            return false;
        }
        XSetSelectionOwner(display, atom("XdndSelection"), id, CurrentTime);
        if (XGetSelectionOwner(display, atom("XdndSelection")) != id) {
            XUngrabPointer(display, CurrentTime);
            XUngrabKeyboard(display, CurrentTime);
            return false;
        }
        payload = std::move(data);
        completion = std::move(done);
        target = 0;
        accepted = waiting = released = dropping = moved = false;
        active[display] = shared_from_this();
        update();
        XFlush(display);
        return true;
    }
    void finish(NativeDragResult result) {
        auto keepAlive = shared_from_this();
        if (!completion)
            return;
        if (!dropping)
            send("XdndLeave");
        XUngrabPointer(display, CurrentTime);
        XUngrabKeyboard(display, CurrentTime);
        if (XGetSelectionOwner(display, atom("XdndSelection")) == id)
            XSetSelectionOwner(display, atom("XdndSelection"), 0, CurrentTime);
        active.erase(display);
        payload.clear();
        target = 0;
        auto done = std::move(completion);
        completion = {};
        try {
            done(result, point);
        } catch (...) {
        }
        XFlush(display);
    }
    void position() {
        moved = false;
        accepted = false;
        waiting = true;
        deadline = platform::DesktopWindow::nowMs() + 1500;
        const auto packed =
            (static_cast<unsigned long>(int(point.x) & 0xffff) << 16) | (int(point.y) & 0xffff);
        if (!send("XdndPosition", 0, packed, timestamp, atom("XdndActionMove")))
            finish(NativeDragResult::Failed);
    }
    void update() {
        ::Window node = DefaultRootWindow(display), root = 0, child = 0, found = 0;
        int rx = 0, ry = 0, x = 0, y = 0;
        unsigned int mask = 0;
        // Only this process's OneUI hosts may receive opaque DockSession data.
        for (int depth = 0; node && depth < 32; ++depth) {
            ClipboardPeerErrorScope errors(display, node);
            if (!XQueryPointer(display, node, &root, &child, &rx, &ry, &x, &y, &mask) ||
                errors.failed())
                break;
            if (x11DropTarget(display, node))
                found = node;
            node = child;
        }
        point = {float(rx), float(ry)};
        if (found != target) {
            send("XdndLeave");
            target = found;
            accepted = waiting = false;
            if (target && !send("XdndEnter", 5L << 24, atom(platform::dockMime))) {
                finish(NativeDragResult::Failed);
                return;
            }
        }
        if (target) {
            if (waiting)
                moved = true;
            else
                position();
        }
    }
    void drop() {
        if (!target) {
            finish(NativeDragResult::Unhandled);
            return;
        }
        if (waiting)
            return;
        if (moved) {
            position();
            return;
        }
        if (!accepted) {
            finish(NativeDragResult::Cancelled);
            return;
        }
        dropping = true;
        deadline = platform::DesktopWindow::nowMs() + 3000;
        if (!send("XdndDrop", 0, timestamp))
            finish(NativeDragResult::Failed);
    }
    bool event(XEvent &event) {
        if (event.type == SelectionRequest &&
            event.xselectionrequest.selection == atom("XdndSelection")) {
            const auto &request = event.xselectionrequest;
            ClipboardPeerErrorScope errors(display, request.requestor);
            XEvent reply{};
            reply.xselection.type = SelectionNotify;
            reply.xselection.display = display;
            reply.xselection.requestor = request.requestor;
            reply.xselection.selection = request.selection;
            reply.xselection.target = request.target;
            reply.xselection.time = request.time;
            // Selection requests must come from the destination to which Drop was sent.
            if (dropping && request.requestor == target &&
                request.target == atom(platform::dockMime)) {
                reply.xselection.property = request.property ? request.property : request.target;
                XChangeProperty(display, request.requestor, reply.xselection.property,
                                request.target, 8, PropModeReplace,
                                reinterpret_cast<const unsigned char *>(payload.data()),
                                int(payload.size()));
            }
            XSendEvent(display, request.requestor, False, NoEventMask, &reply);
            return true;
        }
        if (event.type == SelectionClear &&
            event.xselectionclear.selection == atom("XdndSelection")) {
            finish(NativeDragResult::Cancelled);
            return true;
        }
        if (event.type == ClientMessage && event.xclient.window == id &&
            event.xclient.format == 32 &&
            static_cast<::Window>(event.xclient.data.l[0]) == target) {
            if (event.xclient.message_type == atom("XdndStatus") && waiting && !dropping) {
                waiting = false;
                accepted = (event.xclient.data.l[1] & 1) &&
                           static_cast<Atom>(event.xclient.data.l[4]) == atom("XdndActionMove");
                if (released)
                    drop();
                else if (moved)
                    position();
                return true;
            }
            if (event.xclient.message_type == atom("XdndFinished") && dropping) {
                finish((event.xclient.data.l[1] & 1) &&
                               static_cast<Atom>(event.xclient.data.l[2]) == atom("XdndActionMove")
                           ? NativeDragResult::Dropped
                           : NativeDragResult::Failed);
                return true;
            }
        }
        if (event.type == KeyPress && event.xkey.window == id) {
            if (XLookupKeysym(&event.xkey, 0) == XK_Escape)
                finish(NativeDragResult::Cancelled);
            return true;
        }
        if (event.type == KeyRelease && event.xkey.window == id)
            return true;
        if (event.type == MotionNotify && !released) {
            timestamp = event.xmotion.time;
            update();
            return true;
        }
        if (event.type == ButtonPress && !released) {
            finish(NativeDragResult::Cancelled);
            return true;
        }
        if (event.type == ButtonRelease && !released && event.xbutton.button == Button1) {
            timestamp = event.xbutton.time;
            update();
            released = true;
            XUngrabPointer(display, CurrentTime);
            XUngrabKeyboard(display, CurrentTime);
            drop();
            return true;
        }
        return false;
    }
};
std::shared_ptr<platform::NativeDockDrag> makeX11DockDrag(Window &window, Display *display,
                                                          platform::DockDragCallbacks callbacks) {
    window.initialize();
    auto target = x11DropTarget(display, reinterpret_cast<std::uintptr_t>(window.nativeHandle()));
    if (!target)
        return {};
    auto result = std::make_shared<X11DockDrag>(window, display, std::move(callbacks));
    target->setDock(result->callbacks);
    result->closed = window.observeClosed([weak = std::weak_ptr<X11DockDrag>(result)] {
        if (auto self = weak.lock()) {
            self->window = nullptr;
            self->finish(NativeDragResult::Cancelled);
        }
    });
    return result;
}
bool x11DockEvent(Display *display, XEvent &event) {
    auto found = active.find(display);
    auto self = found == active.end() ? nullptr : found->second;
    return self && self->event(event);
}
bool pollX11DockDrag(Display *display) {
    auto found = active.find(display);
    if (found == active.end())
        return false;
    auto self = found->second;
    if ((self->waiting || self->dropping) && platform::DesktopWindow::nowMs() >= self->deadline)
        self->finish(NativeDragResult::Failed);
    return true;
}
std::string x11DockPayload(Display *display, ::Window source) {
    auto found = active.find(display);
    return found != active.end() && found->second->id == source ? found->second->payload
                                                                : std::string{};
}
} // namespace oneui::linux_platform
