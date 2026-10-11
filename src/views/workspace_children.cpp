#include "runtime.h"

namespace oneui::views {
// Structural collections do not insert a native Column between pool and items.
// The REPT Instance is semantic only; its empty widget is never mounted.
void Runtime::workspaceChildren(const std::shared_ptr<Instance> &instance) {
    const auto &type = instance->element.component;
    if (type != "PaneGrid" && type != "DockWorkspace" && type != "ReorderableGrid")
        return;
    std::vector<std::shared_ptr<Widget>> widgets;
    for (const auto &child : instance->children) {
        if (!child->transparent)
            widgets.push_back(child->element.widget);
        else {
            auto &items = list(
                child->scopes.at(
                    image.records[oneview::REPT][image.records[oneview::NODE][child->node][9]][3] &
                    0xffff),
                image.records[oneview::REPT][image.records[oneview::NODE][child->node][9]][1]);
            const auto &rows = child->pendingRows ? *child->pendingRows : child->rows;
            for (const auto &key : items.keys)
                widgets.push_back(rows.at(key)->element.widget);
        }
    }
    if(type=="ReorderableGrid") {
        // Validate the complete projection before publishing a list transaction.
        auto items=oneui::templateGridItems(widgets);
        auto grid=instance->element.as<oneui::ReorderableGrid>();
        auto commit=[grid,items=std::move(items)]() mutable {grid->setItems(std::move(items));};
        if(staging && instance->published)commits.push_back(std::move(commit));
        else commit();
        return;
    }
    // Identity-only workspaces (including ABI 44 scope fixtures) have no dock
    // projection. Keep their structural children usable without inventing a
    // geometry snapshot; authored geometry still validates transactionally.
    if (type == "DockWorkspace" && instance->workspaceCache) {
        prepareDock(instance, widgets);
        return;
    }
    auto parent = std::dynamic_pointer_cast<View>(instance->element.widget);
    auto plan = parent->children()==widgets ? nullptr : parent->prepareChildren(widgets);
    if(plan) {
    if (staging && instance->published)
        commits.push_back([plan] { plan->commit(); });
    else
        plan->commit();
    }
    if (type == "PaneGrid")
        prepareWorkspace(instance,widgets);
}
} // namespace oneui::views
