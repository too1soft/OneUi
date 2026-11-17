#include "touch_scroll.h"
#include "oneui/layout/overlay_host.h"
#include <algorithm>
#include <cmath>
namespace oneui::mobile {
namespace {
std::shared_ptr<ScrollView> findScroll(const std::shared_ptr<Widget> &widget, Point point) {
    if (!widget || !widget->visible() || widget->disabled() || !widget->hitTest(point)) return {};
    if (auto host = std::dynamic_pointer_cast<OverlayHost>(widget)) {
        // OverlayHost owns these subtrees outside View::children(). Match its
        // pointer order: highest layer first, latest entry first within a layer.
        auto entries = host->overlays();
        std::stable_sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            return a.layer < b.layer;
        });
        for (auto i = entries.rbegin(); i != entries.rend(); ++i) {
            const auto &child = i->child;
            if (!child || !child->visible() || child->disabled()) continue;
            // A hit surface owns the gesture even when it has no scroll view.
            // A modal backdrop also prevents selecting a scroller underneath.
            if (child->hitTest(point)) return findScroll(child, point);
            if (i->blocksOutsidePointer) return {};
        }
        if (auto scroll = findScroll(host->content(), point)) return scroll;
    }
    if (auto view = std::dynamic_pointer_cast<View>(widget)) {
        for (auto i = view->children().rbegin(); i != view->children().rend(); ++i)
            if (auto scroll = findScroll(*i, point)) return scroll;
    }
    auto scroll = std::dynamic_pointer_cast<ScrollView>(widget);
    return scroll && scroll->maxScrollOffset() > 0 ? scroll : nullptr;
}
} // namespace
void TouchScroll::reset() { target_.reset(); dragging_ = horizontal_ = false; }
bool TouchScroll::dispatch(const std::shared_ptr<Widget> &root, const OneUiMobilePointerV1 &event) {
    if (event.phase == ONEUI_MOBILE_CANCEL) {
        if (event.id == -1 || event.primary) reset();
        return false;
    }
    if (!event.primary || event.kind != ONEUI_MOBILE_TOUCH) return false;
    if (event.phase == ONEUI_MOBILE_DOWN) {
        reset();
        start_ = {event.x, event.y};
        auto target = findScroll(root, start_);
        target_ = target;
        if (target) offset_ = target->scrollOffset();
        return false;
    }
    auto target = target_.lock();
    if (!target) return false;
    if (event.phase == ONEUI_MOBILE_MOVE && !horizontal_) {
        const float dx = event.x - start_.x, dy = event.y - start_.y;
        if (!dragging_ && std::hypot(dx, dy) >= 8) {
            horizontal_ = std::fabs(dx) >= std::fabs(dy);
            if (!horizontal_) {
                dragging_ = true;
                root->clearInteractionState(); // Cancel a press without generating a click.
            }
        }
        if (dragging_) target->setScrollOffset(offset_ - dy);
        return dragging_;
    }
    if (event.phase == ONEUI_MOBILE_UP) {
        const bool consumed = dragging_;
        reset();
        return consumed;
    }
    return dragging_;
}
} // namespace oneui::mobile
