#include "oneui/view.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace oneui {
std::shared_ptr<View::ChildrenUpdate> View::prepareChildren(
    std::vector<std::shared_ptr<Widget>> children) {
    auto plan = std::shared_ptr<ChildrenUpdate>(new ChildrenUpdate(*this));
    plan->next_ = std::move(children);
    std::unordered_set<Widget*> next, previous;
    for (const auto& child : plan->next_)
        if (!child || !next.insert(child.get()).second)
            throw std::invalid_argument("Duplicate or null child");
    for (const auto& child : children_) previous.insert(child.get());
    plan->added_.reserve(plan->next_.size());
    plan->removed_.reserve(children_.size());
    for (const auto& child : children_)
        if (!next.count(child.get())) plan->removed_.push_back(child);
    for (const auto& child : plan->next_) if (!previous.count(child.get())) {
        if (child->hasCompositionOwner())
            throw std::invalid_argument("Child is already mounted");
        plan->added_.push_back(child);
        installChildCallbacks(*child);
    }
    return plan;
}
View::ChildrenUpdate::~ChildrenUpdate() {
    if (!committed_)
        for (const auto& child : added_)
            try { child->detachFromOwner(&owner_); } catch (...) {}
}
void View::ChildrenUpdate::commit() noexcept {
    if (committed_) return;
    committed_ = true;
    if (owner_.children_ == next_) return;
    for (const auto& child : removed_) {
        if (owner_.focusedChild_ == child.get()) {
            owner_.focusedChild_ = nullptr;
            try { child->onFocusChanged(false); } catch (...) {}
        }
        if (owner_.pressedChild_ == child.get()) owner_.pressedChild_ = nullptr;
        if (owner_.hoveredChild_ == child.get()) owner_.hoveredChild_ = nullptr;
        try { child->detachFromOwner(&owner_); } catch (...) {}
    }
    owner_.children_.swap(next_);
    // Application invalidators cannot roll back an already committed tree.
    try { owner_.invalidate(); } catch (...) {}
}
void View::reconcileChildren(std::vector<std::shared_ptr<Widget>> children) {
    // Preserve the original eager reparenting contract (e.g. ApplicationMenuHost
    // moves its document into MenuShell). Only the staged views runtime requires
    // unpublished additions and uses prepareChildren explicitly.
    std::unordered_set<Widget*> keep;
    for (const auto& child : children)
        if (!child || !keep.insert(child.get()).second)
            throw std::invalid_argument("Duplicate or null child");
    if (children == children_) return;
    if (focusedChild_ && !keep.count(focusedChild_)) {
        focusedChild_->onFocusChanged(false);
        focusedChild_ = nullptr;
    }
    if (pressedChild_ && !keep.count(pressedChild_)) pressedChild_ = nullptr;
    if (hoveredChild_ && !keep.count(hoveredChild_)) hoveredChild_ = nullptr;
    std::unordered_set<Widget*> previous;
    for (const auto& child : children_) {
        previous.insert(child.get());
        if (!keep.count(child.get())) child->detachFromOwner(this);
    }
    for (const auto& child : children)
        if (!previous.count(child.get())) installChildCallbacks(*child);
    children_ = std::move(children);
    invalidate();
}
}
