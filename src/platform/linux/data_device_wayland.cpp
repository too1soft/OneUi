#include "data_device_wayland.h"
#include "internal/unicode.h"
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
namespace oneui::linux_platform {
WaylandDataDevice::WaylandDataDevice(wl_display *d, wl_data_device_manager *m, wl_seat *seat,
                                     FindWindow lookup, ClientPoint point)
    : display(d), manager(m), find(std::move(lookup)), clientPoint(std::move(point)) {
    device = wl_data_device_manager_get_data_device(manager, seat);
    static const wl_data_device_listener dataListener{
        [](void *d, wl_data_device *, wl_data_offer *object) {
            auto &c = *static_cast<WaylandDataDevice *>(d);
            auto offer = std::make_unique<Offer>();
            offer->object = object;
            static const wl_data_offer_listener listener{
                [](void *d, wl_data_offer *, const char *mime) {
                    auto &o = *static_cast<Offer *>(d);
                    o.utf8 |= !std::strcmp(mime, "text/plain;charset=utf-8");
                    o.plain |= !std::strcmp(mime, "text/plain");
                    o.files |= !std::strcmp(mime, "text/uri-list");
                    o.dock |= !std::strcmp(mime, platform::dockMime);
                },
                [](void *, wl_data_offer *, uint32_t) {},
                [](void *d, wl_data_offer *, uint32_t action) {
                    static_cast<Offer *>(d)->action = action;
                }};
            wl_data_offer_add_listener(object, &listener, offer.get());
            c.offers[object] = std::move(offer);
        },
        [](void *d, wl_data_device *, uint32_t serial, wl_surface *surface, wl_fixed_t x,
           wl_fixed_t y, wl_data_offer *object) {
            auto &c = *static_cast<WaylandDataDevice *>(d);
            c.cancelDrag();
            c.dragOffer = object;
            c.dragSurface = surface;
            c.dragSerial = serial;
            c.dragPoint = {float(wl_fixed_to_double(x)), float(wl_fixed_to_double(y))};
            c.updateDrag();
        },
        [](void *d, wl_data_device *) { static_cast<WaylandDataDevice *>(d)->cancelDrag(); },
        [](void *d, wl_data_device *, uint32_t, wl_fixed_t x, wl_fixed_t y) {
            auto &c = *static_cast<WaylandDataDevice *>(d);
            c.dragPoint = {float(wl_fixed_to_double(x)), float(wl_fixed_to_double(y))};
            c.updateDrag();
        },
        [](void *d, wl_data_device *) { static_cast<WaylandDataDevice *>(d)->beginDrop(); },
        [](void *d, wl_data_device *, wl_data_offer *object) {
            auto &c = *static_cast<WaylandDataDevice *>(d);
            if (c.selection && c.selection != object) {
                wl_data_offer_destroy(c.selection);
                c.offers.erase(c.selection);
            }
            c.selection = object;
        }};
    wl_data_device_add_listener(device, &dataListener, this);
}
WaylandDataDevice::~WaylandDataDevice() {
    dockCompletion = {};
    if (dockSource)
        wl_data_source_destroy(dockSource);
    drop.reader.reset();
    sends.clear();
    for (auto &item : offers)
        wl_data_offer_destroy(item.first);
    if (source)
        wl_data_source_destroy(source);
    if (wl_data_device_get_version(device) >= WL_DATA_DEVICE_RELEASE_SINCE_VERSION)
        wl_data_device_release(device);
    else
        wl_data_device_destroy(device);
}
void WaylandDataDevice::setClipboard(const std::wstring &value, uint32_t serial) {
    if (!device || !serial)
        throw std::runtime_error("Wayland clipboard requires a seat and a user-input serial");
    auto bytes = unicode::toUtf8(value);
    if (bytes.size() > 16u * 1024u * 1024u)
        throw std::runtime_error("Wayland clipboard exceeds 16 MiB");
    if (source)
        wl_data_source_destroy(source);
    ownedText = std::move(bytes);
    source = wl_data_device_manager_create_data_source(manager);
    static const wl_data_source_listener listener{
        [](void *, wl_data_source *, const char *) {},
        [](void *d, wl_data_source *, const char *, int32_t fd) {
            auto &c = *static_cast<WaylandDataDevice *>(d);
            c.sends.send(fd, c.ownedText, platform::DesktopWindow::nowMs() + 5000);
        },
        [](void *d, wl_data_source *source) {
            auto &c = *static_cast<WaylandDataDevice *>(d);
            if (c.source == source)
                c.source = nullptr;
            wl_data_source_destroy(source);
        },
        [](void *, wl_data_source *) {},
        [](void *, wl_data_source *) {},
        [](void *, wl_data_source *, uint32_t) {}};
    wl_data_source_add_listener(source, &listener, this);
    wl_data_source_offer(source, "text/plain;charset=utf-8");
    wl_data_source_offer(source, "text/plain");
    wl_data_device_set_selection(device, source, serial);
    wl_display_flush(display);
}
std::wstring WaylandDataDevice::clipboard() {
    if (source)
        return unicode::fromUtf8(ownedText);
    auto it = offers.find(selection);
    if (it == offers.end() || (!it->second->utf8 && !it->second->plain))
        return {};
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) < 0)
        throw std::runtime_error("Wayland clipboard pipe failed");
    DataPipeReader reader;
    if (!reader.start(fds[0], platform::DesktopWindow::nowMs() + 1500, 16u * 1024u * 1024u)) {
        ::close(fds[1]);
        throw std::runtime_error("Wayland clipboard reader configuration failed");
    }
    wl_data_offer_receive(selection, it->second->utf8 ? "text/plain;charset=utf-8" : "text/plain",
                          fds[1]);
    ::close(fds[1]);
    wl_display_flush(display);
    for (;;) {
        auto state = reader.pump(platform::DesktopWindow::nowMs());
        if (state == PipeTransferState::Complete)
            return unicode::fromUtf8(reader.bytes());
        if (state == PipeTransferState::Failed)
            throw std::runtime_error("Wayland clipboard transfer timed out or exceeded 16 MiB");
        pollfd descriptor{reader.fd(), POLLIN, 0};
        poll(&descriptor, 1, 20);
    }
}

int WaylandDataDevice::pump(int timeout) {
    auto now = platform::DesktopWindow::nowMs();
    sends.pump(now);
    readDrop();
    if (drop.reader.fd() >= 0)
        timeout = std::min(timeout, std::max(0, int(drop.reader.deadline() - now)));
    if (dockSource && dockDropped) {
        if (now >= dockDeadline)
            finishDock(platform::NativeDragResult::Failed);
        else
            timeout = std::min(timeout, std::max(0, int(dockDeadline - now)));
    }
    return sends.constrainTimeout(timeout, now);
}
void WaylandDataDevice::pollDescriptors(std::vector<pollfd> &descriptors) const {
    sends.pollDescriptors(descriptors);
    if (drop.reader.fd() >= 0)
        descriptors.push_back({drop.reader.fd(), POLLIN, 0});
}
} // namespace oneui::linux_platform
