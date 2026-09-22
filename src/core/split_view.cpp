#include "oneui/layout/split_view.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {

namespace {

constexpr float kMinimumDividerHitExtent = 8.0f;
constexpr float kRatioEpsilon = 0.0001f;

} // namespace

// A real leaf in the focus tree: Tab can reach a divider without stealing
// arrow keys from a text field or terminal in either pane.
class SplitView::Divider final : public Widget {
public:
    explicit Divider(SplitView* owner) : owner_(owner) {
        setAccessibleRole(AccessibilityRole::Custom);
        setAccessibleName(L"Split view divider");
        setAccessibleDescription(L"Arrow keys resize; Home/End reach limits; double click resets; Escape cancels drag.");
    }
    void detach() { owner_ = nullptr; }
    bool isFocusable() const override { return interactive() && owner_ && owner_->hasResizableDivider(); }
    bool onKeyDown(const KeyEvent& event) override { return owner_ && owner_->handleDividerKey(event); }
    void paint(Canvas& canvas) override { if (owner_) owner_->paintDivider(canvas); }
private:
    SplitView* owner_;
};

SplitView::SplitView(SplitOrientation orientation) : orientation_(orientation) {
    divider_ = std::make_shared<Divider>(this);
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"Split view divider");
    setAccessibleValue(L"50%");
}

SplitView::~SplitView() { divider_->detach(); }

void SplitView::setFirst(std::shared_ptr<Widget> child) {
    first_ = std::move(child);
    rebuildChildren();
}

void SplitView::setSecond(std::shared_ptr<Widget> child) {
    second_ = std::move(child);
    rebuildChildren();
}

void SplitView::setOrientation(SplitOrientation orientation) {
    if (orientation_ == orientation) {
        return;
    }
    orientation_ = orientation;
    resetInteractionState();
    invalidate();
}

SplitOrientation SplitView::orientation() const {
    return orientation_;
}

void SplitView::setSplitRatio(float ratio) {
    updateSplitRatio(ratio, false);
}

float SplitView::splitRatio() const {
    return splitRatio_;
}

void SplitView::setGap(float gap) {
    if (!std::isfinite(gap)) return;
    gap_ = std::max(0.0f, gap);
    invalidate();
}

void SplitView::setDividerColors(Color normal, Color active) {
    dividerColor_ = normal;
    dividerActiveColor_ = active;
    invalidate();
}

void SplitView::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void SplitView::setResizable(bool resizable) {
    if (resizable_ == resizable) {
        return;
    }
    resizable_ = resizable;
    if (!resizable_) {
        resetInteractionState();
    }
    invalidate();
}

bool SplitView::resizable() const {
    return resizable_;
}

void SplitView::setMinimumPaneExtent(float first, float second) {
    if (!std::isfinite(first) || !std::isfinite(second)) return;
    firstMinimumExtent_ = std::max(0.0f, first);
    secondMinimumExtent_ = std::max(0.0f, second);
    updateSplitRatio(splitRatio_, false);
}

void SplitView::setOnSplitRatioChanged(std::function<void(float)> callback) {
    onSplitRatioChanged_ = std::move(callback);
}

void SplitView::setOnSplitRatioCommitted(std::function<void(float)> callback) {
    onSplitRatioCommitted_ = std::move(callback);
}

bool SplitView::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        resetInteractionState();
        return false;
    }

    if (draggingDivider_) {
        const Rect content = contentRect();
        const float available = availableExtent();
        if (available <= 0.0f) {
            return true;
        }
        const float origin = orientation_ == SplitOrientation::Horizontal ? content.x : content.y;
        const float dividerStart = axisPosition(event.position) - dragOffset_ - gap_ * 0.5f;
        updateSplitRatio((dividerStart - origin) / available, true);
        return true;
    }

    const bool hovered = hasResizableDivider() && dividerHitRect().contains(event.position);
    if (dividerHovered_ != hovered) {
        dividerHovered_ = hovered;
        invalidate();
    }
    if (hovered) {
        return true;
    }
    return View::onMouseMove(event);
}

bool SplitView::onKeyDown(const KeyEvent& event) {
    if (interactive() && event.pressed && event.key == Key::Escape && draggingDivider_) {
        cancelDividerDrag();
        return true;
    }
    return View::onKeyDown(event);
}

