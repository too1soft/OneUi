#include "runtime.h"

namespace oneui::views {
// Structural collections do not insert a native Column between pool and items.
// The REPT Instance is semantic only; its empty widget is never mounted.
void Runtime::workspaceChildren(const std::shared_ptr<Instance> &instance) {
    const auto &type = instance->element.component;
    if (type != "PaneGrid" && type != "DockWorkspace")
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
    auto parent = std::dynamic_pointer_cast<View>(instance->element.widget);
    if (parent->children() == widgets)
        return;
    auto plan = parent->prepareChildren(std::move(widgets));
    if (staging && instance->published)
        commits.push_back([plan] { plan->commit(); });
    else
        plan->commit();
}
} // namespace oneui::views
