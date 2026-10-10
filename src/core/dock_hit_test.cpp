#include "oneui/layout/dock_model.h"
#include <algorithm>
#include "internal/split_geometry.h"
#include <array>
#include <cmath>

namespace oneui {
namespace {
bool valid(Rect r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) &&
           std::isfinite(r.height) && r.width > 0 && r.height > 0 && std::isfinite(r.x + r.width) &&
           std::isfinite(r.y + r.height);
}
bool contains(Rect r, Point p) {
    return p.x >= r.x && p.y >= r.y && p.x <= r.x + r.width && p.y <= r.y + r.height;
}
DockHitPlacement direction(std::size_t index) { return static_cast<DockHitPlacement>(index + 1); }
Rect preview(Rect r, DockHitPlacement placement) {
    switch (placement) {
    case DockHitPlacement::Left:
        r.width *= .5f;
        break;
    case DockHitPlacement::Right:
        r.width *= .5f;
        r.x += r.width;
        break;
    case DockHitPlacement::Top:
        r.height *= .5f;
        break;
    case DockHitPlacement::Bottom:
        r.height *= .5f;
        r.y += r.height;
        break;
    default:
        break;
    }
    return r;
}
} // namespace
std::optional<DockHit> dockHitTest(DockId source, Point point,
                                   const std::vector<DockHitTarget> &targets, DockHitMode mode) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || int(mode) < 0 || int(mode) > 3)
        return {};
    for (const auto &target : targets) {
        auto r = target.bounds;
        if (!target.pane || target.pane == source || !valid(r) || !contains(r, point))
            continue;
        const bool guide = mode == DockHitMode::GuideStrict || mode == DockHitMode::GuideNearest;
        if (guide && target.floating)
            continue;
        DockHitPlacement placement = DockHitPlacement::None;
        if (mode == DockHitMode::PaneSwap) {
            float x = (point.x - r.x) / r.width, y = (point.y - r.y) / r.height;
            if (x > .28f && x < .72f && y > .28f && y < .72f)
                placement = DockHitPlacement::Swap;
            else {
                placement = direction(detail::nearestNormalizedEdge(x, y));
            }
        } else if (mode == DockHitMode::TabMerge) {
            // Preserve the native tab-session's established 20% edge targets.
            const float x = (point.x - r.x) / r.width, y = (point.y - r.y) / r.height;
            if (x < .2f)
                placement = DockHitPlacement::Left;
            else if (x > .8f)
                placement = DockHitPlacement::Right;
            else if (y < .2f)
                placement = DockHitPlacement::Top;
            else if (y > .8f)
                placement = DockHitPlacement::Bottom;
        } else {
            float cx = r.x + r.width * .5f, cy = r.y + r.height * .5f;
            const std::array<Rect, 4> zones{{{cx - 68, cy - 18, 42, 36},
                                             {cx + 26, cy - 18, 42, 36},
                                             {cx - 21, cy - 58, 42, 36},
                                             {cx - 21, cy + 22, 42, 36}}};
            for (std::size_t i = 0; i < zones.size(); ++i)
                if (contains(zones[i], point)) {
                    placement = direction(i);
                    break;
                }
            if (placement == DockHitPlacement::None) {
                const std::array<float, 4> distance{point.x - r.x, r.x + r.width - point.x,
                                                    point.y - r.y, r.y + r.height - point.y};
                auto closest = std::min_element(distance.begin(), distance.end());
                if (mode == DockHitMode::GuideNearest || *closest <= 42)
                    placement = direction(std::size_t(closest - distance.begin()));
            }
        }
        return DockHit{target.pane, r, preview(r, placement), placement};
    }
    return {};
}
} // namespace oneui
