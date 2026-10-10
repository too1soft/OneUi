#include "internal/split_geometry.h"
#include "oneui/layout/workspace_tree.h"
#include <cmath>

namespace oneui {
DockError DockTreeIndex::layout(const std::string &group, Rect viewport, WorkspaceGeometry &output,
                                const std::string &maximized) const {
    auto root = roots_.find(group);
    if (root == roots_.end())
        return DockError::InvalidTarget;
    if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y) ||
        !std::isfinite(viewport.width) || !std::isfinite(viewport.height) || viewport.width < 0 ||
        viewport.height < 0)
        return DockError::InvalidGeometry;
    WorkspaceGeometry next;
    next.content = {viewport.width, viewport.height};
    if (root->second == missing) {
        if (!maximized.empty())
            return DockError::MissingPane;
        output = std::move(next);
        return DockError::None;
    }
    if (maximized.empty()) {
        const auto m = links_[root->second].minimum;
        viewport.width = std::max(viewport.width, m.width);
        viewport.height = std::max(viewport.height, m.height);
        next.content = {viewport.width, viewport.height};
    }
    if (!std::isfinite(viewport.x + viewport.width) || !std::isfinite(viewport.y + viewport.height))
        return DockError::InvalidGeometry;
    std::vector<std::pair<size_t, Rect>> pending{{root->second, viewport}};
    while (!pending.empty()) {
        const auto [at, rect] = pending.back();
        pending.pop_back();
        const auto &n = nodes_[at];
        const auto &link = links_[at];
        if (link.first == missing) {
            if (maximized.empty())
                next.panes.push_back({n.pane, rect});
            else if (n.pane == maximized)
                next.panes.push_back({n.pane, viewport});
            continue;
        }
        const bool horizontal = n.axis == DockAxis::Horizontal;
        const auto gap = std::min(gap_, horizontal ? rect.width : rect.height);
        const auto available = (horizontal ? rect.width : rect.height) - gap;
        const auto a = links_[link.first].minimum, b = links_[link.second].minimum;
        const auto amount = detail::splitAmount(available, n.ratio, horizontal ? a.width : a.height,
                                                horizontal ? b.width : b.height);
        const auto r = detail::splitRects(rect, gap, amount, horizontal);
        if (maximized.empty())
            next.dividers.push_back({n.id, r.divider});
        pending.emplace_back(link.second, r.second);
        pending.emplace_back(link.first, r.first);
    }
    if (!maximized.empty() && next.panes.empty())
        return DockError::MissingPane;
    output = std::move(next);
    return DockError::None;
}
} // namespace oneui
