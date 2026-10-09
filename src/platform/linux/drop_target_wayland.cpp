#include "data_device_wayland.h"
#include "platform/shared/file_drop.h"
#include <fcntl.h>
#include <unistd.h>

namespace oneui::linux_platform {
bool WaylandDataDevice::dockOffer(wl_surface *surface, const std::string &payload, Point point,
                                  bool commit) {
    auto window = find(surface);
    auto entry = dockHosts.find(surface);
    auto host = entry != dockHosts.end() ? entry->second.lock() : nullptr;
    if (!window || window->nativeModal() || payload.empty() || !host || !host->drop)
        return false;
    try {
        return host->drop(payload, clientPoint(surface, point), commit);
    } catch (...) {
        return false;
    }
}
void WaylandDataDevice::cancelDrag() {
    auto entry = dockHosts.find(dragSurface);
    auto host = entry != dockHosts.end() ? entry->second.lock() : nullptr;
    if (host && host->leave)
        host->leave();
    if (dragOffer) {
        wl_data_offer_destroy(dragOffer);
        offers.erase(dragOffer);
    }
    dragOffer = nullptr;
    dragSurface = nullptr;
}
void WaylandDataDevice::updateDrag() {
    const auto it = offers.find(dragOffer);
    if (it == offers.end())
        return;
    auto window = find(dragSurface);
    const bool dock = it->second->dock;
    const bool accept = dock ? dockOffer(dragSurface, dockPayload, dragPoint, false)
                             : window && window->acceptsFileDrop() && it->second->files;
    const auto action =
        dock ? WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE : WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    wl_data_offer_accept(dragOffer, dragSerial,
                         accept ? (dock ? platform::dockMime : "text/uri-list") : nullptr);
    if (wl_data_offer_get_version(dragOffer) >= WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION)
        wl_data_offer_set_actions(dragOffer, accept ? action : 0, accept ? action : 0);
}
void WaylandDataDevice::beginDrop() {
    const auto it = offers.find(dragOffer);
    auto window = find(dragSurface);
    if (it == offers.end() || !window) {
        cancelDrag();
        return;
    }
    const bool dock = it->second->dock;
    const auto action =
        dock ? WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE : WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    const bool accept = dock ? dockOffer(dragSurface, dockPayload, dragPoint, false)
                             : window->acceptsFileDrop() && it->second->files;
    if (!accept || (wl_data_offer_get_version(dragOffer) >= 3 && it->second->action != action)) {
        cancelDrag();
        return;
    }
    finishDrop(false);
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) < 0) {
        cancelDrag();
        return;
    }
    if (!drop.reader.start(fds[0], platform::DesktopWindow::nowMs() + 3000,
                           dock ? 1024 : 4u * 1024u * 1024u)) {
        ::close(fds[1]);
        cancelDrag();
        return;
    }
    drop.offer = dragOffer;
    drop.target = dragSurface;
    drop.dock = dock;
    drop.lifetime = window->nativeLifetime();
    drop.point = dragPoint;
    wl_data_offer_receive(drop.offer, dock ? platform::dockMime : "text/uri-list", fds[1]);
    ::close(fds[1]);
    dragOffer = nullptr;
    dragSurface = nullptr;
}
void WaylandDataDevice::finishDrop(bool success) {
    if (!drop.offer)
        return;
    auto lifetime = drop.lifetime.lock();
    auto window = lifetime && *lifetime ? find(drop.target) : nullptr;
    auto paths = success && !drop.dock ? platform::filePathsFromUriList(drop.reader.bytes())
                                       : std::vector<std::wstring>{};
    if (drop.dock)
        success =
            success && window && dockOffer(drop.target, drop.reader.bytes(), drop.point, true);
    else
        success = success && window && window->acceptsFileDrop() && !paths.empty();
    if (success && wl_data_offer_get_version(drop.offer) >= WL_DATA_OFFER_FINISH_SINCE_VERSION)
        wl_data_offer_finish(drop.offer);
    wl_data_offer_destroy(drop.offer);
    offers.erase(drop.offer);
    const auto point = drop.point;
    drop = {};
    if (success && !paths.empty())
        window->deliverFileDrop(std::move(paths), point);
}
void WaylandDataDevice::readDrop() {
    if (!drop.offer)
        return;
    const auto alive = drop.lifetime.lock();
    if (!alive || !*alive) {
        finishDrop(false);
        return;
    }
    const auto state = drop.reader.pump(platform::DesktopWindow::nowMs());
    if (state != PipeTransferState::Pending)
        finishDrop(state == PipeTransferState::Complete);
}
} // namespace oneui::linux_platform