bool SplitView::handleDividerKey(const KeyEvent& event) {
    if (!interactive() || !event.pressed || !hasResizableDivider() || event.control || event.alt || event.win) return false;
    if (draggingDivider_) return false;
    float next = splitRatio_;
    if (event.key == Key::Home) next = 0.0f;
    else if (event.key == Key::End) next = 1.0f;
    else if ((orientation_ == SplitOrientation::Horizontal && event.key == Key::Left) ||
             (orientation_ == SplitOrientation::Vertical && event.key == Key::Up)) next -= 0.025f;
    else if ((orientation_ == SplitOrientation::Horizontal && event.key == Key::Right) ||
             (orientation_ == SplitOrientation::Vertical && event.key == Key::Down)) next += 0.025f;
    else return false;
    const auto life = lifetimeToken();
    updateSplitRatio(next, true);
    if (life.expired()) return true;
    if (onSplitRatioCommitted_) { auto callback = onSplitRatioCommitted_; callback(splitRatio_); }
    return true;
}

void SplitView::paintDivider(Canvas& canvas) {
    if (!hasResizableDivider()) return;
    const auto content = contentRect();
    const auto hit = dividerHitRect();
    const bool active = dividerHovered_ || draggingDivider_ || divider_->focusVisible();
    const auto color = active ? dividerActiveColor_ : dividerColor_;
    const float thickness = std::max(1.0f, gap_);
    if (orientation_ == SplitOrientation::Horizontal) {
        const float x = hit.x + hit.width * 0.5f;
        canvas.drawLine({x, content.y}, {x, content.y + content.height}, color, thickness);
    } else {
        const float y = hit.y + hit.height * 0.5f;
        canvas.drawLine({content.x, y}, {content.x + content.width, y}, color, thickness);
    }
}

bool SplitView::onMouseDown(const MouseEvent& event) {
    if (!interactive()) return false;
    if (event.button == MouseButton::Left && hasResizableDivider()
        && dividerHitRect().contains(event.position)) {
        focusChild(divider_.get(), false);
        if (event.clickCount == 2) {
            const auto life = lifetimeToken();
            updateSplitRatio(0.5f, true);
            if (!life.expired() && onSplitRatioCommitted_) { auto callback = onSplitRatioCommitted_; callback(splitRatio_); }
            return true;
        }
        dragBeginRatio_ = splitRatio_;
        const Rect divider = dividerHitRect();
        const float center = orientation_ == SplitOrientation::Horizontal
            ? divider.x + divider.width * 0.5f
            : divider.y + divider.height * 0.5f;
        dragOffset_ = axisPosition(event.position) - center;
        draggingDivider_ = true;
        dividerHovered_ = true;
        invalidate();
        return true;
    }
    return View::onMouseDown(event);
}

bool SplitView::onMouseUp(const MouseEvent& event) {
    if (draggingDivider_) {
        dividerHovered_ = hasResizableDivider() && dividerHitRect().contains(event.position);
        finishDividerDrag();
        invalidate();
        return true;
    }
    return View::onMouseUp(event);
}

CursorKind SplitView::cursor(Point point) const {
    if (draggingDivider_ || (hasResizableDivider() && dividerHitRect().contains(point))) {
        return orientation_ == SplitOrientation::Horizontal
            ? CursorKind::ResizeHorizontal
            : CursorKind::ResizeVertical;
    }
    return View::cursor(point);
}

void SplitView::layoutChildren() {
    const Rect content = contentRect();

    divider_->setAccessibleName(accessibleName());
    divider_->setVisible(hasResizableDivider());
    divider_->setFrame(dividerHitRect());
    divider_->setAccessibleValue(std::to_wstring(static_cast<int>(constrainedRatio(splitRatio_) * 100.0f)) + L"%");
    const bool hasFirst = first_ && first_->visible();
    const bool hasSecond = second_ && second_->visible();
    const float effectiveGap = hasFirst && hasSecond ? gap_ : 0.0f;

    if (!hasFirst && !hasSecond) {
        return;
    }

    if (hasFirst && !hasSecond) {
        first_->setFrame(content);
        return;
    }

    if (!hasFirst && hasSecond) {
        second_->setFrame(content);
        return;
    }

    if (orientation_ == SplitOrientation::Horizontal) {
        const float available = std::max(0.0f, content.width - effectiveGap);
        splitRatio_ = constrainedRatio(splitRatio_);
        const float firstWidth = available * splitRatio_;
        first_->setFrame(Rect{content.x, content.y, firstWidth, content.height});
        second_->setFrame(Rect{content.x + firstWidth + effectiveGap, content.y, available - firstWidth, content.height});
    } else {
        const float available = std::max(0.0f, content.height - effectiveGap);
        splitRatio_ = constrainedRatio(splitRatio_);
        const float firstHeight = available * splitRatio_;
        first_->setFrame(Rect{content.x, content.y, content.width, firstHeight});
        second_->setFrame(Rect{content.x, content.y + firstHeight + effectiveGap, content.width, available - firstHeight});
    }
}

