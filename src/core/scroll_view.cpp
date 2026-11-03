#include "oneui/layout/scroll_view.h"

#include "oneui/style.h"
#include "oneui/layout/stack.h"
#include "internal/scroll_trace.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {
ScrollView::ScrollView() {
    setPreferredSize(Size{0.0f, 160.0f});
}

void ScrollView::setContent(std::shared_ptr<Widget> content) {
    clearChildren();
    content_ = std::move(content);
    if (content_) {
        add(content_);
    }
    horizontalScrollOffset_ = clampHorizontalOffset(horizontalScrollOffset_);
    scrollOffset_ = clampOffset(scrollOffset_);
    resetScrollMotion(scrollOffset_);
    invalidate();
}

void ScrollView::setContentWidth(float width) {
    contentWidth_ = std::max(0.0f, width);
    horizontalScrollOffset_ = clampHorizontalOffset(horizontalScrollOffset_);
    invalidate();
}

void ScrollView::setContentHeight(float height) {
    const float next = std::max(0.0f, height);
    // Declarative bindings re-apply an unchanged height on unrelated refreshes;
    // that must not cancel a wheel scroll that is still animating.
    if (std::fabs(next - contentHeight_) <= 0.001f) {
        return;
    }
    contentHeight_ = next;
    const float clamped = clampOffset(scrollOffset_);
    if (std::fabs(clamped - scrollOffset_) > 0.001f || !scrollMotion_.running()) {
        scrollOffset_ = clamped;
        resetScrollMotion(scrollOffset_);
    }
    invalidate();
}

void ScrollView::setWheelStep(float step) {
    wheelStep_ = std::max(1.0f, step);
}

void ScrollView::setChromeVisible(bool visible) {
    if (chromeVisible_ == visible) {
        return;
    }
    chromeVisible_ = visible;
    invalidate();
}

void ScrollView::setScrollbarStyle(Color color, float thickness) {
    scrollbarColor_ = color;
    scrollbarThickness_ = std::max(1.0f, thickness);
    invalidate();
}
void ScrollView::setScrollbarInset(float inset) {
    scrollbarInset_ = std::max(0.0f, inset);
    invalidate();
}

void ScrollView::setStyleBox(StyleBox style) {
    styleBox_ = std::move(style);
    invalidate();
}

void ScrollView::clearStyleBox() {
    if (!styleBox_) {
        return;
    }
    styleBox_.reset();
    invalidate();
}

void ScrollView::setHorizontalScrollOffset(float offset) {
    const float next = clampHorizontalOffset(offset);
    if (std::fabs(next - horizontalScrollOffset_) <= 0.001f) {
        return;
    }
    horizontalScrollOffset_ = next;
    layoutChildren();
    invalidate();
}

void ScrollView::setScrollOffset(float offset) {
    const float next = clampOffset(offset);
    resetScrollMotion(next);
    if (std::fabs(next - scrollOffset_) <= 0.001f) {
        return;
    }
    scrollOffset_ = next;
    layoutChildren();
    invalidate();
}

float ScrollView::horizontalScrollOffset() const {
    return horizontalScrollOffset_;
}

float ScrollView::scrollOffset() const {
    return scrollOffset_;
}

float ScrollView::maxHorizontalScrollOffset() const {
    return std::max(0.0f, resolvedContentWidth() - viewportRect().width);
}

float ScrollView::maxScrollOffset() const {
    return std::max(0.0f, resolvedContentHeight() - viewportRect().height);
}

