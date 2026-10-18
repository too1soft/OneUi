#include "../views/workspace_drag_external.h"
#include "oneui/controls/tabs.h"
#include "oneui/layout/panel.h"

OneUiViewResult oneui_view_mount_register_workspace_tabs(OneUiViewMount* handle,
    OneUiUtf8String domain, OneUiUtf8String id, OneUiWidget* tabs) {
    return call(__func__, [&] {
        check(handle && tabs && tabs->widget, ONEUI_VIEW_INVALID_ARGUMENT, "Mount and tabs required");
        auto mount=handle->value;
        mount->check();
        check(bool(std::dynamic_pointer_cast<oneui::Tabs>(tabs->widget)) ||
              bool(std::dynamic_pointer_cast<oneui::Panel>(tabs->widget)),
              ONEUI_VIEW_INVALID_ARGUMENT, "Workspace strip must be Tabs or a declarative Panel host");
        check(contains(mount->window->rootContent,tabs->widget.get()),
              ONEUI_VIEW_INVALID_ARGUMENT, "Tabs must belong to the mount window");
        auto value=mount->active.lock();
        check(bool(value),ONEUI_VIEW_CLOSED,"No active view");
        registerWorkspaceTabs(*value,text(domain),text(id),tabs->widget);
    });
}
OneUiViewResult oneui_view_workspace_tab_drag(OneUiView* view, OneUiUtf8String domain,
    OneUiUtf8String source, int phase, float x, float y) {
    return call(__func__, [&] {
        workspaceTabDrag(*runtime(view),text(domain),text(source),phase,{x,y});
    });
}
