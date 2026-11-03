#include "oneui/layout/scroll_view.h"
#include "internal/scroll_trace.h"
#include "internal/ui_clock.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {
namespace {
double currentTimeMs() { return internal::uiTimeMs(); }
} // namespace

bool ScrollView::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        clearInteractionState();
        return false;
    }

    if (draggingVerticalThumb_) {
        const Rect viewport = viewportRect();
        const Rect thumb = verticalThumbRect();
        const float trackTravel = std::max(0.0f, viewport.height - thumb.height - scrollbarInset_ * 2.0f);
        const float maxOffset = maxScrollOffset();
        if (trackTravel <= 0.001f || maxOffset <= 0.001f) {
            return true;
        }
        const float delta = event.position.y - dragStartY_;
        setScrollOffset(dragStartVerticalOffset_ + delta / trackTravel * maxOffset);
        // The drag owns the pointer until release, even when the offset is clamped.
        return true;
    }

    if (!draggingHorizontalThumb_) {
        return View::onMouseMove(event);
    }

    const Rect viewport = viewportRect();
    const Rect thumb = horizontalThumbRect();
    const float trackTravel = std::max(0.0f, viewport.width - thumb.width - 10.0f);
    const float maxOffset = maxHorizontalScrollOffset();
    if (trackTravel <= 0.001f || maxOffset <= 0.001f) {
        return false;
    }

    const float previous = horizontalScrollOffset_;
    const float delta = event.position.x - dragStartX_;
    setHorizontalScrollOffset(dragStartHorizontalOffset_ + delta / trackTravel * maxOffset);
    return std::fabs(previous - horizontalScrollOffset_) > 0.001f;
}

bool ScrollView::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !hitTest(event.position)) {
        if (internal::scrollTraceEnabled()) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "scroll_view", "pointer_down_missed", reinterpret_cast<std::uintptr_t>(this),
                static_cast<double>(static_cast<int>(event.button)), scrollOffset_, 0.0, 0.0,
                0.0, maxScrollOffset(), 0.0,
                static_cast<double>(event.position.x), static_cast<double>(event.position.y)});
        }
        return false;
    }

    if (event.button == MouseButton::Left && hasVerticalOverflow()
        && verticalScrollbarHitRect().contains(event.position)) {
        const Rect thumb = verticalThumbRect();
        if (event.position.y >= thumb.y && event.position.y <= thumb.y + thumb.height) {
            draggingVerticalThumb_ = true;
            dragStartY_ = event.position.y;
            dragStartVerticalOffset_ = scrollOffset_;
        } else {
            // A click on the track pages by one viewport toward the click.
            const float page = viewportRect().height;
            setScrollOffset(scrollOffset_ + (event.position.y < thumb.y ? -page : page));
        }
        return true;
    }

    if (hasHorizontalOverflow() && horizontalThumbRect().contains(event.position)) {
        draggingHorizontalThumb_ = true;
        dragStartX_ = event.position.x;
        dragStartHorizontalOffset_ = horizontalScrollOffset_;
        return true;
    }

    const bool handled = View::onMouseDown(event);
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "scroll_view", handled ? "pointer_down_forwarded" : "pointer_down_unhandled",
            reinterpret_cast<std::uintptr_t>(this),
            static_cast<double>(static_cast<int>(event.button)), scrollOffset_, 0.0, 0.0,
            0.0, maxScrollOffset(), 0.0,
            static_cast<double>(event.position.x), static_cast<double>(event.position.y)});
    }
    return handled;
}

bool ScrollView::onMouseUp(const MouseEvent& event) {
    if (draggingVerticalThumb_) {
        draggingVerticalThumb_ = false;
        return true;
    }
    if (draggingHorizontalThumb_) {
        (void)event;
        draggingHorizontalThumb_ = false;
        return true;
    }
    const bool handled = View::onMouseUp(event);
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "scroll_view", handled ? "pointer_up_forwarded" : "pointer_up_unhandled",
            reinterpret_cast<std::uintptr_t>(this),
            static_cast<double>(static_cast<int>(event.button)), scrollOffset_, 0.0, 0.0,
            0.0, maxScrollOffset(), 0.0,
            static_cast<double>(event.position.x), static_cast<double>(event.position.y)});
    }
    return handled;
}

