#include "oneui/modal_session.h"
#include <unordered_map>

namespace oneui {
namespace {
struct ParentGate {
    unsigned count = 0;
    std::weak_ptr<Widget> focus;
    bool focusVisible = false;
};
thread_local std::unordered_map<Window *, ParentGate> modalParents;
} // namespace
struct ModalSession::Impl {
    static thread_local std::unordered_map<Window *, std::weak_ptr<Impl>> children;
    Window *child = nullptr;
    Window *parent = nullptr;
    Window::EventSubscription childClosed, parentClosed;
    std::function<void(int)> completed;
    int result = 0;
    bool active = true;
    void release(bool deliver) {
        if (!active)
            return;
        active = false;
        auto owner = parent;
        auto retiring = child;
        parent = nullptr;
        child = nullptr;
        children.erase(retiring);
        childClosed.reset();
        parentClosed.reset();
        bool ownerAlive = owner && !owner->closed();
        auto ownerLifetime = ownerAlive ? owner->observeClosed([&] {
            ownerAlive = false;
            modalParents.erase(owner);
        })
                                        : Window::EventSubscription{};
        if (retiring && !retiring->closed())
            retiring->setOwner(nullptr);
        if (ownerAlive) {
            const auto found = modalParents.find(owner);
            if (found != modalParents.end() && --found->second.count == 0) {
                const auto gate = found->second;
                modalParents.erase(found);
                owner->setInputEnabled(true);
                if (ownerAlive)
                    owner->activate();
                if (ownerAlive) {
                    if (auto focus = gate.focus.lock())
                        owner->requestFocus(focus.get(), gate.focusVisible);
                }
            }
        }
        auto callback = std::move(completed);
        if (deliver && callback)
            callback(result);
    }
};
thread_local std::unordered_map<Window *, std::weak_ptr<ModalSession::Impl>>
    ModalSession::Impl::children;
ModalSession::ModalSession(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}
std::unique_ptr<ModalSession> ModalSession::show(Window &child, Window &parent,
                                                 std::function<void(int)> completed) {
    if (&child == &parent || child.closed() || parent.closed())
        return {};
    if (auto existing = Impl::children.find(&child); existing != Impl::children.end())
        if (auto session = existing->second.lock(); session && session->active)
            return {};
    // Follow active modal parent links before mutating native ownership. This
    // rejects cycles while allowing a modal child to own another modal dialog.
    for (Window *ancestor = &parent; ancestor;) {
        if (ancestor == &child)
            return {};
        auto found = Impl::children.find(ancestor);
        auto session = found == Impl::children.end() ? nullptr : found->second.lock();
        ancestor = session ? session->parent : nullptr;
    }
    parent.initialize();
    child.initialize();
    auto focus = parent.commandRoot_.lock();
    while (focus) {
        auto next = focus->activeFocusChild();
        if (!next)
            break;
        focus = std::move(next);
    }
    if (!child.setOwner(&parent))
        return {};
    if (!parent.setInputEnabled(false)) {
        child.setOwner(nullptr);
        return {};
    }
    auto &gate = modalParents[&parent];
    if (gate.count++ == 0) {
        gate.focus = focus;
        gate.focusVisible = focus && focus->focusVisible();
    }
    auto impl = std::make_shared<Impl>();
    impl->child = &child;
    impl->parent = &parent;
    impl->completed = std::move(completed);
    Impl::children[&child] = impl;
    auto result = std::unique_ptr<ModalSession>(new ModalSession(impl));
    const auto weak = std::weak_ptr<Impl>(impl);
    impl->childClosed = child.observeClosed([weak] {
        if (auto self = weak.lock())
            self->release(true);
    });
    impl->parentClosed = parent.observeClosed([weak] {
        if (auto self = weak.lock()) {
            modalParents.erase(self->parent);
            self->parent = nullptr;
            if (self->child) {
                self->child->setCloseToTray(false);
                self->child->close();
            }
            self->release(true);
        }
    });
    child.show();
    child.activate();
    return result;
}
ModalSession::~ModalSession() { impl_->release(false); }
bool ModalSession::active() const { return impl_->active; }
void ModalSession::finish(int result) {
    auto self = impl_;
    if (!self->active || !self->child)
        return;
    self->result = result;
    self->child->requestClose();
}
} // namespace oneui