void ScrollView::paint(Canvas& canvas) {
    layoutChildren();

    const Rect viewport = viewportRect();
    if (styleBox_) {
        paintStyleBox(canvas, viewport, *styleBox_);
    } else if (chromeVisible_) {
        const auto& t = theme();
        canvas.fillRect(viewport, t.surface, t.radiusMd);
        canvas.strokeRect(viewport, t.border, t.radiusMd, 1.0f);
    }

    canvas.save();
    canvas.clipRect(viewport);
    if (content_ && content_->visible()) {
        content_->paint(canvas);
    }
    canvas.restore();

    if (hasVerticalOverflow()) {
        const Rect thumb = verticalThumbRect();
        canvas.fillRect(thumb, scrollbarColor_, thumb.width / 2.0f);
    }

    if (hasHorizontalOverflow()) {
        const Rect thumb = horizontalThumbRect();
        canvas.fillRect(thumb, scrollbarColor_, thumb.height / 2.0f);
    }
}


bool ScrollView::tickAnimations(double nowMs) {
    const bool childrenRunning = View::tickAnimations(nowMs);
    const double frameIntervalMs = scrollTraceLastTickMs_ > 0.0
        ? nowMs - scrollTraceLastTickMs_
        : 0.0;
    scrollTraceLastTickMs_ = nowMs;
    const float previousOffset = scrollOffset_;
    const double tickStartMs = internal::scrollTraceNowMs();
    const bool ticked = advanceScrollMotion(nowMs);
    if (internal::scrollTraceEnabled() && ticked) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "scroll_view", "animation_tick", reinterpret_cast<std::uintptr_t>(this),
            0.0, scrollOffset_, scrollMotion_.target(), scrollMotion_.velocity(),
            frameIntervalMs,
            maxScrollOffset(), internal::scrollTraceNowMs() - tickStartMs,
            scrollOffset_ - previousOffset,
            content_ ? content_->frame().y : 0.0});
    }
    if (scrollMotion_.running()) {
        requestAnimationFrame();
    }
    return childrenRunning || ticked;
}


void ScrollView::layoutChildren() {
    if (!content_) {
        return;
    }

    // 视口或内容尺寸变化后，既有偏移可能越界（例如初次布局前预设了“滚动到底”，
    // 或窗口变大后底部露白），布局时统一回夹到合法区间。
    const float clampedOffset = clampOffset(scrollOffset_);
    if (std::fabs(clampedOffset - scrollOffset_) > 0.001f) {
        scrollOffset_ = clampedOffset;
        resetScrollMotion(clampedOffset);
    }
    horizontalScrollOffset_ = clampHorizontalOffset(horizontalScrollOffset_);

    const Rect viewport = viewportRect();
    const float scrollbarReserve = hasVerticalOverflow() ? scrollbarThickness_ + scrollbarInset_ * 2.0f : 0.0f;
    const float fallbackWidth = std::max(0.0f, viewport.width - scrollbarReserve);
    const bool hasContentWidth = contentWidth_ > 0.0f || (content_ && content_->preferredSize().width > 0.0f);
    const float contentWidth = hasContentWidth ? std::max(fallbackWidth, resolvedContentWidth()) : fallbackWidth;
    content_->setFrame(Rect{viewport.x - horizontalScrollOffset_, viewport.y - scrollOffset_, contentWidth, resolvedContentHeight()});
}

void ScrollView::resetInteractionState() {
    View::resetInteractionState();
    draggingHorizontalThumb_ = false;
    draggingVerticalThumb_ = false;
}

Rect ScrollView::verticalScrollbarHitRect() const {
    if (!hasVerticalOverflow()) {
        return Rect{};
    }
    // The content already leaves this gutter free (see layoutChildren), so
    // the wider target never steals presses from the content.
    const Rect viewport = viewportRect();
    const float width = std::max(10.0f, scrollbarThickness_ + scrollbarInset_ * 2.0f);
    return Rect{viewport.x + viewport.width - width, viewport.y, width, viewport.height};
}

