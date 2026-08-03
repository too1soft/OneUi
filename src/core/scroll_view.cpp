#include "oneui/layout/scroll_view.h"

#include "oneui/style.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace oneui {
namespace {

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

// A critically damped spring keeps velocity continuous while wheel and
// precision-touchpad events extend or reverse the target. The previous
// duration-based easing restarted on every event, which produced a visible
// move/pause rhythm even when frames were presented at the monitor rate.
constexpr double kWheelSpringAngularFrequency = 60.0;
constexpr double kWheelInputPreviewSeconds = 0.008;
constexpr float kWheelSettleDistance = 0.05f;
constexpr float kWheelSettleVelocity = 1.0f;

} // namespace

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
    resetScrollAnimation(scrollOffset_);
    invalidate();
}

void ScrollView::setContentWidth(float width) {
    contentWidth_ = std::max(0.0f, width);
    horizontalScrollOffset_ = clampHorizontalOffset(horizontalScrollOffset_);
    invalidate();
}

void ScrollView::setContentHeight(float height) {
    contentHeight_ = std::max(0.0f, height);
    scrollOffset_ = clampOffset(scrollOffset_);
    resetScrollAnimation(scrollOffset_);
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
    resetScrollAnimation(next);
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
    if (chromeVisible_) {
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

bool ScrollView::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        clearInteractionState();
        return false;
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
        return false;
    }

    if (hasHorizontalOverflow() && horizontalThumbRect().contains(event.position)) {
        draggingHorizontalThumb_ = true;
        dragStartX_ = event.position.x;
        dragStartHorizontalOffset_ = horizontalScrollOffset_;
        return true;
    }

    return View::onMouseDown(event);
}

bool ScrollView::onMouseUp(const MouseEvent& event) {
    if (draggingHorizontalThumb_) {
        (void)event;
        draggingHorizontalThumb_ = false;
        return true;
    }
    return View::onMouseUp(event);
}

bool ScrollView::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !hitTest(event.position) || !hasVerticalOverflow()) {
        return false;
    }

    const double nowMs = currentTimeMs();
    advanceScrollAnimation(nowMs);

    const float previousTarget = scrollAnimationRunning_ ? scrollTargetOffset_ : scrollOffset_;
    const float target = clampOffset(previousTarget - event.deltaY * wheelStep_);
    if (std::fabs(target - previousTarget) <= 0.001f) {
        return false;
    }

    const float targetDelta = target - previousTarget;
    if (scrollVelocity_ * targetDelta < 0.0f) {
        // Direction changes should feel immediate instead of first coasting in
        // the direction of the previous gesture.
        scrollVelocity_ *= 0.25f;
    }
    scrollTargetOffset_ = target;
    scrollAnimationRunning_ = true;
    scrollAnimationLastMs_ = nowMs;
    advanceScrollAnimationBy(kWheelInputPreviewSeconds);
    layoutChildren();
    invalidate();
    requestAnimationFrame();
    return true;
}

bool ScrollView::tickAnimations(double nowMs) {
    const bool childrenRunning = View::tickAnimations(nowMs);
    const bool ticked = advanceScrollAnimation(nowMs);
    if (scrollAnimationRunning_) {
        requestAnimationFrame();
    }
    return childrenRunning || ticked;
}

bool ScrollView::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
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
        return View::onKeyDown(event);
    }

    return std::fabs(previousX - horizontalScrollOffset_) > 0.001f || std::fabs(previousY - scrollOffset_) > 0.001f;
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
        resetScrollAnimation(clampedOffset);
    }
    horizontalScrollOffset_ = clampHorizontalOffset(horizontalScrollOffset_);

    const Rect viewport = viewportRect();
    const float scrollbarReserve = hasVerticalOverflow() ? scrollbarThickness_ + 10.0f : 0.0f;
    const float fallbackWidth = std::max(0.0f, viewport.width - scrollbarReserve);
    const bool hasContentWidth = contentWidth_ > 0.0f || (content_ && content_->preferredSize().width > 0.0f);
    const float contentWidth = hasContentWidth ? std::max(fallbackWidth, resolvedContentWidth()) : fallbackWidth;
    content_->setFrame(Rect{viewport.x - horizontalScrollOffset_, viewport.y - scrollOffset_, contentWidth, resolvedContentHeight()});
}

void ScrollView::resetInteractionState() {
    View::resetInteractionState();
    draggingHorizontalThumb_ = false;
}

Rect ScrollView::horizontalThumbRect() const {
    if (!hasHorizontalOverflow()) {
        return Rect{};
    }

    const float thumbHeight = scrollbarThickness_;
    constexpr float thumbInset = 5.0f;
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
    constexpr float thumbInset = 5.0f;
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

void ScrollView::resetScrollAnimation(float offset) {
    scrollTargetOffset_ = offset;
    scrollVelocity_ = 0.0f;
    scrollAnimationLastMs_ = 0.0;
    scrollAnimationRunning_ = false;
}

bool ScrollView::advanceScrollAnimation(double nowMs) {
    if (!scrollAnimationRunning_) {
        return false;
    }

    if (scrollAnimationLastMs_ <= 0.0) {
        scrollAnimationLastMs_ = nowMs;
        return true;
    }

    const double elapsedSeconds = std::max(0.0, (nowMs - scrollAnimationLastMs_) / 1000.0);
    scrollAnimationLastMs_ = nowMs;
    if (elapsedSeconds <= 0.0) {
        return true;
    }

    const float previous = scrollOffset_;
    advanceScrollAnimationBy(elapsedSeconds);
    if (std::fabs(scrollOffset_ - previous) > 0.001f) {
        layoutChildren();
        invalidate();
    }
    return true;
}

void ScrollView::advanceScrollAnimationBy(double elapsedSeconds) {
    const double displacement = static_cast<double>(scrollOffset_ - scrollTargetOffset_);
    const double velocity = static_cast<double>(scrollVelocity_);
    const double coefficient = velocity + kWheelSpringAngularFrequency * displacement;
    const double decay = std::exp(-kWheelSpringAngularFrequency * elapsedSeconds);
    const double nextDisplacement = (displacement + coefficient * elapsedSeconds) * decay;
    const double nextVelocity = (velocity - kWheelSpringAngularFrequency * coefficient * elapsedSeconds) * decay;

    scrollOffset_ = clampOffset(scrollTargetOffset_ + static_cast<float>(nextDisplacement));
    scrollVelocity_ = static_cast<float>(nextVelocity);

    if (std::fabs(scrollTargetOffset_ - scrollOffset_) <= kWheelSettleDistance
        && std::fabs(scrollVelocity_) <= kWheelSettleVelocity) {
        scrollOffset_ = scrollTargetOffset_;
        scrollVelocity_ = 0.0f;
        scrollAnimationRunning_ = false;
    }
}

} // namespace oneui
