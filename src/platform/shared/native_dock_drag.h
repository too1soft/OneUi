#pragma once
#include "oneui/platform/window.h"
#include <functional>
#include <memory>
#include <string>

namespace oneui::platform {
inline constexpr const char *dockMime = "application/x-oneui-dock-v1";
enum class NativeDragResult { Dropped, Cancelled, Unhandled, Unsupported, Failed };
struct DockDragCallbacks {
    // Points use OneUI client coordinates. Payloads are untrusted until the
    // DockSession validates its active opaque transaction token.
    std::function<bool(const std::string &, Point, bool commit)> drop;
    std::function<void()> leave;
};
class NativeDockDrag {
  public:
    virtual ~NativeDockDrag() = default;
    // Called from the initiating pointer event on the UI thread. Completion can
    // be synchronous (OLE) or asynchronous (AppKit/Wayland).
    virtual bool start(std::string payload, std::function<void(NativeDragResult, Point)> done) = 0;
    virtual const char *limitation() const { return ""; }
};
std::shared_ptr<NativeDockDrag> makeNativeDockDrag(Window &, DockDragCallbacks);
} // namespace oneui::platform
