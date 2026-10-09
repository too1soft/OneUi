#include "dock_session_internal.h"
#include "oneui/layout/split_view.h"
#include "oneui/layout/stack.h"
#include "oneui/ui_native_theme.h"
#include <algorithm>

namespace oneui {
namespace {
void followTheme(const std::shared_ptr<Widget> &widget) {
    widget->setOnStyleScopeChanged([weak = std::weak_ptr<Widget>(widget)] {
        if (auto current = weak.lock())
            if (auto sheet = current->styleScope())
                ui::applyNativeTheme(*current, *sheet, false);
    });
}
} // namespace
DockSession::Impl::Presentation DockSession::Impl::build(DockId host, const Layout &layout) {
    Presentation result;
    result.generation = revision + 1;
    if (layout.root)
        result.root = buildNode(host, *layout.root, layout, result);
    return result;
}
std::shared_ptr<Widget> DockSession::Impl::buildNode(DockId host, const DockNode &node,
                                                     const Layout &layout, Presentation &result,
                                                     std::vector<bool> path) {
    auto weak = weak_from_this();
    if (node.pane) {
        const auto &group = layout.groups.at(node.pane);
        auto column = std::make_shared<Stack>();
        column->setEngine(StackEngine::Yoga);
        auto tabs = std::make_shared<Tabs>();
        followTheme(tabs);
        std::vector<std::wstring> titles;
        for (auto id : group.tabs)
            titles.push_back(panes.at(id).title);
        tabs->setItems(std::move(titles));
        tabs->setDocumentMode(true);
        tabs->setReorderEnabled(true);
        tabs->setSelectedIndex(int(std::find(group.tabs.begin(), group.tabs.end(), group.selected) -
                                   group.tabs.begin()));
        const auto ids = group.tabs;
        tabs->setOnChanged([weak, ids](int index) {
            if (index < 0 || std::size_t(index) >= ids.size())
                return;
            if (auto self = weak.lock())
                self->dispatch([weak, pane = ids[index]] {
                    if (auto self = weak.lock())
                        self->select(pane);
                });
        });
        tabs->setOnReorderRequested([weak, ids, host](int from, int to) {
            if (from < 0 || to < 0 || std::size_t(from) >= ids.size() ||
                std::size_t(to) >= ids.size())
                return;
            if (auto self = weak.lock())
                self->dispatch([weak, pane = ids[from], host, to] {
                    if (auto self = weak.lock())
                        self->move(pane, {host, pane, DockPosition::Tab, to});
                });
        });
        tabs->setOnDetachRequested([weak, ids](int index, Point) {
            if (index < 0 || std::size_t(index) >= ids.size())
                return;
            if (auto self = weak.lock())
                self->startNativeDrag(ids[index]);
        });
        auto content = std::make_shared<Panel>();
        content->setBorderWidth(0);
        content->setBackground({0, 0, 0, 0});
        column->add(tabs);
        column->setFlex(tabs, {0, 0, 36});
        column->add(content);
        column->setFlex(content, {1, 1, 0});
        result.slots.emplace(node.pane, content);
        result.tabs.emplace(node.pane, tabs);
        return column;
    }
    auto split =
        std::make_shared<SplitView>(node.axis == DockAxis::Horizontal ? SplitOrientation::Horizontal
                                                                      : SplitOrientation::Vertical);
    followTheme(split);
    split->setSplitRatio(node.ratio);
    split->setResizable(true);
    split->setGap(6);
    auto minimum = [&](DockId id) {
        Size size{};
        for (auto pane : layout.groups.at(id).tabs) {
            auto min = panes.at(pane).minimum;
            size.width = std::max(size.width, min.width);
            size.height = std::max(size.height, min.height + 36);
        }
        return size;
    };
    auto a = dockMinimum(*node.first, 6, minimum), b = dockMinimum(*node.second, 6, minimum);
    bool horizontal = node.axis == DockAxis::Horizontal;
    split->setMinimumPaneExtent(horizontal ? a.width : a.height, horizontal ? b.width : b.height);
    auto first = path, second = path;
    first.push_back(false);
    second.push_back(true);
    split->setFirst(buildNode(host, *node.first, layout, result, std::move(first)));
    split->setSecond(buildNode(host, *node.second, layout, result, std::move(second)));
    split->setOnSplitRatioCommitted([weak, host, path, generation = revision + 1](float ratio) {
        auto self = weak.lock();
        if (!self || self->busy)
            return;
        auto hostView = self->hosts.find(host);
        if (hostView == self->hosts.end() || hostView->second.view.generation != generation)
            return;
        auto found = self->layouts.find(host);
        if (found != self->layouts.end() && found->second.root &&
            found->second.root->setRatio(path, ratio) == DockError::None) {
            // A resize invalidates drag transactions, but keeps this presentation valid.
            self->dragToken = 0;
            self->notify();
        }
    });
    return split;
}
void DockSession::Impl::connect(const std::map<DockId, Layout> &state,
                                std::map<DockId, Presentation> &views) {
    for (auto &[hostId, view] : views) {
        auto &host = hosts.at(hostId);
        for (auto &[groupId, slot] : view.slots) {
            auto selected = state.at(hostId).groups.at(groupId).selected;
            slot->setContent(panes.at(selected).content);
        }
        host.surface->setContent(view.root);
    }
}
DockSessionResult DockSession::Impl::apply(std::map<DockId, Layout> candidate,
                                           const std::set<DockId> &dirty, DockId moving,
                                           Window *source, Window *target) {
    auto keepAlive = shared_from_this();
    struct BusyGuard {
        bool &value;
        BusyGuard(bool &v) : value(v) { value = true; }
        ~BusyGuard() { value = false; }
    } guard(busy);
    std::map<DockId, Presentation> next, previous;
    std::map<DockId, std::shared_ptr<Widget>> focused;
    for (auto id : dirty)
        for (auto &[groupId, group] : layouts.at(id).groups) {
            auto widget = panes.at(group.selected).content;
            if (!widget->focused())
                continue;
            while (auto child = widget->activeFocusChild())
                widget = std::move(child);
            focused.emplace(group.selected, std::move(widget));
        }
    std::function<bool(Window &, Window &, DockTransferPhase)> transfer;
    if (moving && source && target && source != target) {
        auto &pane = panes.at(moving);
        transfer = pane.transfer;
    }
    auto alive = [&](Window *window) {
        return window && std::any_of(hosts.begin(), hosts.end(),
                                     [&](auto &entry) { return entry.second.window == window; });
    };
    bool prepared = false, detached = false;
    auto restoreFocus = [&](const std::map<DockId, Layout> &state) {
        for (auto &[pane, widget] : focused) {
            auto destination = locate(pane, state);
            if (destination.host && destination.selected) {
                auto window = hosts.at(destination.host).window;
                if (window)
                    window->requestFocus(widget.get());
            }
        }
    };
    auto rollback = [&] {
        if (detached && !stopped) {
            for (auto &[id, view] : next)
                for (auto &[group, slot] : view.slots)
                    slot->setContent({});
            connect(layouts, previous);
        }
        if (prepared && transfer && alive(source) && alive(target)) {
            try {
                transfer(*source, *target, DockTransferPhase::Rollback);
            } catch (...) {
            }
        }
        if (!stopped)
            restoreFocus(layouts);
    };
    try {
        // Allocate containers before touching an existing widget or its controller.
        for (auto id : dirty) {
            next.emplace(id, build(id, candidate.at(id)));
            previous.emplace(id, hosts.at(id).view);
        }
        if (transfer) {
            prepared = true;
            if (!transfer(*source, *target, DockTransferPhase::Prepare)) {
                rollback();
                return DockSessionResult::ResourceRejected;
            }
        }
        if (stopped || (source && !alive(source)) || (target && !alive(target))) {
            rollback();
            return DockSessionResult::InvalidHost;
        }
        detached = true;
        if (moving && source && source != target &&
            !source->prepareContentTransfer(panes.at(moving).content.get())) {
            rollback();
            return DockSessionResult::InvalidHost;
        }
        for (auto &[id, view] : previous)
            for (auto &[group, slot] : view.slots)
                slot->setContent({});
        connect(candidate, next);
        if (transfer && !transfer(*source, *target, DockTransferPhase::Commit)) {
            rollback();
            return DockSessionResult::ResourceRejected;
        }
        if (stopped || (source && !alive(source)) || (target && !alive(target))) {
            rollback();
            return DockSessionResult::InvalidHost;
        }
        layouts = std::move(candidate);
        for (auto &[id, view] : next)
            hosts.at(id).view = std::move(view);
        ++revision;
        restoreFocus(layouts);
    } catch (...) {
        try {
            rollback();
        } catch (...) { /* Allocation failure still retains every pane owner. */
        }
        return DockSessionResult::Failed;
    }
    // Notifications run after the transaction leaves its reentrancy guard.
    notify();
    for (auto id : dirty)
        if (owned.count(id) && !layouts.at(id).root) {
            dispatch([weak = weak_from_this(), id] {
                auto self = weak.lock();
                if (!self || self->stopped || self->busy)
                    return;
                auto host = self->hosts.find(id);
                if (host != self->hosts.end() && host->second.window && self->owned.count(id) &&
                    !self->layouts.at(id).root) {
                    host->second.window->setCloseToTray(false);
                    host->second.window->close();
                }
            });
        }
    return DockSessionResult::Applied;
}
} // namespace oneui
