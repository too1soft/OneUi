#include "oneui/layout/dock_model.h"
#include <algorithm>
#include <cmath>

namespace oneui {
namespace {
bool finiteRect(Rect r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) &&
           std::isfinite(r.height) && r.width >= 0 && r.height >= 0;
}
bool finiteSize(Size size) {
    return std::isfinite(size.width) && std::isfinite(size.height) && size.width >= 0 &&
           size.height >= 0;
}
void geometry(const DockNode &node, Rect bounds, float gap, const DockMinimum &minimum,
              std::vector<bool> path, DockGeometry &result) {
    if (node.pane) {
        result.panes[node.pane] = bounds;
        return;
    }
    const bool horizontal = node.axis == DockAxis::Horizontal;
    const auto extent = horizontal ? bounds.width : bounds.height;
    gap = std::min(gap, extent);
    const auto available = extent - gap;
    const auto firstMin = dockMinimum(*node.first, gap, minimum),
               secondMin = dockMinimum(*node.second, gap, minimum);
    const auto a = horizontal ? firstMin.width : firstMin.height,
               b = horizontal ? secondMin.width : secondMin.height;
    const auto amount = a + b <= available ? std::clamp(available * node.ratio, a, available - b)
                                           : available * node.ratio;
    auto first = bounds, second = bounds, divider = bounds;
    if (horizontal) {
        first.width = amount;
        second.x += amount + gap;
        second.width = available - amount;
        divider.x += amount;
        divider.width = gap;
    } else {
        first.height = amount;
        second.y += amount + gap;
        second.height = available - amount;
        divider.y += amount;
        divider.height = gap;
    }
    result.dividers.push_back({path, node.axis, divider});
    path.push_back(false);
    geometry(*node.first, first, gap, minimum, path, result);
    path.back() = true;
    geometry(*node.second, second, gap, minimum, std::move(path), result);
}
} // namespace
Size dockMinimum(const DockNode &node, float gap, const DockMinimum &minimum) {
    if (node.pane)
        return minimum(node.pane);
    if (!node.first || !node.second)
        return {};
    const auto a = dockMinimum(*node.first, gap, minimum),
               b = dockMinimum(*node.second, gap, minimum);
    return node.axis == DockAxis::Horizontal
               ? Size{a.width + b.width + gap, std::max(a.height, b.height)}
               : Size{std::max(a.width, b.width), a.height + b.height + gap};
}
DockError dockGeometry(const DockNode &root, Rect bounds, float gap, const DockMinimum &minimum,
                       DockGeometry &result) {
    const auto error = root.validate();
    if (error != DockError::None)
        return error;
    if (!finiteRect(bounds) || !std::isfinite(gap) || gap < 0 || !minimum)
        return DockError::InvalidGeometry;
    // Evaluate user policy once per pane, both for consistency and bounded calls.
    std::map<DockId, Size> sizes;
    for (auto id : root.ids()) {
        auto size = minimum(id);
        if (!finiteSize(size))
            return DockError::InvalidGeometry;
        sizes[id] = size;
    }
    DockGeometry next;
    geometry(root, bounds, gap, [&](DockId id) { return sizes.at(id); }, {}, next);
    result = std::move(next);
    return DockError::None;
}
DockError clampDockFloating(Rect bounds, Rect workspace, Size minimum, Rect &result) {
    if (!finiteRect(bounds) || !finiteRect(workspace) || !finiteSize(minimum))
        return DockError::InvalidGeometry;
    const auto width = std::min(std::max(bounds.width, minimum.width), workspace.width);
    const auto height = std::min(std::max(bounds.height, minimum.height), workspace.height);
    result = {std::clamp(bounds.x, workspace.x, workspace.x + workspace.width - width),
              std::clamp(bounds.y, workspace.y, workspace.y + workspace.height - height), width,
              height};
    return DockError::None;
}
DockError resizeDockFloating(Rect start, Point delta, DockResizeEdge edge, Rect workspace,
                             Size minimum, Rect &output) {
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || int(edge) < 0 || int(edge) > 7)
        return DockError::InvalidGeometry;
    Rect result;
    auto error = clampDockFloating(start, workspace, minimum, result);
    if (error != DockError::None)
        return error;
    start = result;
    const auto w = std::min(minimum.width, workspace.width),
               h = std::min(minimum.height, workspace.height);
    if (edge == DockResizeEdge::East || edge == DockResizeEdge::NorthEast ||
        edge == DockResizeEdge::SouthEast)
        result.width =
            std::clamp(start.width + delta.x, w, workspace.x + workspace.width - start.x);
    if (edge == DockResizeEdge::South || edge == DockResizeEdge::SouthEast ||
        edge == DockResizeEdge::SouthWest)
        result.height =
            std::clamp(start.height + delta.y, h, workspace.y + workspace.height - start.y);
    if (edge == DockResizeEdge::West || edge == DockResizeEdge::NorthWest ||
        edge == DockResizeEdge::SouthWest) {
        result.x = std::clamp(start.x + delta.x, workspace.x, start.x + start.width - w);
        result.width = start.x + start.width - result.x;
    }
    if (edge == DockResizeEdge::North || edge == DockResizeEdge::NorthEast ||
        edge == DockResizeEdge::NorthWest) {
        result.y = std::clamp(start.y + delta.y, workspace.y, start.y + start.height - h);
        result.height = start.y + start.height - result.y;
    }
    output = result;
    return DockError::None;
}
} // namespace oneui