Rect SplitView::contentRect() const {
    Rect content = frame().inset(padding_);
    content.width = std::max(0.0f, content.width);
    content.height = std::max(0.0f, content.height);
    return content;
}

bool SplitView::hasResizableDivider() const {
    return resizable_ && first_ && first_->visible() && second_ && second_->visible();
}

float SplitView::availableExtent() const {
    const Rect content = contentRect();
    const float extent = orientation_ == SplitOrientation::Horizontal ? content.width : content.height;
    return std::max(0.0f, extent - gap_);
}

float SplitView::constrainedRatio(float ratio) const {
    if (!std::isfinite(ratio)) return splitRatio_;
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    const float available = availableExtent();
    if (available <= 0.0f) {
        return ratio;
    }

    const float totalMinimum = firstMinimumExtent_ + secondMinimumExtent_;
    if (totalMinimum > available && totalMinimum > 0.0f) {
        return firstMinimumExtent_ / totalMinimum;
    }

    const float minimumRatio = firstMinimumExtent_ / available;
    const float maximumRatio = 1.0f - secondMinimumExtent_ / available;
    return std::clamp(ratio, minimumRatio, maximumRatio);
}

Rect SplitView::dividerHitRect() const {
    if (!hasResizableDivider()) {
        return Rect{};
    }

    const Rect content = contentRect();
    const float available = availableExtent();
    const float firstExtent = available * constrainedRatio(splitRatio_);
    const float hitExtent = std::max(gap_, kMinimumDividerHitExtent);
    if (orientation_ == SplitOrientation::Horizontal) {
        const float center = content.x + firstExtent + gap_ * 0.5f;
        return Rect{center - hitExtent * 0.5f, content.y, hitExtent, content.height};
    }
    const float center = content.y + firstExtent + gap_ * 0.5f;
    return Rect{content.x, center - hitExtent * 0.5f, content.width, hitExtent};
}

float SplitView::axisPosition(Point point) const {
    return orientation_ == SplitOrientation::Horizontal ? point.x : point.y;
}

void SplitView::finishDividerDrag() {
    if (!draggingDivider_) {
        return;
    }
    draggingDivider_ = false;
    dragOffset_ = 0.0f;
    if (onSplitRatioCommitted_) {
        auto callback = onSplitRatioCommitted_;
        callback(splitRatio_);
    }
}

void SplitView::cancelDividerDrag() {
    if (!draggingDivider_) return;
    draggingDivider_ = false;
    dragOffset_ = 0.0f;
    updateSplitRatio(dragBeginRatio_, true);
}

void SplitView::updateSplitRatio(float ratio, bool notify) {
    const float next = constrainedRatio(ratio);
    if (std::abs(next - splitRatio_) <= kRatioEpsilon) {
        return;
    }
    splitRatio_ = next;
    setAccessibleValue(std::to_wstring(static_cast<int>(splitRatio_ * 100.0f)) + L"%");
    invalidate();
    if (notify && onSplitRatioChanged_) {
        auto callback = onSplitRatioChanged_;
        callback(splitRatio_);
    }
}

bool SplitView::hasInteractionState() const {
    return dividerHovered_ || draggingDivider_ || View::hasInteractionState();
}

void SplitView::resetInteractionState() {
    dividerHovered_ = false;
    finishDividerDrag();
    View::resetInteractionState();
}

void SplitView::rebuildChildren() {
    clearChildren();
    if (first_) {
        add(first_);
    }
    add(divider_);
    if (second_) {
        add(second_);
    }
    invalidate();
}

} // namespace oneui
