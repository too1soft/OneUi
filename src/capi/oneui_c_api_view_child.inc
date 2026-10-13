// Independent typed child views reuse the existing Mount lifecycle and factory.
OneUiViewResult oneui_view_mount_child(OneUiViewMount* handle, OneUiUtf8String name,
                                      OneUiViewMount** out) {
    if (out) *out = nullptr;
    return call(__func__, [&] {
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
            check(existing->closed || existing->destination != destination,
                  ONEUI_VIEW_BUSY, "Child destination reserved");
        auto child = std::make_shared<Mount>();
        child->window = parent->window;
        child->windowRoot = parent->windowRoot;
        child->destination = destination;
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
        parent->children.push_back(child);
        registerMount(child->window, child);
        *out = result.release();
    });
}
