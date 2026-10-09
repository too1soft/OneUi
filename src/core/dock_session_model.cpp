#include "dock_session_internal.h"
#include <algorithm>
#include <stdexcept>

namespace oneui {
void DockSession::Impl::check() const {
    if (thread != std::this_thread::get_id())
        throw std::logic_error("DockSession belongs to its UI thread");
}
DockPaneLocation DockSession::Impl::locate(DockId pane,
                                           const std::map<DockId, Layout> &state) const {
    for (auto &[host, layout] : state)
        for (auto &[groupId, group] : layout.groups) {
            auto found = std::find(group.tabs.begin(), group.tabs.end(), pane);
            if (found != group.tabs.end())
                return {host, groupId, int(found - group.tabs.begin()), group.selected == pane};
        }
    return {};
}
void DockSession::Impl::erasePane(Layout &layout, DockId pane) {
    for (auto it = layout.groups.begin(); it != layout.groups.end(); ++it) {
        auto &group = it->second;
        auto found = std::find(group.tabs.begin(), group.tabs.end(), pane);
        if (found == group.tabs.end())
            continue;
        auto index = std::size_t(found - group.tabs.begin());
        group.tabs.erase(found);
        if (group.tabs.empty()) {
            if (layout.root)
                layout.root = layout.root->without(it->first);
            layout.groups.erase(it);
        } else if (group.selected == pane) {
            group.selected = group.tabs[std::min(index, group.tabs.size() - 1)];
        }
        return;
    }
}
DockSessionResult DockSession::Impl::insert(std::map<DockId, Layout> &state, DockId pane,
                                            DockDestination dest) {
    auto host = hosts.find(dest.host);
    if (host == hosts.end() || !host->second.window || host->second.window->closed())
        return DockSessionResult::InvalidHost;
    if (int(dest.position) < 0 || int(dest.position) > int(DockPosition::Bottom) || dest.index < -1)
        return DockSessionResult::InvalidTarget;
    auto &layout = state.at(dest.host);
    DockId group = 0;
    if (dest.pane) {
        auto target = locate(dest.pane, state);
        if (target.host != dest.host)
            return DockSessionResult::InvalidTarget;
        group = target.group;
    } else if (layout.root)
        group = layout.root->ids().front();
    if (!group) {
        group = nextGroup++;
        layout.root.emplace(group);
        layout.groups.emplace(group, Group{{pane}, pane});
        return DockSessionResult::Applied;
    }
    if (dest.position == DockPosition::Tab) {
        auto &tabs = layout.groups.at(group);
        auto index =
            dest.index < 0 ? tabs.tabs.size() : std::min(std::size_t(dest.index), tabs.tabs.size());
        tabs.tabs.insert(tabs.tabs.begin() + index, pane);
        tabs.selected = pane;
    } else {
        const auto id = nextGroup++;
        bool before = dest.position == DockPosition::Left || dest.position == DockPosition::Top;
        auto axis = dest.position == DockPosition::Left || dest.position == DockPosition::Right
                        ? DockAxis::Horizontal
                        : DockAxis::Vertical;
        DockNode split(axis, .5f, DockNode(before ? id : group), DockNode(before ? group : id));
        if (!layout.root->replace(group, std::move(split)))
            return DockSessionResult::InvalidTarget;
        layout.groups.emplace(id, Group{{pane}, pane});
    }
    return DockSessionResult::Applied;
}
DockSessionResult DockSession::Impl::move(DockId pane, DockDestination dest) {
    check();
    if (busy)
        return DockSessionResult::Busy;
    if (!panes.count(pane))
        return DockSessionResult::InvalidPane;
    auto from = locate(pane, layouts);
    auto candidate = layouts;
    // Reordering relative to oneself retains the group even when it has one tab.
    if (dest.pane == pane) {
        if (dest.host != from.host || dest.position != DockPosition::Tab)
            return DockSessionResult::InvalidTarget;
        auto &group = candidate.at(from.host).groups.at(from.group);
        group.tabs.erase(group.tabs.begin() + from.index);
        auto index = dest.index < 0 ? group.tabs.size()
                                    : std::min(std::size_t(dest.index), group.tabs.size());
        group.tabs.insert(group.tabs.begin() + index, pane);
        group.selected = pane;
    } else {
        if (from.host)
            erasePane(candidate.at(from.host), pane);
        auto status = insert(candidate, pane, dest);
        if (status != DockSessionResult::Applied)
            return status;
    }
    auto source = from.host ? hosts.at(from.host).window : nullptr;
    auto target = hosts.at(dest.host).window;
    std::set<DockId> dirty{dest.host};
    if (from.host)
        dirty.insert(from.host);
    return apply(std::move(candidate), dirty, pane, source, target);
}
DockSessionResult DockSession::Impl::select(DockId pane) {
    check();
    if (busy)
        return DockSessionResult::Busy;
    auto from = locate(pane, layouts);
    if (!from.host)
        return DockSessionResult::InvalidPane;
    if (from.selected)
        return DockSessionResult::Applied;
    auto candidate = layouts;
    candidate.at(from.host).groups.at(from.group).selected = pane;
    return apply(std::move(candidate), {from.host});
}
void DockSession::Impl::notify() {
    if (changedQueued || stopped)
        return;
    changedQueued = true;
    if (!dispatch([weak = weak_from_this()] {
            auto self = weak.lock();
            if (!self)
                return;
            self->changedQueued = false;
            if (self->stopped)
                return;
            if (auto callback = self->changed) {
                try {
                    callback();
                } catch (...) { /* A committed transfer remains committed. */
                }
            }
        }))
        changedQueued = false;
}
} // namespace oneui
