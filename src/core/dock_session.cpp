#include "dock_session_internal.h"
#include <algorithm>
#include <cmath>

namespace oneui {
DockSession::DockSession(Application &app) : impl_(std::make_shared<Impl>(app)) {}
DockSession::~DockSession() {
    auto self = impl_;
    self->check();
    self->stopped = true;
    self->changed = {};
    for (auto &[id, host] : self->hosts) {
        host.closed.reset();
        for (auto &[group, slot] : host.view.slots)
            slot->setContent({});
        host.surface->setContent({});
    }
    for (auto &[id, window] : self->owned) {
        window->setCloseToTray(false);
        window->close();
    }
}
DockId DockSession::Impl::attach(Window &window, std::shared_ptr<Panel> surface,
                                 std::shared_ptr<void> owner) {
    check();
    if (busy || stopped || window.closed() || !surface || !surface->children().empty())
        return 0;
    for (auto &[id, host] : hosts)
        if (host.window == &window || host.surface == surface)
            return 0;
    auto id = nextHost++;
    auto subscription = window.observeClosed([weak = weak_from_this(), id] {
        if (auto self = weak.lock())
            self->hostClosed(id);
    });
    hosts.emplace(id,
                  Host{&window, std::move(surface), std::move(subscription), {}, std::move(owner)});
    layouts.emplace(id, Layout{});
    attachNativeDrag(id);
    applyTheme(hosts.at(id).surface);
    return id;
}
DockId DockSession::attachHost(Window &window, std::shared_ptr<Panel> surface,
                               std::shared_ptr<void> owner) {
    auto self = impl_;
    return self->attach(window, std::move(surface), std::move(owner));
}
DockSessionResult DockSession::addPane(DockPane pane, DockDestination dest) {
    auto self = impl_;
    self->check();
    if (self->busy)
        return DockSessionResult::Busy;
    if (!pane.id || !pane.content || self->panes.count(pane.id) ||
        pane.content->hasCompositionOwner() || !std::isfinite(pane.minimum.width) ||
        !std::isfinite(pane.minimum.height) || pane.minimum.width < 0 || pane.minimum.height < 0)
        return DockSessionResult::InvalidPane;
    for (auto &[id, existing] : self->panes)
        if (existing.content == pane.content)
            return DockSessionResult::InvalidPane;
    const auto id = pane.id;
    self->panes.emplace(id, std::move(pane));
    auto result = self->move(id, dest);
    if (result != DockSessionResult::Applied)
        self->panes.erase(id);
    else
        self->applyTheme(self->panes.at(id).content);
    return result;
}
DockSessionResult DockSession::removePane(DockId pane) {
    auto self = impl_;
    self->check();
    if (self->busy)
        return DockSessionResult::Busy;
    if (!self->panes.count(pane))
        return DockSessionResult::InvalidPane;
    auto from = self->locate(pane, self->layouts);
    if (from.host) {
        auto candidate = self->layouts;
        Impl::erasePane(candidate.at(from.host), pane);
        auto result = self->apply(std::move(candidate), {from.host});
        if (result != DockSessionResult::Applied)
            return result;
    }
    self->panes.erase(pane);
    self->dragToken = 0;
    return DockSessionResult::Applied;
}
DockSessionResult DockSession::movePane(DockId pane, DockDestination destination) {
    auto self = impl_;
    return self->move(pane, destination);
}
DockSessionResult DockSession::selectPane(DockId pane) {
    auto self = impl_;
    return self->select(pane);
}
DockSessionResult DockSession::detachPane(DockId pane, WindowOptions options, DockId *resultHost) {
    auto self = impl_;
    return self->detach(pane, std::move(options), resultHost);
}
DockSessionResult DockSession::Impl::detach(DockId pane, WindowOptions options,
                                            DockId *resultHost) {
    auto self = shared_from_this();
    self->check();
    if (resultHost)
        *resultHost = 0;
    if (self->busy)
        return DockSessionResult::Busy;
    if (!self->panes.count(pane))
        return DockSessionResult::InvalidPane;
    if (Application::current() != self->application)
        return DockSessionResult::InvalidHost;
    if (self->owned.size() >= 64)
        return DockSessionResult::Failed;
    options.visible = false;
    auto window = Window::create(options);
    auto surface = std::make_shared<Panel>();
    surface->setBorderWidth(0);
    auto previous = self->locate(pane, self->layouts);
    if (previous.host)
        surface->setStyleScope(self->hosts.at(previous.host).surface->styleScope());
    window->setContent(surface);
    window->initialize();
    if (!window->nativeHandle() || !self->application->addWindow(*window))
        return DockSessionResult::Failed;
    auto id = self->attach(*window, surface);
    if (!id) {
        self->application->removeWindow(*window);
        return DockSessionResult::Failed;
    }
    auto result = self->move(pane, {id});
    if (result != DockSessionResult::Applied) {
        self->hosts.erase(id);
        self->layouts.erase(id);
        self->application->removeWindow(*window);
        window->close();
        return result;
    }
    auto native = window.get();
    self->owned.emplace(id, std::move(window));
    native->show();
    native->activate();
    if (resultHost)
        *resultHost = id;
    return result;
}
void DockSession::Impl::hostClosed(DockId id) {
    auto found = hosts.find(id);
    if (found == hosts.end() || stopped)
        return;
    found->second.window = nullptr;
    // Do not destroy a native Window on its own close callback's stack.
    dispatch([weak = weak_from_this(), id] {
        auto self = weak.lock();
        if (!self || self->stopped)
            return;
        if (self->busy) {
            self->hostClosed(id);
            return;
        }
        auto found = self->hosts.find(id);
        if (found == self->hosts.end())
            return;
        DockId destination = 0;
        for (auto &[other, host] : self->hosts)
            if (host.window && !host.window->closed()) {
                destination = other;
                break;
            }
        std::vector<DockId> panes;
        for (auto &[group, data] : self->layouts.at(id).groups)
            panes.insert(panes.end(), data.tabs.begin(), data.tabs.end());
        if (destination)
            for (auto pane : panes)
                self->move(pane, {destination});
        // If there is no host left, keep the original owners as orphan panes.
        for (auto &[group, slot] : found->second.view.slots)
            slot->setContent({});
        found->second.surface->setContent({});
        self->layouts.erase(id);
        self->hosts.erase(id);
        self->owned.erase(id);
        ++self->revision;
        self->dragToken = 0;
        self->notify();
    });
}
std::uint64_t DockSession::beginDrag(DockId pane) {
    auto self = impl_;
    self->check();
    if (self->busy || !self->panes.count(pane))
        return 0;
    self->dragPane = pane;
    self->dragRevision = self->revision;
    return self->dragToken = self->nextToken++;
}
DockSessionResult DockSession::commitDrag(std::uint64_t token, DockDestination destination) {
    auto self = impl_;
    self->check();
    if (!token || token != self->dragToken || self->dragRevision != self->revision)
        return DockSessionResult::StaleTransaction;
    self->dragToken = 0;
    return self->move(self->dragPane, destination);
}
bool DockSession::cancelDrag(std::uint64_t token) {
    auto self = impl_;
    self->check();
    if (!token || token != self->dragToken)
        return false;
    self->dragToken = 0;
    return true;
}
DockPaneLocation DockSession::location(DockId pane) const {
    impl_->check();
    return impl_->locate(pane, impl_->layouts);
}
std::vector<DockId> DockSession::panes(DockId host) const {
    impl_->check();
    std::vector<DockId> result;
    for (auto &[id, pane] : impl_->panes)
        if (!host || location(id).host == host)
            result.push_back(id);
    return result;
}
std::vector<DockId> DockSession::hosts() const {
    impl_->check();
    std::vector<DockId> result;
    for (auto &[id, host] : impl_->hosts)
        if (host.window)
            result.push_back(id);
    return result;
}
std::shared_ptr<Widget> DockSession::content(DockId id) const {
    impl_->check();
    auto found = impl_->panes.find(id);
    return found == impl_->panes.end() ? nullptr : found->second.content;
}
Window *DockSession::hostWindow(DockId id) const {
    impl_->check();
    auto found = impl_->hosts.find(id);
    return found == impl_->hosts.end() ? nullptr : found->second.window;
}
void DockSession::setOnChanged(std::function<void()> callback) {
    impl_->check();
    impl_->changed = std::move(callback);
}
} // namespace oneui
