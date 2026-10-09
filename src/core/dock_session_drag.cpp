#include "dock_session_internal.h"
#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

namespace oneui {
void DockSession::Impl::attachNativeDrag(DockId id) {
    auto &host = hosts.at(id);
    platform::DockDragCallbacks callbacks;
    callbacks.drop = [weak = weak_from_this(), id](const std::string &payload, Point point,
                                                   bool commit) {
        auto self = weak.lock();
        return self && self->nativeDrop(id, payload, point, commit);
    };
    try {
        host.nativeDrag = platform::makeNativeDockDrag(*host.window, std::move(callbacks));
    } catch (...) {
        host.nativeDrag.reset();
    }
}
bool DockSession::nativeDragSupported(DockId id) const {
    impl_->check();
    auto host = impl_->hosts.find(id);
    return host != impl_->hosts.end() && host->second.window && bool(host->second.nativeDrag);
}
std::string DockSession::nativeDragLimitation(DockId id) const {
    impl_->check();
    auto host = impl_->hosts.find(id);
    if (host == impl_->hosts.end() || !host->second.window)
        return "Host is unavailable";
    return host->second.nativeDrag ? host->second.nativeDrag->limitation()
                                   : "Native dock drag is unsupported by this backend";
}
bool DockSession::startNativeDrag(DockId pane) {
    auto self = impl_;
    return self->startNativeDrag(pane);
}
bool DockSession::Impl::startNativeDrag(DockId pane) {
    check();
    if (busy || stopped || !nativePayload.empty())
        return false;
    auto from = locate(pane, layouts);
    if (!from.host)
        return false;
    auto native = hosts.at(from.host).nativeDrag;
    if (!native)
        return false;
    std::vector<std::shared_ptr<Widget>> retained;
    for (auto &[id, host] : hosts)
        retained.push_back(host.view.root);
    dragPane = pane;
    dragRevision = revision;
    dragToken = nextToken++;
    nativeDropAttempted = false;
    try {
        std::random_device random;
        std::ostringstream payload;
        payload << "oneui-dock:" << std::hex << std::setfill('0');
        for (int i = 0; i < 4; ++i)
            payload << std::setw(8) << random();
        payload << ':' << dragToken;
        nativePayload = payload.str();
        auto completion = [weak = weak_from_this(), token = dragToken,
                           retained](platform::NativeDragResult result, Point point) {
            auto self = weak.lock();
            if (!self)
                return;
            const bool valid = self->dragToken == token && self->revision == self->dragRevision;
            self->nativePayload.clear();
            self->dragToken = 0;
            if (!self->stopped && valid && !self->nativeDropAttempted &&
                result == platform::NativeDragResult::Unhandled) {
                DockId newHost = 0;
                if (self->detach(self->dragPane, {self->panes.at(self->dragPane).title, 640, 440},
                                 &newHost) == DockSessionResult::Applied) {
                    auto window = self->hosts.at(newHost).window;
                    WindowPlacement placement;
                    if (window->getWindowPlacement(placement)) {
                        placement.x = int(point.x) - 24;
                        placement.y = window->backend() == WindowBackend::Cocoa
                                          ? int(point.y) - placement.height + 24
                                          : int(point.y) - 24;
                        window->setWindowPlacement(placement);
                    }
                }
            }
            // OLE runs a nested loop. Keep the originating controls alive until
            // the original pointer-handler stack has returned to the main loop.
            self->dispatch([retained] {});
        };
        if (native->start(nativePayload, std::move(completion)))
            return true;
    } catch (...) { /* Retain the original tree if the native operation cannot start. */
    }
    nativePayload.clear();
    dragToken = 0;
    return false;
}
bool DockSession::Impl::nativeDrop(DockId hostId, const std::string &payload, Point point,
                                   bool commit) {
    check();
    if (busy || stopped || payload.empty() || payload != nativePayload || !dragToken ||
        revision != dragRevision)
        return false;
    auto found = hosts.find(hostId);
    if (found == hosts.end() || !found->second.window || !found->second.surface->visible() ||
        !found->second.surface->frame().contains(point))
        return false;
    auto &host = found->second;
    auto &layout = layouts.at(hostId);
    DockDestination destination{hostId};
    bool hit = !layout.root;
    for (auto &[groupId, slot] : host.view.slots) {
        const auto &group = layout.groups.at(groupId);
        auto tabs = host.view.tabs.at(groupId);
        if (tabs->frame().contains(point)) {
            destination.pane = group.selected;
            destination.index = int(group.tabs.size());
            for (std::size_t i = 0; i < group.tabs.size(); ++i) {
                auto frame = tabs->itemFrame(int(i));
                if (point.x < frame.x + frame.width / 2) {
                    destination.index = int(i);
                    break;
                }
            }
            hit = true;
            break;
        }
        auto rect = slot->frame();
        if (!rect.contains(point))
            continue;
        destination.pane = group.selected;
        auto area = dockHitTest(0, point, {{group.selected, rect, false}}, DockHitMode::TabMerge);
        if (!area)
            return false;
        destination.position = static_cast<DockPosition>(int(area->placement));
        hit = true;
        break;
    }
    if (!hit || (destination.pane == dragPane && destination.position != DockPosition::Tab))
        return false;
    if (!commit)
        return true;
    nativeDropAttempted = true;
    dragToken = 0;
    return move(dragPane, destination) == DockSessionResult::Applied;
}
} // namespace oneui
