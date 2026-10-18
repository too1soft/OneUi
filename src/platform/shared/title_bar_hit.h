#pragma once

#include "oneui/view.h"

#include <memory>

namespace oneui::platform {

// In a custom (borderless) title bar the deepest visible widget under the
// pointer takes the click when it is an enabled, focusable control such as a
// button, field or tab. Everything else in the title bar drags the window, so
// applications can place a search box or menu button there without carving
// out interactive regions by hand.
inline bool titleBarControlAt(const std::shared_ptr<Widget>& widget, Point point) {
    if (!widget || !widget->visible() || !widget->frame().contains(point)) return false;
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
