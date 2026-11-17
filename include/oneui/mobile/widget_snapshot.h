#pragma once
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/view.h"
#include "oneui/controls/slider.h"
#include <functional>

namespace oneui::mobile {
struct WidgetSnapshot {
    std::wstring name;
    Rect bounds;
    AccessibilityRole role;
    bool disabled, selected, checked, focused;
    Rect layoutBounds; // Unclipped geometry for layout/IME verification.
    float minimum=0, maximum=0, value=0, step=0;
    std::weak_ptr<Slider> rangeTarget;
};
// Bounded, read-only semantic geometry. Never includes editor values,
// passwords, service messages or business payloads.
inline std::vector<WidgetSnapshot> widgetSnapshot(const std::shared_ptr<Widget> &root) {
    std::vector<WidgetSnapshot> result;
    const auto intersection = [](Rect a, Rect b) {
        const float x = std::max(a.x, b.x), y = std::max(a.y, b.y);
        return Rect{x, y, std::max(0.f, std::min(a.x + a.width, b.x + b.width) - x),
                    std::max(0.f, std::min(a.y + a.height, b.y + b.height) - y)};
    };
    std::function<void(const std::shared_ptr<Widget> &, unsigned, Rect)> walk;
    walk = [&](const std::shared_ptr<Widget> &widget, unsigned depth, Rect clip) {
        if (!widget || !widget->visible() || depth > 64 || result.size() >= 512)
            return;
        const auto info = widget->accessibilityInfo();
        const auto bounds = intersection(widget->frame(), clip);
        if (!info.name.empty() && bounds.width > 0 && bounds.height > 0) {
            WidgetSnapshot node{info.name.substr(0, 256), bounds, info.role, widget->disabled(),
                              info.state.selected, info.state.checked, widget->focused(), widget->frame()};
            if(auto slider=std::dynamic_pointer_cast<Slider>(widget)){
                node.minimum=static_cast<float>(slider->minimum());node.maximum=static_cast<float>(slider->maximum());
                node.value=static_cast<float>(slider->value());node.step=static_cast<float>(slider->step());node.rangeTarget=slider;
            }
            result.push_back(std::move(node));
        }
        if (std::dynamic_pointer_cast<ScrollView>(widget))
            clip = bounds;
        if (auto host = std::dynamic_pointer_cast<OverlayHost>(widget)) {
            const OverlayEntry *modal = nullptr;
            for (const auto &entry : host->overlays())
                if (entry.child && entry.child->visible() && entry.trapsFocus &&
                    (!modal || entry.layer >= modal->layer))
                    modal = &entry;
            // A modal sheet exposes its own controls, not the inactive page.
            if (modal) {
                walk(modal->child, depth + 1, clip);
                return;
            }
            walk(host->content(), depth + 1, clip);
            for (const auto &entry : host->overlays())
                walk(entry.child, depth + 1, clip);
            return;
        }
        if (auto view = std::dynamic_pointer_cast<View>(widget))
            for (const auto &child : view->children())
                walk(child, depth + 1, clip);
    };
    if (root)
        walk(root, 0, root->frame());
    return result;
}
} // namespace oneui::mobile
