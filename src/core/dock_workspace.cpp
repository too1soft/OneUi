#include "oneui/layout/dock_model.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <tuple>

namespace oneui {
namespace {
bool rectangle(Rect r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) &&
           std::isfinite(r.height) && r.width > 0 && r.height > 0;
}
bool ratio(float r) { return std::isfinite(r) && r > 0 && r < 1; }
std::optional<std::tuple<DockId, DockEdge, float>> location(const DockNode &root, DockId id) {
    if (root.pane || !root.first || !root.second)
        return {};
    const bool horizontal = root.axis == DockAxis::Horizontal;
    if (root.first->pane == id)
        return {
            {root.second->ids().front(), horizontal ? DockEdge::Left : DockEdge::Top, root.ratio}};
    if (root.second->pane == id)
        return {{root.first->ids().front(), horizontal ? DockEdge::Right : DockEdge::Bottom,
                 root.ratio}};
    auto first = location(*root.first, id);
    return first ? first : location(*root.second, id);
}
} // namespace
DockError DockWorkspace::restore(DockSnapshot state) {
    std::set<DockId> seen;
    if (state.root) {
        auto error = state.root->validate();
        if (error != DockError::None)
            return error;
        const auto ids = state.root->ids();
        seen.insert(ids.begin(), ids.end());
    }
    std::uint64_t sequence = 0;
    if (state.floating.size() + seen.size() > 1024 || state.hidden.size() > 1024)
        return DockError::InvalidGeometry;
    for (auto &[id, pane] : state.floating) {
        if (!id || !seen.insert(id).second)
            return DockError::DuplicateId;
        if (!rectangle(pane.bounds))
            return DockError::InvalidGeometry;
        sequence = std::max(sequence, pane.order);
    }
    if (state.maximized && !seen.count(state.maximized))
        return DockError::MissingPane;
    for (auto &[id, old] : state.hidden) {
        if (!id || !ratio(old.ratio))
            return DockError::InvalidGeometry;
        for (auto *root : {&old.before, &old.after})
            if (*root) {
                auto error = (*root)->validate();
                if (error != DockError::None)
                    return error;
            }
        if (old.floating && !rectangle(old.floating->bounds))
            return DockError::InvalidGeometry;
    }
    state_ = std::move(state);
    sequence_ = sequence;
    return DockError::None;
}
bool DockWorkspace::visible(DockId id) const {
    return state_.floating.count(id) || (state_.root && state_.root->contains(id));
}
void DockWorkspace::remember(DockId id) {
    if (auto found = state_.floating.find(id); found != state_.floating.end()) {
        state_.hidden[id].floating = found->second;
        return;
    }
    if (!state_.root || !state_.root->contains(id))
        return;
    DockRestore old;
    old.before = state_.root;
    old.after = state_.root->without(id);
    if (auto where = location(*state_.root, id))
        std::tie(old.target, old.edge, old.ratio) = *where;
    state_.hidden[id] = std::move(old);
}
DockError DockWorkspace::hide(DockId id) {
    if (!visible(id))
        return DockError::MissingPane;
    remember(id);
    if (state_.root)
        state_.root = state_.root->without(id);
    state_.floating.erase(id);
    if (state_.maximized == id)
        state_.maximized = 0;
    return DockError::None;
}
DockError DockWorkspace::insert(DockId id, DockId target, DockEdge edge, float r) {
    if (!state_.root || !state_.root->contains(target))
        return DockError::InvalidTarget;
    const bool before = edge == DockEdge::Left || edge == DockEdge::Top;
    DockNode next(edge == DockEdge::Left || edge == DockEdge::Right ? DockAxis::Horizontal
                                                                    : DockAxis::Vertical,
                  r, DockNode(before ? id : target), DockNode(before ? target : id));
    state_.root->replace(target, std::move(next));
    return DockError::None;
}
DockError DockWorkspace::show(DockId id) {
    if (visible(id))
        return DockError::None;
    auto found = state_.hidden.find(id);
    if (found == state_.hidden.end())
        return DockError::MissingPane;
    const auto old = found->second;
    if (old.floating) {
        state_.floating[id] = *old.floating;
        raise(id);
    } else {
        bool canRestore = state_.root == old.after && bool(old.before);
        if (canRestore)
            for (auto key : old.before->ids())
                if (key != id && state_.floating.count(key))
                    canRestore = false;
        if (canRestore)
            state_.root = old.before;
        else if (old.target && state_.root && state_.root->contains(old.target))
            insert(id, old.target, old.edge, old.ratio);
        else if (state_.root)
            state_.root =
                DockNode(DockAxis::Horizontal, 0.7f, std::move(*state_.root), DockNode(id));
        else
            state_.root = DockNode(id);
    }
    state_.maximized = 0;
    return DockError::None;
}
DockError DockWorkspace::dock(DockId id, DockId target, DockEdge edge) {
    if (id == target || !visible(id) || !state_.root || !state_.root->contains(target) ||
        int(edge) < 0 || int(edge) > 3)
        return DockError::InvalidTarget;
    if (state_.root)
        state_.root = state_.root->without(id);
    state_.floating.erase(id);
    insert(id, target, edge, 0.5f);
    state_.hidden.erase(id);
    state_.maximized = 0;
    return DockError::None;
}
DockError DockWorkspace::floatPane(DockId id, Rect bounds) {
    if (!rectangle(bounds))
        return DockError::InvalidGeometry;
    if (!visible(id))
        return DockError::MissingPane;
    if (!state_.floating.count(id)) {
        remember(id);
        if (state_.root)
            state_.root = state_.root->without(id);
    }
    const bool pinned = state_.floating.count(id) && state_.floating.at(id).pinned;
    state_.floating[id] = {bounds, pinned, 0};
    raise(id);
    state_.maximized = 0;
    return DockError::None;
}
DockError DockWorkspace::redock(DockId id) {
    if (!state_.floating.count(id))
        return DockError::MissingPane;
    if (!state_.hidden.count(id)) {
        DockRestore old;
        if (state_.root)
            old.target = state_.root->ids().front();
        state_.hidden[id] = std::move(old);
    }
    state_.floating.erase(id);
    state_.hidden.at(id).floating.reset();
    return show(id);
}
DockError DockWorkspace::maximize(DockId id) {
    if (!visible(id))
        return DockError::MissingPane;
    state_.maximized = state_.maximized == id ? 0 : id;
    return DockError::None;
}
DockError DockWorkspace::setPinned(DockId id, bool pinned) {
    auto found = state_.floating.find(id);
    if (found == state_.floating.end())
        return DockError::MissingPane;
    found->second.pinned = pinned;
    return raise(id);
}
std::vector<DockId> DockWorkspace::paintOrder() const {
    std::vector<DockId> ids;
    for (auto &entry : state_.floating)
        ids.push_back(entry.first);
    std::stable_sort(ids.begin(), ids.end(), [&](auto a, auto b) {
        const auto &x = state_.floating.at(a);
        const auto &y = state_.floating.at(b);
        return std::tie(x.pinned, x.order) < std::tie(y.pinned, y.order);
    });
    return ids;
}
DockError DockWorkspace::raise(DockId id) {
    if (!state_.floating.count(id))
        return DockError::MissingPane;
    if (sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        sequence_ = 0;
        for (auto key : paintOrder())
            state_.floating.at(key).order = sequence_++;
    }
    state_.floating.at(id).order = ++sequence_;
    return DockError::None;
}
DockError DockWorkspace::setFloatBounds(DockId id, Rect bounds) {
    if (!rectangle(bounds))
        return DockError::InvalidGeometry;
    auto found = state_.floating.find(id);
    if (found == state_.floating.end())
        return DockError::MissingPane;
    found->second.bounds = bounds;
    return DockError::None;
}
DockError DockWorkspace::swap(DockId a, DockId b) {
    return state_.root ? state_.root->swap(a, b) : DockError::MissingPane;
}
DockError DockWorkspace::setRatio(const std::vector<bool> &path, float ratio) {
    return state_.root ? state_.root->setRatio(path, ratio) : DockError::InvalidTarget;
}
void DockWorkspace::balance() {
    if (state_.root)
        state_.root->balance();
}
} // namespace oneui
