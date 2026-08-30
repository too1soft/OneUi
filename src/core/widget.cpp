#include "oneui/widget.h"

#include <utility>

namespace oneui {

void Widget::setFrame(Rect frame) {
    if (frame_.x == frame.x && frame_.y == frame.y && frame_.width == frame.width && frame_.height == frame.height) {
        return;
    }
    frame_ = frame;
}

Rect Widget::frame() const {
    return frame_;
}

void Widget::setPreferredSize(Size size) {
    preferredSize_ = size;
    invalidate();
}

Size Widget::preferredSize() const {
    return preferredSize_;
}

void Widget::setDisabled(bool disabled) {
    if (this->disabled() == disabled) {
        return;
    }
    disabledBinding_.set(disabled, disabled_);
    if (disabled) {
        clearInteractionState();
        setFocused(false);
    }
    invalidate();
}

void Widget::bindDisabled(State<bool>& state) {
    disabledBinding_ = Binding<bool>(state, [this] {
        if (disabled()) {
            clearInteractionState();
            setFocused(false);
        }
        invalidate();
    });
    if (disabled()) {
        clearInteractionState();
        setFocused(false);
    }
    invalidate();
}

bool Widget::disabled() const {
    return disabledBinding_.get(disabled_);
}

void Widget::setVisible(bool visible) {
    if (this->visible() == visible) {
        return;
    }
    visibleBinding_.set(visible, visible_);
    if (!visible) {
        clearInteractionState();
        setFocused(false);
    }
    invalidate();
}

void Widget::bindVisible(State<bool>& state) {
    visibleBinding_ = Binding<bool>(state, [this] {
        if (!visible()) {
            clearInteractionState();
            setFocused(false);
        }
        invalidate();
    });
    if (!visible()) {
        clearInteractionState();
        setFocused(false);
    }
    invalidate();
}

bool Widget::visible() const {
    return visibleBinding_.get(visible_);
}

bool Widget::clearInteractionState() {
    if (!hasInteractionState()) {
        return false;
    }
    resetInteractionState();
    invalidate();
    return true;
}

void Widget::setInvalidator(std::function<void()> invalidator) {
    invalidatorOwner_ = nullptr;
    invalidator_ = std::move(invalidator);
}

void Widget::setRectInvalidator(std::function<void(Rect)> invalidator) {
    rectInvalidatorOwner_ = nullptr;
    rectInvalidator_ = std::move(invalidator);
}

void Widget::setAnimationScheduler(std::function<void()> scheduler) {
    animationSchedulerOwner_ = nullptr;
    animationScheduler_ = std::move(scheduler);
}

void Widget::attachToOwner(
    const void* owner,
    std::function<void()> invalidator,
    std::function<void(Rect)> rectInvalidator,
    std::function<void()> animationScheduler) {
    attachInvalidatorToOwner(owner, std::move(invalidator));
    attachRectInvalidatorToOwner(owner, std::move(rectInvalidator));
    attachAnimationSchedulerToOwner(owner, std::move(animationScheduler));
}

void Widget::attachInvalidatorToOwner(
    const void* owner,
    std::function<void()> invalidator) {
    setInvalidator(std::move(invalidator));
    invalidatorOwner_ = owner;
}

void Widget::attachRectInvalidatorToOwner(
    const void* owner,
    std::function<void(Rect)> invalidator) {
    setRectInvalidator(std::move(invalidator));
    rectInvalidatorOwner_ = owner;
}

void Widget::attachAnimationSchedulerToOwner(
    const void* owner,
    std::function<void()> scheduler) {
    setAnimationScheduler(std::move(scheduler));
    animationSchedulerOwner_ = owner;
}

void Widget::detachFromOwner(const void* owner) {
    if (invalidatorOwner_ == owner) {
        setInvalidator({});
    }
    if (rectInvalidatorOwner_ == owner) {
        setRectInvalidator({});
    }
    if (animationSchedulerOwner_ == owner) {
        setAnimationScheduler({});
    }
}

bool Widget::onMouseMove(const MouseEvent&) {
    return false;
}

bool Widget::onMouseDown(const MouseEvent&) {
    return false;
}

bool Widget::onMouseUp(const MouseEvent&) {
    return false;
}

bool Widget::onMouseWheel(const MouseWheelEvent&) {
    return false;
}

bool Widget::onKeyDown(const KeyEvent&) {
    return false;
}

bool Widget::onKeyUp(const KeyEvent&) {
    return false;
}

bool Widget::onTextInput(wchar_t) {
    return false;
}

bool Widget::onTextInputText(const std::wstring& text) {
    bool handled = false;
    for (const wchar_t character : text) {
        handled = onTextInput(character) || handled;
    }
    return handled;
}

Rect Widget::textInputCaretRect() const {
    return frame();
}

bool Widget::onFocusChanged(bool focused) {
    const bool previousFocused = focused_;
    const bool previousFocusVisible = focusVisible_;
    setFocused(focused);
    if (!focused) {
        setFocusVisible(false);
    }
    return previousFocused != focused_ || previousFocusVisible != focusVisible_;
}

bool Widget::isFocusable() const {
    return false;
}

bool Widget::tabStop() const {
    return tabStop_;
}

void Widget::setTabStop(bool value) {
    tabStop_ = value;
}

bool Widget::focusFirstLeaf() {
    return false; // 叶子控件（Button/TextField）与容器（View）各自覆写
}

bool Widget::focusLastLeaf() {
    return false;
}

bool Widget::hitTest(Point point) const {
    return visible() && frame_.contains(point);
}

CursorKind Widget::cursor(Point point) const {
    if (!interactive() || !hitTest(point)) {
        return CursorKind::Default;
    }

    const auto info = accessibilityInfo();
    if (info.state.disabled) {
        return CursorKind::Default;
    }

    switch (info.role) {
    case AccessibilityRole::Button:
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::Slider:
    case AccessibilityRole::Tab:
    case AccessibilityRole::ListItem:
        return CursorKind::Pointer;
    case AccessibilityRole::TextBox:
        return info.state.readOnly ? CursorKind::Default : CursorKind::Text;
    default:
        return CursorKind::Default;
    }
}

bool Widget::paintsAboveSiblings() const {
    return false;
}

bool Widget::tickAnimations(double) {
    return false;
}

bool Widget::focused() const {
    return focused_;
}

bool Widget::focusVisible() const {
    return focused_ && focusVisible_;
}

void Widget::setFocusVisible(bool visible) {
    if (focusVisible_ == visible) {
        return;
    }
    focusVisible_ = visible;
    invalidate();
}

void Widget::setAccessibleRole(AccessibilityRole role) {
    if (accessibilityRole_ == role) {
        return;
    }
    accessibilityRole_ = role;
    invalidate();
}

AccessibilityRole Widget::accessibleRole() const {
    return accessibilityRole_;
}

void Widget::setAccessibleName(std::wstring name) {
    if (accessibleName_ == name) {
        return;
    }
    accessibleName_ = std::move(name);
    invalidate();
}

const std::wstring& Widget::accessibleName() const {
    return accessibleName_;
}

void Widget::setAccessibleDescription(std::wstring description) {
    if (accessibleDescription_ == description) {
        return;
    }
    accessibleDescription_ = std::move(description);
    invalidate();
}

const std::wstring& Widget::accessibleDescription() const {
    return accessibleDescription_;
}

void Widget::setTooltip(std::wstring tooltip) {
    tooltip_ = std::move(tooltip);
}

const std::wstring& Widget::tooltip() const {
    return tooltip_;
}

const std::wstring* Widget::tooltipAt(Point point) const {
    if (!visible_ || disabled_ || tooltip_.empty() || !hitTest(point)) {
        return nullptr;
    }
    return &tooltip_;
}

void Widget::setAccessibleValue(std::wstring value) {
    if (accessibleValue_ == value) {
        return;
    }
    accessibleValue_ = std::move(value);
    invalidate();
}

const std::wstring& Widget::accessibleValue() const {
    return accessibleValue_;
}

void Widget::setAccessibilityState(AccessibilityState state) {
    accessibilityState_ = state;
    invalidate();
}

AccessibilityState Widget::accessibilityState() const {
    return accessibilityInfo().state;
}

AccessibilityInfo Widget::accessibilityInfo() const {
    auto state = accessibilityState_;
    state.disabled = disabled();
    state.focused = focused();
    state.focusVisible = focusVisible();
    return AccessibilityInfo{accessibilityRole_, accessibleName_, accessibleDescription_, accessibleValue_, state};
}

void Widget::setFocused(bool focused) {
    if (focused_ == focused) {
        return;
    }
    focused_ = focused;
    if (!focused) {
        focusVisible_ = false;
    }
    invalidate();
}

void Widget::invalidate() {
    if (rectInvalidator_) {
        rectInvalidator_(frame_);
    } else if (invalidator_) {
        invalidator_();
    }
}

void Widget::invalidateRect(Rect rect) {
    if (rectInvalidator_) {
        rectInvalidator_(rect);
    } else {
        invalidate();
    }
}

void Widget::requestAnimationFrame() {
    if (animationScheduler_) {
        animationScheduler_();
    }
}

bool Widget::hasAnimationScheduler() const {
    return static_cast<bool>(animationScheduler_);
}

bool Widget::contains(Point point) const {
    return frame_.contains(point);
}

bool Widget::interactive() const {
    return visible() && !disabled();
}

bool Widget::hasInteractionState() const {
    return false;
}

void Widget::resetInteractionState() {
}

} // namespace oneui
