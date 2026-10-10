#include "internal/split_geometry.h"
#include "oneui/layout/workspace_tree.h"
#include <cmath>
#include <unordered_set>

namespace oneui {
namespace {
bool key(const std::string &s) { return !s.empty() && s.find('\0') == std::string::npos; }
bool sizeValid(Size s) {
    return std::isfinite(s.width) && std::isfinite(s.height) && s.width >= 0 && s.height >= 0;
}
} // namespace
DockError DockTreeIndex::reset(std::vector<WorkspaceNode> nodes, std::vector<WorkspaceGroup> groups,
                               std::vector<WorkspacePane> panes, float gap) {
    if (nodes.size() > nodeLimit || groups.size() > nodeLimit || panes.size() > nodeLimit ||
        !std::isfinite(gap) || gap < 0)
        return DockError::InvalidGeometry;
    DockTreeIndex next;
    next.nodes_ = std::move(nodes);
    next.links_.resize(next.nodes_.size());
    next.gap_ = gap;
    std::unordered_map<std::string, Size> sizes;
    for (const auto &p : panes) {
        if (!key(p.id) || !sizeValid(p.minimum))
            return DockError::InvalidGeometry;
        if (!sizes.emplace(p.id, p.minimum).second)
            return DockError::DuplicateId;
    }
    for (size_t i = 0; i < next.nodes_.size(); ++i) {
        const auto &n = next.nodes_[i];
        if (!key(n.id))
            return DockError::InvalidTarget;
        if (!next.ids_.emplace(n.id, i).second)
            return DockError::DuplicateId;
    }
    std::unordered_set<std::string> leaves;
    for (size_t i = 0; i < next.nodes_.size(); ++i) {
        const auto &n = next.nodes_[i];
        if (!std::isfinite(n.ratio) || n.ratio <= 0 || n.ratio >= 1 ||
            (n.axis != DockAxis::Horizontal && n.axis != DockAxis::Vertical))
            return DockError::InvalidGeometry;
        if (!n.pane.empty()) {
            if (!n.first.empty() || !n.second.empty() || !sizes.count(n.pane))
                return DockError::MissingPane;
            if (!leaves.insert(n.pane).second)
                return DockError::DuplicateId;
            next.links_[i].minimum = sizes.at(n.pane);
        } else {
            if (!next.ids_.count(n.first) || !next.ids_.count(n.second) || n.first == n.second)
                return DockError::InvalidTarget;
            next.links_[i].first = next.ids_.at(n.first);
            next.links_[i].second = next.ids_.at(n.second);
        }
    }
    std::vector<unsigned char> state(next.nodes_.size());
    std::vector<std::pair<size_t, bool>> pending;
    for (const auto &g : groups) {
        if (!key(g.id) || (!g.root.empty() && !next.ids_.count(g.root)))
            return DockError::InvalidTarget;
        const auto root = g.root.empty() ? missing : next.ids_.at(g.root);
        if (!next.roots_.emplace(g.id, root).second)
            return DockError::DuplicateId;
        if (root == missing)
            continue;
        pending.emplace_back(root, false);
        while (!pending.empty()) {
            const auto [at, visited] = pending.back();
            pending.pop_back();
            auto &link = next.links_[at];
            if (visited) {
                link.minimum = detail::splitMinimum(next.links_[link.first].minimum,
                                                    next.links_[link.second].minimum, gap,
                                                    next.nodes_[at].axis == DockAxis::Horizontal);
                if (!sizeValid(link.minimum))
                    return DockError::InvalidGeometry;
                state[at] = 2;
                continue;
            }
            // A second visit is a cycle or a multiple-parent reference, including across groups.
            if (state[at])
                return DockError::InvalidTarget;
            state[at] = 1;
            if (link.first == missing)
                state[at] = 2;
            else {
                pending.emplace_back(at, true);
                pending.emplace_back(link.second, false);
                pending.emplace_back(link.first, false);
            }
        }
    }
    if (std::any_of(state.begin(), state.end(), [](auto s) { return s != 2; }))
        return DockError::InvalidTarget;
    *this = std::move(next);
    return DockError::None;
}
Size DockTreeIndex::minimum(const std::string &group) const {
    auto found = roots_.find(group);
    return found == roots_.end() || found->second == missing ? Size{}
                                                             : links_[found->second].minimum;
}
} // namespace oneui
