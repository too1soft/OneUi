#pragma once

#include "internal/accessibility_frame_revision.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/window_title_bar.h"
#include "oneui/layout/overlay_host.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace oneui::win32_accessibility {

struct ChildEntry {
    std::weak_ptr<Widget> widget;
    TitleBarButtonId button = TitleBarButtonId::None;
    long fragmentId = 0;
    std::shared_ptr<Widget> lock() const { return widget.lock(); }
};

inline void collect(const std::shared_ptr<Widget>& widget, std::vector<ChildEntry>& output,
                    std::unordered_set<Widget*>& visited) {
    if (!widget || !widget->visible() || !visited.insert(widget.get()).second ||
        !internal::hasAccessibleFrame(widget->frame())) return;

    const auto info = widget->accessibilityInfo();
    if (info.role != AccessibilityRole::None || !info.name.empty()) {
        output.push_back({widget});
        if (dynamic_cast<WindowTitleBar*>(widget.get())) {
            for (auto id : {TitleBarButtonId::Minimize, TitleBarButtonId::Maximize, TitleBarButtonId::Close}) {
                output.push_back({widget, id});
            }
        }
    }
    if (const auto* host = dynamic_cast<OverlayHost*>(widget.get())) {
        const OverlayEntry* modal = nullptr;
        for (const auto& entry : host->overlays()) {
            if (entry.child && entry.child->visible() && entry.trapsFocus &&
                (!modal || entry.layer >= modal->layer)) modal = &entry;
        }
        if (modal) {
            collect(modal->child, output, visited);
            return;
        }
        collect(host->content(), output, visited);
        for (const auto& entry : host->overlays()) collect(entry.child, output, visited);
    }
    if (const auto* view = dynamic_cast<View*>(widget.get())) {
        for (const auto& child : view->children()) collect(child, output, visited);
    } else if (const auto* popup = dynamic_cast<Popup*>(widget.get()); popup && popup->isOpen()) {
        collect(popup->content(), output, visited);
    }
}

} // namespace oneui::win32_accessibility
