#pragma once
#include "data_transfer_pipe.h"
#include "platform/shared/desktop_window.h"
#include "platform/shared/native_dock_drag.h"
#include <map>
#include <wayland-client.h>

namespace oneui::linux_platform {
class WaylandDataDevice : public std::enable_shared_from_this<WaylandDataDevice> {
  public:
    struct Offer {
        wl_data_offer *object = nullptr;
        bool utf8 = false, plain = false, files = false, dock = false;
        uint32_t action = 0;
    };
    using FindWindow = std::function<platform::DesktopWindow *(wl_surface *)>;
    using ClientPoint = std::function<Point(wl_surface *, Point)>;
    WaylandDataDevice(wl_display *, wl_data_device_manager *, wl_seat *, FindWindow, ClientPoint);
    ~WaylandDataDevice();
    void setClipboard(const std::wstring &, uint32_t serial);
    std::wstring clipboard();
    int pump(int timeout);
    void pollDescriptors(std::vector<pollfd> &) const;
    std::shared_ptr<platform::NativeDockDrag> dockDrag(Window &, platform::DockDragCallbacks);
    void pointerButton(wl_surface *, uint32_t serial, bool pressed);
    bool startDock(wl_surface *, std::string,
                   std::function<void(platform::NativeDragResult, Point)>);
    void finishDock(platform::NativeDragResult);
    void cancelDrag();
    void updateDrag();
    void beginDrop();
    void readDrop();
    void finishDrop(bool);
    bool dockOffer(wl_surface *, const std::string &, Point, bool commit);
    wl_display *display;
    wl_data_device_manager *manager;
    wl_data_device *device = nullptr;
    FindWindow find;
    ClientPoint clientPoint;
    std::map<wl_data_offer *, std::unique_ptr<Offer>> offers;
    std::map<wl_surface *, std::weak_ptr<platform::DockDragCallbacks>> dockHosts;
    wl_data_offer *selection = nullptr, *dragOffer = nullptr;
    wl_surface *dragSurface = nullptr;
    Point dragPoint{};
    uint32_t dragSerial = 0;
    struct DropRead {
        DataPipeReader reader;
        wl_data_offer *offer = nullptr;
        wl_surface *target = nullptr;
        std::weak_ptr<std::atomic<bool>> lifetime;
        Point point{};
        bool dock = false;
    } drop;
    wl_data_source *source = nullptr;
    std::string ownedText;
    DataPipeWriters sends;
    wl_surface *grabSurface = nullptr, *dockSourceSurface = nullptr;
    uint32_t grabSerial = 0;
    wl_data_source *dockSource = nullptr;
    std::string dockPayload;
    std::function<void(platform::NativeDragResult, Point)> dockCompletion;
    uint32_t dockAction = 0;
    double dockDeadline = 0;
    bool dockDropped = false;
};
} // namespace oneui::linux_platform
