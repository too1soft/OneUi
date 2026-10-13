// Read-only geometry for anchored actions and restoring scroll positions.
#include "oneui/controls/template_table.h"
#include "oneui/controls/popup.h"
#include "oneui/layout/dock_workspace_view.h"
namespace {
std::shared_ptr<oneui::Widget> namedViewElement(const std::shared_ptr<Runtime>& value, OneUiUtf8String name) {
        const auto key = text(name);
        check(!key.empty(), ONEUI_VIEW_INVALID_ARGUMENT, "Element name required");
        std::shared_ptr<oneui::Widget> found;
        std::function<void(const std::shared_ptr<oneui::Widget>&)> visit;
        visit = [&](const auto &widget) {
            if (oneui::views::narrow(widget->accessibleName()) == key) {
                check(!found, ONEUI_VIEW_INVALID_ARGUMENT, "Ambiguous element name");
                found = widget;
            }
            if (auto popup = std::dynamic_pointer_cast<oneui::Popup>(widget)) {
                if (popup->content()) visit(popup->content());
            } else if (auto overlay = std::dynamic_pointer_cast<oneui::OverlayHost>(widget)) {
                if (overlay->content()) visit(overlay->content());
                for (const auto &entry : overlay->overlays()) visit(entry.child);
            } else if (auto container = std::dynamic_pointer_cast<oneui::View>(widget))
                for (const auto &child : container->children()) visit(child);
        };
        visit(value->root->element.widget);
        check(bool(found), ONEUI_VIEW_NOT_FOUND, "Element not found");
        return found;
}
}
OneUiViewResult oneui_view_element_metrics(OneUiView *view, OneUiUtf8String name,
                                          OneUiViewElementMetrics *out) {
    return call(__func__, [&] {
        check(out && out->struct_size == sizeof(*out), ONEUI_VIEW_INVALID_ARGUMENT,
              "Metrics output size");
        const auto value = runtime(view);
        value->alive();
        auto found = namedViewElement(value, name);
        OneUiViewElementMetrics result{};
        result.struct_size = sizeof(result);
        const auto frame = found->frame();
        result.x = frame.x; result.y = frame.y;
        result.width = frame.width; result.height = frame.height;
        if (auto table = std::dynamic_pointer_cast<oneui::TemplateTable>(found)) {
            result.scrollable = 1;
            result.scroll_y = table->scrollOffset();
            result.scroll_x = table->horizontalScrollOffset();
            result.max_scroll_y = table->maxScrollOffset();
        } else if (auto scroll = std::dynamic_pointer_cast<oneui::ScrollView>(found)) {
            result.scrollable = 1;
            result.scroll_y = scroll->scrollOffset();
            result.scroll_x = scroll->horizontalScrollOffset();
            result.max_scroll_y = scroll->maxScrollOffset();
        }
        *out = result;
    });
}

OneUiViewResult oneui_view_dock_item_metrics(OneUiView* view, OneUiUtf8String name,
                                           OneUiUtf8String pane, OneUiViewElementMetrics* out) {
    return call(__func__, [&] {
        check(out && out->struct_size == sizeof(*out), ONEUI_VIEW_INVALID_ARGUMENT, "Metrics output size");
        auto value = runtime(view); value->alive();
        auto dock = std::dynamic_pointer_cast<oneui::DockWorkspaceView>(namedViewElement(value, name));
        check(bool(dock), ONEUI_VIEW_INVALID_ARGUMENT, "Element is not DockWorkspace");
        const auto key = text(pane);
        for (const auto& child : dock->children()) {
            auto item = std::dynamic_pointer_cast<oneui::DockItemView>(child);
            if (!item || item->key != key) continue;
            OneUiViewElementMetrics result{}; result.struct_size = sizeof(result);
            auto r = item->frame();
            result.x = r.x; result.y = r.y; result.width = r.width; result.height = r.height;
            *out = result; return;
        }
        check(false, ONEUI_VIEW_NOT_FOUND, "Dock pane not found");
    });
}

OneUiViewResult oneui_view_dock_action(OneUiView* view, OneUiUtf8String name,
                                      int action, OneUiUtf8String pane, int flag) {
    return call(__func__, [&] {
        check(action >= 0 && action <= 5 && (flag == 0 || flag == 1),
              ONEUI_VIEW_INVALID_ARGUMENT, "Invalid dock action");
        const auto value = runtime(view); value->alive();
        auto dock = std::dynamic_pointer_cast<oneui::DockWorkspaceView>(namedViewElement(value, name));
        check(bool(dock), ONEUI_VIEW_INVALID_ARGUMENT, "Element is not DockWorkspace");
        const auto key = text(pane);
        const auto &placements = dock->snapshot().placements;
        check(std::any_of(placements.begin(), placements.end(), [&](const auto &p) {return p.id == key;}),
              ONEUI_VIEW_NOT_FOUND, "Dock pane not found");
        const oneui::DockIntentKind kinds[] = {oneui::DockIntentKind::Float, oneui::DockIntentKind::Pin,
            oneui::DockIntentKind::Raise, oneui::DockIntentKind::Maximize,
            oneui::DockIntentKind::Visibility, oneui::DockIntentKind::Redock};
        dock->requestAction(kinds[action], key, flag != 0);
    });
}
OneUiViewResult oneui_view_element_focused(OneUiView* view, OneUiUtf8String name,
                                          int* out) {
    if(out)*out=0;
    return call(__func__,[&] {
        check(out,ONEUI_VIEW_INVALID_ARGUMENT,"Focus output required");
        auto value=runtime(view);value->alive();
        *out=namedViewElement(value,name)->focused()?1:0;
    });
}

OneUiViewResult oneui_view_mount_focus_element(OneUiViewMount *handle, OneUiUtf8String name,
                                              int focus_visible, int *out_focused) {
    if (out_focused) *out_focused = 0;
    return call(__func__, [&] {
        check(handle && out_focused, ONEUI_VIEW_INVALID_ARGUMENT, "Mount/output required");
        auto mount = handle->value;
        mount->check();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Focus during VM callback");
        auto view = mount->active.lock();
        check(bool(view), ONEUI_VIEW_CLOSED, "No active view");
        view->alive();
        auto found = namedViewElement(view, name);
        *out_focused = mount->window->window->requestFocus(found.get(), focus_visible != 0) ? 1 : 0;
    });
}
