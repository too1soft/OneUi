#include "oneui/view.h"
#include "oneui/controls/form_field.h"
#include "oneui/controls/tabs.h"
#include "oneui/controls/text_field.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/stack.h"

namespace oneui::internal {

// Geometry-only traversal includes composites whose owned children do not
// belong to View::children(). Reuse their existing layout implementations;
// no event synthesis, raster surface, or new virtual ABI is needed.
void layoutWidgetSubtree(Widget& widget) {
    if (!widget.visible()) return;
    if (auto* stack = dynamic_cast<Stack*>(&widget)) {
        if (stack->engine_ == StackEngine::Yoga) {
            stack->layoutYogaSubtree();
            return;
        }
    }
    auto* view = dynamic_cast<View*>(&widget);
    if (view) view->layoutChildren();

    auto* host = dynamic_cast<OverlayHost*>(&widget);
    if (host) {
        const auto content = host->content();
        if (content && content->visible()) {
            content->setFrame(host->frame());
            layoutWidgetSubtree(*content);
        }
    }
    if (auto* field = dynamic_cast<FormField*>(&widget)) {
        field->layoutChild();
        const auto child = field->child();
        if (child) layoutWidgetSubtree(*child);
    }
    if (auto* tabs = dynamic_cast<Tabs*>(&widget)) {
        if (tabs->editingIndex_ >= 0) tabs->updateEditorFrame();
    }
    if (view) {
        const auto children = view->children();
        for (const auto& child : children)
            if (child) layoutWidgetSubtree(*child);
    }
    if (host) {
        host->layoutAnchoredOverlays();
        const auto overlays = host->overlays();
        for (const auto& entry : overlays)
            if (entry.child) layoutWidgetSubtree(*entry.child);
    }
}

} // namespace oneui::internal