bool ScrollView::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !hitTest(event.position)) {
        return false;
    }

    // Route through the hovered subtree first, even when this viewport fits
    // its content. Nested lists/tables own their wheel until they return false
    // at a boundary; only then may this scroll view consume the same event.
    if (View::onMouseWheel(event)) {
        return true;
    }
    if (!hasVerticalOverflow()) {
        return false;
    }

    const double nowMs = event.timestampMs > 0.0 ? event.timestampMs : currentTimeMs();
    const double wheelIntervalMs = scrollTraceLastWheelMs_ > 0.0
        ? nowMs - scrollTraceLastWheelMs_
        : 0.0;
    scrollTraceLastWheelMs_ = nowMs;
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "scroll_view", "wheel_enter", reinterpret_cast<std::uintptr_t>(this),
            event.deltaY, scrollOffset_, scrollMotion_.target(), scrollMotion_.velocity(),
            wheelIntervalMs, maxScrollOffset(), 0.0, wheelStep_,
            scrollMotion_.running() ? 1.0 : 0.0});
    }

    const double catchupStartMs = internal::scrollTraceNowMs();
    const bool caughtUp = advanceScrollMotion(nowMs);
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "scroll_view", "wheel_catchup", reinterpret_cast<std::uintptr_t>(this),
            event.deltaY, scrollOffset_, scrollMotion_.target(), scrollMotion_.velocity(),
            wheelIntervalMs, maxScrollOffset(), internal::scrollTraceNowMs() - catchupStartMs,
            caughtUp ? 1.0 : 0.0, 0.0});
    }

    const float inputDistance = -event.deltaY * wheelStep_;
    const bool accepted = scrollMotion_.addDelta(
        inputDistance,
        0.0f,
        maxScrollOffset(),
        nowMs,
        kDefaultWheelScrollMotionSpec);
    if (accepted || scrollMotion_.running()) {
        requestAnimationFrame();
    }
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "scroll_view", "wheel_applied", reinterpret_cast<std::uintptr_t>(this),
            event.deltaY, scrollOffset_, scrollMotion_.target(), scrollMotion_.velocity(),
            wheelIntervalMs, maxScrollOffset(), kDefaultWheelScrollMotionSpec.retargetSettlingDurationMs,
            inputDistance, accepted ? 1.0 : 0.0});
    }
    return accepted || caughtUp;
}

bool ScrollView::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    // Embedded controls own their navigation keys before the scrolling
    // fallback (sliders, selects, editors and lists all use arrows/Home/End).
    const auto life = lifetimeToken();
    const bool handled = View::onKeyDown(event);
    if (life.expired()) return true;
    if (handled) {
        if (event.key == Key::Tab) scrollFocusedIntoView();
        return true;
    }
    const float previousX = horizontalScrollOffset_;
    const float previousY = scrollOffset_;

    switch (event.key) {
    case Key::Up:
        setScrollOffset(scrollOffset_ - wheelStep_);
        break;
    case Key::Down:
        setScrollOffset(scrollOffset_ + wheelStep_);
        break;
    case Key::Home:
        setScrollOffset(0.0f);
        setHorizontalScrollOffset(0.0f);
        break;
    case Key::End:
        setScrollOffset(maxScrollOffset());
        setHorizontalScrollOffset(maxHorizontalScrollOffset());
        break;
    case Key::Left:
        setHorizontalScrollOffset(horizontalScrollOffset_ - wheelStep_);
        break;
    case Key::Right:
        setHorizontalScrollOffset(horizontalScrollOffset_ + wheelStep_);
        break;
    default:
        return false;
    }

    return std::fabs(previousX - horizontalScrollOffset_) > 0.001f || std::fabs(previousY - scrollOffset_) > 0.001f;
}

bool ScrollView::onFocusChanged(bool focused) {
    const bool handled = View::onFocusChanged(focused);
    if (focused) scrollFocusedIntoView();
    return handled;
}

bool ScrollView::focusFirstLeaf() {
    const bool handled = View::focusFirstLeaf();
    if (handled) scrollFocusedIntoView();
    return handled;
}

bool ScrollView::focusLastLeaf() {
    const bool handled = View::focusLastLeaf();
    if (handled) scrollFocusedIntoView();
    return handled;
}

bool ScrollView::requestFocus(Widget* descendant, bool focusVisible) {
    const bool handled = View::requestFocus(descendant, focusVisible);
    if (handled) scrollFocusedIntoView();
    return handled;
}

void ScrollView::scrollFocusedIntoView() {
    auto leaf = activeFocusChild();
    if (!leaf) return;
    while (auto child = leaf->activeFocusChild()) leaf = std::move(child);
    const Rect target = leaf->frame();
    const Rect viewport = viewportRect();
    if (target.width <= 0.0f || target.height <= 0.0f) return;
    auto reveal = [](float start, float size, float viewportStart, float viewportSize) {
        if (start < viewportStart) return start - viewportStart;
        if (start + size > viewportStart + viewportSize)
            return std::min(start - viewportStart, start + size - viewportStart - viewportSize);
        return 0.0f;
    };
    setHorizontalScrollOffset(horizontalScrollOffset_ +
        reveal(target.x, target.width, viewport.x, viewport.width));
    setScrollOffset(scrollOffset_ +
        reveal(target.y, target.height, viewport.y, viewport.height));
}

} // namespace oneui
