// Independent typed child views reuse the existing Mount lifecycle and factory.
namespace {
void createChildMount(OneUiViewMount* handle, OneUiUtf8String name,
                      OneUiWidget* nativeDestination, OneUiViewMount** out) {
    check(handle && out, ONEUI_VIEW_INVALID_ARGUMENT, "Parent mount and output required");
    auto parent = handle->value;
    parent->check();
    check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Child mount during callback");
    auto view = parent->active.lock();
    check(bool(view), ONEUI_VIEW_CLOSED, "No active parent view");
    view->alive();
    auto widget = namedViewElement(view, name);
    std::shared_ptr<Instance> slot;
    std::vector<std::shared_ptr<Instance>> pending{view->root};
    while (!pending.empty()) {
        auto current = pending.back(); pending.pop_back();
        if (current->element.widget == widget) slot = current;
        pending.insert(pending.end(), current->children.begin(), current->children.end());
        for (auto& row : current->rows) pending.push_back(row.second);
    }
    check(slot && slot->element.component == "Surface" && slot->children.empty(),
          ONEUI_VIEW_INVALID_ARGUMENT, "Child destination must be an empty declared Surface");
    auto destination = std::dynamic_pointer_cast<oneui::View>(widget);
    check(bool(destination), ONEUI_VIEW_TYPE, "Surface does not provide a content slot");
    check(destination->children().empty(), ONEUI_VIEW_BUSY, "Child destination occupied");
    for (auto& weak : parent->children) if (auto existing = weak.lock())
        check(existing->closed || (existing->destination != destination &&
              existing->childSurface.lock() != destination),
              ONEUI_VIEW_BUSY, "Child destination reserved");
    std::shared_ptr<oneui::View> panel;
    if (nativeDestination) {
        check(nativeDestination->widget != destination &&
              !contains(nativeDestination->widget, destination.get()),
              ONEUI_VIEW_INVALID_ARGUMENT, "Destination would create a cycle");
        panel = std::dynamic_pointer_cast<oneui::Panel>(nativeDestination->widget);
        check(bool(panel), ONEUI_VIEW_TYPE, "Destination must be a Panel");
        check(panel->children().empty(), ONEUI_VIEW_INVALID_ARGUMENT,
              "Destination Panel must be empty");
        check(!panel->hasCompositionOwner() && !contains(parent->window->rootContent, panel.get()),
              ONEUI_VIEW_BUSY, "Destination Panel already attached");
    }
    auto child = std::make_shared<Mount>();
    child->window = parent->window;
    child->windowRoot = parent->windowRoot;
    child->destination = panel ? panel : destination;
    if (panel) {
        child->childSurface = destination;
        child->ownsDestination = true;
    }
    child->externalCss = parent->externalCss;
    child->density = parent->density;
    child->child = true;
    child->parentView = view;
    auto weak = std::weak_ptr<Mount>(child);
    child->closedSubscription = child->window->window->observeClosed([weak] {
        if (auto value = weak.lock()) value->close(true);
    });
    auto result = std::make_unique<OneUiViewMount>();
    result->value = child;
    if (panel) reserveChildDestination(panel, child);
    // Preparation rolls back callback ownership if registration allocates
    // unsuccessfully. No externally visible tree change precedes commit.
    auto attachment = panel ? destination->prepareChildren({panel}) : nullptr;
    parent->children.push_back(child);
    registerMount(child->window, child);
    if (attachment) attachment->commit();
    *out = result.release();
}
} // namespace
OneUiViewResult oneui_view_mount_child(OneUiViewMount* handle, OneUiUtf8String name,
                                      OneUiViewMount** out) {
    if (out) *out = nullptr;
    return call(__func__, [&] { createChildMount(handle, name, nullptr, out); });
}
OneUiViewResult oneui_view_mount_child_with_destination(OneUiViewMount* handle,
    OneUiUtf8String name, OneUiWidget* destination, OneUiViewMount** out) {
    if (out) *out = nullptr;
    return call(__func__, [&] {
        check(destination && destination->widget, ONEUI_VIEW_INVALID_ARGUMENT,
              "Destination Panel required");
        createChildMount(handle, name, destination, out);
    });
}