Rect ScrollView::horizontalThumbRect() const {
    if (!hasHorizontalOverflow()) {
        return Rect{};
    }

    const float thumbHeight = scrollbarThickness_;
    const float thumbInset = scrollbarInset_;
    constexpr float minThumbWidth = 24.0f;
    const Rect viewport = viewportRect();
    const float contentWidth = resolvedContentWidth();
    const float thumbWidth = std::max(minThumbWidth, viewport.width * viewport.width / contentWidth);
    const float travel = std::max(0.0f, viewport.width - thumbWidth - thumbInset * 2.0f);
    const float x = viewport.x + thumbInset + (maxHorizontalScrollOffset() <= 0.0f ? 0.0f : horizontalScrollOffset_ / maxHorizontalScrollOffset() * travel);
    return Rect{x, viewport.y + viewport.height - thumbHeight - thumbInset, thumbWidth, thumbHeight};
}

Rect ScrollView::verticalThumbRect() const {
    if (!hasVerticalOverflow()) {
        return Rect{};
    }

    const float thumbWidth = scrollbarThickness_;
    const float thumbInset = scrollbarInset_;
    constexpr float minThumbHeight = 24.0f;
    const Rect viewport = viewportRect();
    const float contentHeight = resolvedContentHeight();
    const float thumbHeight = std::max(minThumbHeight, viewport.height * viewport.height / contentHeight);
    const float travel = std::max(0.0f, viewport.height - thumbHeight - thumbInset * 2.0f);
    const float y = viewport.y + thumbInset + (maxScrollOffset() <= 0.0f ? 0.0f : scrollOffset_ / maxScrollOffset() * travel);
    return Rect{viewport.x + viewport.width - thumbWidth - thumbInset, y, thumbWidth, thumbHeight};
}

float ScrollView::resolvedContentWidth() const {
    if (contentWidth_ > 0.0f) {
        return contentWidth_;
    }
    if (content_ && content_->preferredSize().width > 0.0f) {
        return content_->preferredSize().width;
    }
    return viewportRect().width;
}

float ScrollView::resolvedContentHeight() const {
    if (contentHeight_ > 0.0f) {
        return contentHeight_;
    }
    if (content_ && content_->preferredSize().height > 0.0f) {
        return content_->preferredSize().height;
    }
    // Width-aware content is opt-in; explicit extents and legacy sizing win.
    if (auto* stack = dynamic_cast<Stack*>(content_.get()); stack && stack->engine() == StackEngine::Yoga) {
        const auto viewport = viewportRect();
        const bool explicitWidth = contentWidth_ > 0 || content_->preferredSize().width > 0;
        const float width = explicitWidth ? resolvedContentWidth() : viewport.width;
        auto measured = stack->measure({std::max(0.0f, width), INFINITY});
        if (!explicitWidth && measured.height > viewport.height + 0.001f) {
            const float gutter = scrollbarThickness_ + scrollbarInset_ * 2.0f;
            measured = stack->measure({std::max(0.0f, width - gutter), INFINITY});
        }
        return std::max(viewport.height, measured.height);
    }
    return viewportRect().height;
}

float ScrollView::clampHorizontalOffset(float offset) const {
    return std::max(0.0f, std::min(offset, maxHorizontalScrollOffset()));
}

float ScrollView::clampOffset(float offset) const {
    return std::max(0.0f, std::min(offset, maxScrollOffset()));
}

bool ScrollView::hasHorizontalOverflow() const {
    return resolvedContentWidth() > viewportRect().width + 0.001f;
}

bool ScrollView::hasVerticalOverflow() const {
    return resolvedContentHeight() > viewportRect().height + 0.001f;
}

Rect ScrollView::viewportRect() const {
    return frame();
}

void ScrollView::resetScrollMotion(float offset) {
    scrollMotion_.reset(offset);
}

bool ScrollView::advanceScrollMotion(double nowMs) {
    if (!scrollMotion_.running()) {
        return false;
    }

    const float previous = scrollOffset_;
    scrollMotion_.tick(nowMs);
    scrollOffset_ = clampOffset(scrollMotion_.value());
    if (std::fabs(scrollOffset_ - previous) > 0.001f) {
        layoutChildren();
        invalidate();
    }
    return true;
}

} // namespace oneui
