#include "oneui/layout/dock_model.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace oneui {
DockNode::DockNode() = default;
DockNode::DockNode(DockId id) : pane(id) {}
DockNode::DockNode(DockAxis a, float r, DockNode one, DockNode two)
    : axis(a), ratio(r), first(std::make_unique<DockNode>(std::move(one))),
      second(std::make_unique<DockNode>(std::move(two))) {}
DockNode::DockNode(const DockNode &other)
    : pane(other.pane), axis(other.axis), ratio(other.ratio),
      first(other.first ? std::make_unique<DockNode>(*other.first) : nullptr),
      second(other.second ? std::make_unique<DockNode>(*other.second) : nullptr) {}
DockNode &DockNode::operator=(const DockNode &other) {
    if (this != &other)
        *this = DockNode(other);
    return *this;
}
bool DockNode::operator==(const DockNode &other) const {
    if (pane || other.pane)
        return pane == other.pane;
    return axis == other.axis && ratio == other.ratio && first && second && other.first &&
           other.second && *first == *other.first && *second == *other.second;
}
bool DockNode::contains(DockId id) const {
    return pane ? pane == id : (first && first->contains(id)) || (second && second->contains(id));
}
std::vector<DockId> DockNode::ids() const {
    if (pane)
        return {pane};
    auto result = first ? first->ids() : std::vector<DockId>{};
    if (second) {
        auto rest = second->ids();
        result.insert(result.end(), rest.begin(), rest.end());
    }
    return result;
}
std::optional<DockNode> DockNode::without(DockId id) const {
    if (pane)
        return pane == id ? std::optional<DockNode>{} : *this;
    auto a = first ? first->without(id) : std::optional<DockNode>{};
    auto b = second ? second->without(id) : std::optional<DockNode>{};
    if (a && b)
        return DockNode(axis, ratio, std::move(*a), std::move(*b));
    return a ? std::move(a) : std::move(b);
}
bool DockNode::replace(DockId id, DockNode next) {
    if (pane == id && id) {
        *this = std::move(next);
        return true;
    }
    if (pane)
        return false;
    if (first && first->contains(id))
        return first->replace(id, std::move(next));
    return second && second->replace(id, std::move(next));
}
DockError DockNode::swap(DockId a, DockId b) {
    if (!contains(a) || !contains(b))
        return DockError::MissingPane;
    const auto walk = [&](auto &&self, DockNode &node) -> void {
        if (node.pane) {
            if (node.pane == a)
                node.pane = b;
            else if (node.pane == b)
                node.pane = a;
        } else {
            self(self, *node.first);
            self(self, *node.second);
        }
    };
    walk(walk, *this);
    return DockError::None;
}
void DockNode::balance() {
    if (!pane && first && second) {
        ratio = 0.5f;
        first->balance();
        second->balance();
    }
}
DockError DockNode::setRatio(const std::vector<bool> &path, float value) {
    if (!std::isfinite(value) || value <= 0 || value >= 1)
        return DockError::InvalidGeometry;
    auto *target = this;
    for (bool side : path) {
        if (target->pane || !target->first || !target->second)
            return DockError::InvalidTarget;
        target = side ? target->second.get() : target->first.get();
    }
    if (target->pane || !target->first || !target->second)
        return DockError::InvalidTarget;
    target->ratio = value;
    return DockError::None;
}
DockError DockNode::validate() const {
    std::set<DockId> seen;
    const auto walk = [&](auto &&self, const DockNode &node, unsigned depth) -> DockError {
        if (depth > 64 || seen.size() >= 1024)
            return DockError::InvalidGeometry;
        if (node.pane) {
            if (node.first || node.second)
                return DockError::InvalidGeometry;
            return seen.insert(node.pane).second ? DockError::None : DockError::DuplicateId;
        }
        if (!node.first || !node.second || !std::isfinite(node.ratio) || node.ratio <= 0 ||
            node.ratio >= 1 ||
            (node.axis != DockAxis::Horizontal && node.axis != DockAxis::Vertical))
            return DockError::InvalidGeometry;
        auto result = self(self, *node.first, depth + 1);
        return result == DockError::None ? self(self, *node.second, depth + 1) : result;
    };
    return walk(walk, *this, 0);
}
} // namespace oneui
