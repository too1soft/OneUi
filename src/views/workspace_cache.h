#pragma once
#include "runtime.h"
#include "oneui/layout/pane_grid.h"
#include "oneui/layout/dock_workspace_view.h"
namespace oneui::views {
struct Instance::WorkspaceCache {
    PaneGridSnapshot grid;
    DockWorkspaceSnapshot dock;
    int64_t navigation = 0;
    std::string edge, mode="focus", domain;
    struct TabSource {std::string pane;bool enabled=false;};
    std::map<std::string,TabSource> tabSources;
    std::string dragKind="single-pane-tab";
};
} // namespace oneui::views
