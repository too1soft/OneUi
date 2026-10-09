#include "data_device_wayland.h"
#include <cstring>
#include <unistd.h>

namespace oneui::linux_platform {
class WaylandDockDrag final : public platform::NativeDockDrag {
  public:
    std::weak_ptr<WaylandDataDevice> device;
    wl_surface *surface;
    std::shared_ptr<platform::DockDragCallbacks> callbacks;
    Window::EventSubscription closed;
    ~WaylandDockDrag() override {
        if (auto data = device.lock()) {
            data->dockHosts.erase(surface);
            if (data->dockSourceSurface == surface)
                data->finishDock(platform::NativeDragResult::Cancelled);
        }
    }
    bool start(std::string payload,
               std::function<void(platform::NativeDragResult, Point)> done) override {
        auto data = device.lock();
        return data && data->startDock(surface, std::move(payload), std::move(done));
    }
    const char *limitation() const override {
        return "Wayland data drag is available; continuous top-level window dragging is "
               "unavailable. Use Detach/Merge commands.";
    }
};
std::shared_ptr<platform::NativeDockDrag>
WaylandDataDevice::dockDrag(Window &window, platform::DockDragCallbacks callbacks) {
    if (wl_data_device_manager_get_version(manager) < 3)
        return {};
    window.initialize();
    auto surface = static_cast<wl_surface *>(window.nativeHandle());
    if (!surface || !find(surface))
        return {};
    auto result = std::make_shared<WaylandDockDrag>();
    result->device = weak_from_this();
    result->surface = surface;
    result->callbacks = std::make_shared<platform::DockDragCallbacks>(std::move(callbacks));
    dockHosts[surface] = result->callbacks;
    result->closed = window.observeClosed([weak = weak_from_this(), surface] {
        if (auto self = weak.lock()) {
            self->dockHosts.erase(surface);
            if (self->grabSurface == surface) {
                self->grabSurface = nullptr;
                self->grabSerial = 0;
            }
            if (self->dockSourceSurface == surface)
                self->finishDock(platform::NativeDragResult::Cancelled);
        }
    });
    return result;
}
void WaylandDataDevice::pointerButton(wl_surface *surface, uint32_t serial, bool pressed) {
    grabSurface = pressed ? surface : nullptr;
    grabSerial = pressed ? serial : 0;
}
bool WaylandDataDevice::startDock(wl_surface *surface, std::string payload,
                                  std::function<void(platform::NativeDragResult, Point)> done) {
    if (dockSource || grabSurface != surface || !grabSerial || payload.empty() ||
        payload.size() > 1024 || wl_data_device_manager_get_version(manager) < 3 || !find(surface))
        return false;
    dockSource = wl_data_device_manager_create_data_source(manager);
    if (!dockSource)
        return false;
    dockPayload = std::move(payload);
    dockCompletion = std::move(done);
    dockSourceSurface = surface;
    dockAction = 0;
    dockDropped = false;
    static const wl_data_source_listener listener{
        [](void *, wl_data_source *, const char *) {},
        [](void *data, wl_data_source *, const char *mime, int32_t fd) {
            auto self = static_cast<WaylandDataDevice *>(data)->shared_from_this();
            if (mime && !std::strcmp(mime, platform::dockMime))
                self->sends.send(fd, self->dockPayload, platform::DesktopWindow::nowMs() + 3000);
            else
                ::close(fd);
        },
        [](void *data, wl_data_source *) {
            // Core Wayland cannot distinguish an outside release from Escape.
            // Keep the original host on cancellation; explicit Detach is always available.
            auto self = static_cast<WaylandDataDevice *>(data)->shared_from_this();
            self->finishDock(platform::NativeDragResult::Cancelled);
        },
        [](void *data, wl_data_source *) {
            auto self = static_cast<WaylandDataDevice *>(data);
            self->dockDropped = true;
            self->dockDeadline = platform::DesktopWindow::nowMs() + 5000;
        },
        [](void *data, wl_data_source *) {
            auto self = static_cast<WaylandDataDevice *>(data)->shared_from_this();
            self->finishDock(self->dockAction == WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE
                                 ? platform::NativeDragResult::Dropped
                                 : platform::NativeDragResult::Failed);
        },
        [](void *data, wl_data_source *, uint32_t action) {
            static_cast<WaylandDataDevice *>(data)->dockAction = action;
        }};
    wl_data_source_add_listener(dockSource, &listener, this);
    wl_data_source_offer(dockSource, platform::dockMime);
    wl_data_source_set_actions(dockSource, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
    wl_data_device_start_drag(device, dockSource, surface, nullptr, grabSerial);
    grabSurface = nullptr;
    grabSerial = 0;
    wl_display_flush(display);
    return true;
}
void WaylandDataDevice::finishDock(platform::NativeDragResult result) {
    if (!dockSource)
        return;
    wl_data_source_destroy(dockSource);
    dockSource = nullptr;
    dockSourceSurface = nullptr;
    dockPayload.clear();
    dockDropped = false;
    auto completion = std::move(dockCompletion);
    dockCompletion = {};
    if (completion)
        try {
            completion(result, {});
        } catch (...) {
        }
}
} // namespace oneui::linux_platform
