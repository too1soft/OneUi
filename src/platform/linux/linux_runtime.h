#pragma once
#include "oneui/platform/monitor.h"
#include "oneui/platform/window.h"
#include "platform/shared/native_dock_drag.h"
#include <stdexcept>
#include <thread>
namespace oneui::linux_platform {
class Connection {
  public:
    virtual ~Connection() = default;
    virtual std::unique_ptr<Window> create(WindowOptions options) = 0;
    virtual void setClipboard(const std::wstring &value) = 0;
    virtual std::wstring clipboard() = 0;
    virtual std::vector<MonitorInfo> monitors() = 0;
    virtual std::shared_ptr<platform::NativeDockDrag> dockDrag(Window &, platform::DockDragCallbacks) { return {}; }
    void assertUiThread() const {
        if (std::this_thread::get_id() != uiThread_)
            throw std::logic_error("Linux desktop services require the connection's UI thread; use post()");
    }
private:
    std::thread::id uiThread_ = std::this_thread::get_id();
};
std::shared_ptr<Connection> x11Connection();
std::shared_ptr<Connection> waylandConnection();
std::shared_ptr<Connection> connection();
} // namespace oneui::linux_platform
