#include "drag_drop_x11.h"
#include "platform/shared/file_drop.h"
#include "x11_error_scope.h"
#include <X11/Xatom.h>
#include <cstring>
#include <map>

namespace oneui::linux_platform {
static std::map<std::pair<Display *, ::Window>, X11DropTarget *> targets;
X11DropTarget *x11DropTarget(Display *display, ::Window id) {
    auto found = targets.find({display, id});
    return found == targets.end() ? nullptr : found->second;
}
X11DropTarget::X11DropTarget(Display *d, ::Window id, platform::DesktopWindow &window)
    : display_(d), id_(id), window_(window) {
    targets[{d, id}] = this;
}
X11DropTarget::~X11DropTarget() {
    cancel();
    targets.erase({display_, id_});
}
Atom X11DropTarget::atom(const char *name) const { return XInternAtom(display_, name, False); }
void X11DropTarget::advertise() {
    if (filesEnabled_ || !dock_.expired()) {
        unsigned long version = 5;
        XChangeProperty(display_, id_, atom("XdndAware"), XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char *>(&version), 1);
    } else {
        XDeleteProperty(display_, id_, atom("XdndAware"));
        cancel();
    }
}
void X11DropTarget::enableFiles(bool enabled) {
    filesEnabled_ = enabled;
    advertise();
}
void X11DropTarget::setDock(std::weak_ptr<platform::DockDragCallbacks> dock) {
    dock_ = std::move(dock);
    advertise();
}
void X11DropTarget::send(const char *type, long flags, Atom action) {
    if (!source_)
        return;
    ClipboardPeerErrorScope errors(display_, source_);
    XEvent reply{};
    reply.xclient.type = ClientMessage;
    reply.xclient.display = display_;
    reply.xclient.window = source_;
    reply.xclient.message_type = atom(type);
    reply.xclient.format = 32;
    reply.xclient.data.l[0] = id_;
    reply.xclient.data.l[1] = flags;
    reply.xclient.data.l[std::strcmp(type, "XdndStatus") == 0 ? 4 : 2] = action;
    XSendEvent(display_, source_, False, NoEventMask, &reply);
}
void X11DropTarget::cancel() {
    finish(false);
    if (auto dock = dock_.lock())
        if (dock->leave)
            dock->leave();
}
void X11DropTarget::finish(bool success) {
    auto files =
        success && !private_ ? platform::filePathsFromUriList(bytes_) : std::vector<std::wstring>{};
    const auto action = atom(private_ ? "XdndActionMove" : "XdndActionCopy");
    if (private_) {
        auto dock = dock_.lock();
        success = success && !window_.nativeModal() && dock && dock->drop;
        if (success)
            try {
                success = dock->drop(bytes_, point_, true);
            } catch (...) {
                success = false;
            }
    } else
        success = success && window_.acceptsFileDrop() && !files.empty();
    if (pending_)
        send("XdndFinished", success ? 1 : 0, success ? action : 0);
    auto point = point_;
    pending_ = incremental_ = accepted_ = files_ = private_ = false;
    source_ = 0;
    bytes_.clear();
    if (success && !files.empty())
        window_.deliverFileDropClient(std::move(files), point);
}
void X11DropTarget::read() {
    Atom type = 0;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(display_, id_, atom("ONEUI_DROP"), 0, 1024 * 1024, True, AnyPropertyType,
                           &type, &format, &count, &remaining, &data) != Success) {
        finish(false);
        return;
    }
    if (type == atom("INCR") && !incremental_ && format == 32) {
        incremental_ = true;
        if (data)
            XFree(data);
        return;
    }
    const auto limit = private_ ? 1024u : 4u * 1024u * 1024u;
    const bool valid = type == atom(private_ ? platform::dockMime : "text/uri-list") &&
                       format == 8 && !remaining && bytes_.size() + count <= limit;
    if (valid && count)
        bytes_.append(reinterpret_cast<char *>(data), count);
    if (data)
        XFree(data);
    if (!valid)
        finish(false);
    else if (!incremental_ || !count)
        finish(true);
}
bool X11DropTarget::event(XEvent &event) {
    if (event.type == ClientMessage)
        return message(event.xclient);
    if (event.type == SelectionNotify && event.xselection.selection == atom("XdndSelection")) {
        if (pending_) {
            if (!event.xselection.property)
                finish(false);
            else if (event.xselection.property == atom("ONEUI_DROP"))
                read();
        }
        return true;
    }
    if (event.type == PropertyNotify && pending_ && incremental_ &&
        event.xproperty.atom == atom("ONEUI_DROP") && event.xproperty.state == PropertyNewValue) {
        read();
        return true;
    }
    return false;
}
bool X11DropTarget::expire() {
    if (pending_ && platform::DesktopWindow::nowMs() >= deadline_)
        finish(false);
    return pending_;
}
bool X11DropTarget::message(const XClientMessageEvent &event) {
    if (event.format != 32)
        return false;
    const auto message = event.message_type;
    if (message == atom("XdndEnter")) {
        cancel();
        source_ = event.data.l[0];
        const auto version = static_cast<unsigned long>(event.data.l[1]) >> 24;
        if (version < 3 || window_.nativeModal())
            return true;
        auto accept = [&](Atom type) {
            files_ |= type == atom("text/uri-list") && window_.acceptsFileDrop();
            private_ |= type == atom(platform::dockMime) && !dock_.expired() &&
                        !x11DockPayload(display_, source_).empty();
        };
        if (!(event.data.l[1] & 1)) {
            for (int i = 2; i < 5; ++i)
                accept(event.data.l[i]);
        } else {
            ClipboardPeerErrorScope errors(display_, source_);
            Atom type = 0;
            int format = 0;
            unsigned long count = 0, remaining = 0;
            unsigned char *data = nullptr;
            if (XGetWindowProperty(display_, source_, atom("XdndTypeList"), 0, 1024, False, XA_ATOM,
                                   &type, &format, &count, &remaining, &data) == Success &&
                type == XA_ATOM && format == 32 && !remaining)
                for (unsigned long i = 0; i < count; ++i)
                    accept(reinterpret_cast<Atom *>(data)[i]);
            if (data)
                XFree(data);
        }
        return true;
    }
    if (message != atom("XdndPosition") && message != atom("XdndLeave") &&
        message != atom("XdndDrop"))
        return false;
    if (static_cast<::Window>(event.data.l[0]) != source_)
        return true;
    if (message == atom("XdndLeave")) {
        cancel();
        return true;
    }
    if (message == atom("XdndPosition")) {
        int x = 0, y = 0;
        ::Window child = 0;
        const auto packed = static_cast<unsigned long>(event.data.l[2]);
        XTranslateCoordinates(display_, DefaultRootWindow(display_), id_,
                              static_cast<short>(packed >> 16), static_cast<short>(packed & 0xffff),
                              &x, &y, &child);
        point_ = {x / window_.dpiScale(), y / window_.dpiScale()};
        const auto action = atom(private_ ? "XdndActionMove" : "XdndActionCopy");
        accepted_ = !window_.nativeModal() && static_cast<Atom>(event.data.l[4]) == action;
        if (private_) {
            auto dock = dock_.lock();
            accepted_ = accepted_ && dock && dock->drop;
            if (accepted_)
                try {
                    accepted_ = dock->drop(x11DockPayload(display_, source_), point_, false);
                } catch (...) {
                    accepted_ = false;
                }
        } else
            accepted_ = accepted_ && files_ && window_.acceptsFileDrop();
        send("XdndStatus", accepted_ ? 3 : 2, accepted_ ? action : 0);
    } else {
        pending_ = true;
        if (!accepted_ || window_.nativeModal()) {
            finish(false);
            return true;
        }
        deadline_ = platform::DesktopWindow::nowMs() + 3000;
        XConvertSelection(display_, atom("XdndSelection"),
                          atom(private_ ? platform::dockMime : "text/uri-list"), atom("ONEUI_DROP"),
                          id_, event.data.l[2]);
    }
    return true;
}
} // namespace oneui::linux_platform
