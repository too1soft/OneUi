#include "internal/widget_presentation.h"
#include <cmath>

namespace oneui {
void Stack::setFlexPresentation(const std::shared_ptr<Widget>& child, StackFlexPresentation value) {
    if (!child) return;
    if (value.basisPercent && (!std::isfinite(*value.basisPercent) || *value.basisPercent < 0)) return;
    auto* existing = internal::WidgetPresentationAccess::find(*this);
    if (!value.basisPercent && value.order == 0) {
        if (existing && existing->flex.erase(child.get())) invalidate();
        return;
    }
    auto& items = internal::WidgetPresentationAccess::get(*this).flex;
    for (auto it = items.begin(); it != items.end();)
        if (it->second.child.expired()) it = items.erase(it); else ++it;
    auto found = items.find(child.get());
    if (found != items.end() && found->second.child.lock() == child &&
        found->second.value.basisPercent == value.basisPercent && found->second.value.order == value.order) return;
    items[child.get()] = {child, value};
    invalidate();
}
StackFlexPresentation Stack::flexPresentation(const std::shared_ptr<Widget>& child) const {
    const auto* state = internal::WidgetPresentationAccess::find(*this);
    if (state) {
        auto found = state->flex.find(child.get());
        if (found != state->flex.end() && found->second.child.lock() == child) return found->second.value;
    }
    return {};
}
} // namespace oneui
