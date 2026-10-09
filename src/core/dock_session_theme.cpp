#include "dock_session_internal.h"
#include "oneui/ui_native_theme.h"

namespace oneui {
void DockSession::Impl::applyTheme(const std::shared_ptr<Widget> &root) {
    if (!styleSheet || !root)
        return;
    const auto sheet = styleSheet;
    const bool previous = busy;
    struct Restore {
        bool &flag;
        bool previous;
        ~Restore() { flag = previous; }
    } restore{busy, previous};
    busy = true;
    root->setStyleScope(sheet);
    std::function<void(const std::shared_ptr<Widget> &)> apply = [&](const auto &widget) {
        ui::applyNativeTheme(*widget, *sheet, false);
        if (auto view = std::dynamic_pointer_cast<View>(widget)) {
            auto children = view->children();
            for (auto &child : children)
                apply(child);
        }
    };
    apply(root);
}
DockSessionResult DockSession::setStyleSheet(std::shared_ptr<StyleSheet> sheet) {
    auto self = impl_;
    self->check();
    if (self->busy)
        return DockSessionResult::Busy;
    if (!sheet || self->stopped)
        return DockSessionResult::Failed;
    std::vector<std::shared_ptr<Widget>> roots;
    for (auto &[id, host] : self->hosts)
        roots.push_back(host.surface);
    for (auto &[id, pane] : self->panes)
        roots.push_back(pane.content);
    self->styleSheet = std::move(sheet);
    try {
        for (auto &root : roots)
            self->applyTheme(root);
        return self->stopped ? DockSessionResult::Failed : DockSessionResult::Applied;
    } catch (...) {
        return DockSessionResult::Failed;
    }
}
} // namespace oneui
