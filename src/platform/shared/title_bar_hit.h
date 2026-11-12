#pragma once

#include "oneui/view.h"
#include "oneui/layout/overlay_host.h"

#include <algorithm>
#include <memory>
#include <numeric>

namespace oneui::platform {

// In a custom (borderless) title bar the deepest visible widget under the
// pointer takes the click when it is an enabled, focusable control such as a
// button, field or tab. Everything else in the title bar drags the window, so
// applications can place a search box or menu button there without carving
// out interactive regions by hand.
inline bool titleBarControlAt(const std::shared_ptr<Widget>& widget, Point point) {
    if (!widget || !widget->visible() || !widget->frame().contains(point)) return false;
    // OverlayHost owns its base and layers outside View::children(). The
    // application root itself is often one, not just the small shortcut badge.
    if (const auto overlay = std::dynamic_pointer_cast<OverlayHost>(widget)) {
        const auto& layers = overlay->overlays();
        std::vector<std::size_t> order(layers.size());
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
            return layers[a].layer < layers[b].layer;
        });
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            const auto& layer = layers[*it];
            if (!layer.child || !layer.child->visible() || layer.child->disabled()) continue;
            if (layer.child->hitTest(point)) return titleBarControlAt(layer.child, point);
            // Modal pointer blocking belongs to OverlayHost's event dispatch,
            // not native caption classification. Keep real controls as client
            // area while allowing a blank caption to move the entire window.
            if (layer.blocksOutsidePointer) break;
        }
        if (overlay->content() && overlay->content()->hitTest(point))
            return titleBarControlAt(overlay->content(), point);
    }
    if (const auto view = std::dynamic_pointer_cast<View>(widget)) {
        const auto& children = view->children();
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            if (*it && (*it)->visible() && (*it)->frame().contains(point))
                return titleBarControlAt(*it, point);
        // Containers (which may report focusable on behalf of their
        // children) never take the click themselves.
        return false;
    }
    return widget->isFocusable() && !widget->disabled();
}

} // namespace oneui::platform
