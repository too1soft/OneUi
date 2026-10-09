#pragma once
#include "platform/shared/desktop_window.h"
#include "platform/shared/native_dock_drag.h"
#include <X11/Xlib.h>
#undef None
#undef Status
namespace oneui::linux_platform {
class X11DropTarget {
  public:
    X11DropTarget(Display *, ::Window, platform::DesktopWindow &);
    ~X11DropTarget();
    void enableFiles(bool);
    void setDock(std::weak_ptr<platform::DockDragCallbacks>);
    bool event(XEvent &);
    bool expire();
    void cancel();

  private:
    Atom atom(const char *) const;
    void advertise();
    void send(const char *, long flags, Atom action);
    bool message(const XClientMessageEvent &);
    void read();
    void finish(bool);
    Display *display_;
    ::Window id_, source_ = 0;
    platform::DesktopWindow &window_;
    std::weak_ptr<platform::DockDragCallbacks> dock_;
    bool filesEnabled_ = false, files_ = false, private_ = false;
    bool accepted_ = false, pending_ = false, incremental_ = false;
    Point point_{};
    double deadline_ = 0;
    std::string bytes_;
};
std::shared_ptr<platform::NativeDockDrag> makeX11DockDrag(Window &, Display *,
                                                          platform::DockDragCallbacks);
bool x11DockEvent(Display *, XEvent &);
bool pollX11DockDrag(Display *);
std::string x11DockPayload(Display *, ::Window);
X11DropTarget *x11DropTarget(Display *, ::Window);
} // namespace oneui::linux_platform
