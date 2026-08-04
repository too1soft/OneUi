#include "oneui/controls/form_field.h"

#include <algorithm>
#include <utility>

namespace oneui {

namespace {

void applyFormFieldStyleOverride(FormFieldStyle& style, const FormFieldStyleOverride& override) {
    if (override.labelColor) {
        style.labelColor = *override.labelColor;
    }
    if (override.helperColor) {
        style.helperColor = *override.helperColor;
    }
    if (override.errorColor) {
        style.errorColor = *override.errorColor;
    }
    if (override.requiredMarkerColor) {
        style.requiredMarkerColor = *override.requiredMarkerColor;
    }
    if (override.padding) {
        style.padding = *override.padding;
    }
    if (override.labelFontSize) {
        style.labelFontSize = *override.labelFontSize;
    }
    if (override.messageFontSize) {
        style.messageFontSize = *override.messageFontSize;
    }
    if (override.labelLineHeight) {
        style.labelLineHeight = *override.labelLineHeight;
    }
    if (override.messageLineHeight) {
        style.messageLineHeight = *override.messageLineHeight;
    }
    if (override.labelGap) {
        style.labelGap = *override.labelGap;
    }
    if (override.controlGap) {
        style.controlGap = *override.controlGap;
    }
}

float estimatedTextWidth(const std::wstring& text, float fontSize) {
    return static_cast<float>(text.size()) * fontSize * 0.54f;
}

} // namespace

FormField::FormField() {
    updatePreferredSize();
}

FormField::~FormField() {
    if (child_) {
        child_->detachFromOwner(this);
    }
}

void FormField::setChild(std::shared_ptr<Widget> child) {
    if (child_) {
        child_->detachFromOwner(this);
    }
    child_ = std::move(child);
    installChildCallbacks();
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

std::shared_ptr<Widget> FormField::child() const {
    return child_;
}

void FormField::clearChild() {
    if (child_) {
        child_->detachFromOwner(this);
    }
    child_.reset();
    propagatedAccessibleName_.clear();
    propagatedAccessibleDescription_.clear();
    updatePreferredSize();
    invalidate();
}

void FormField::setLabel(std::wstring label) {
    labelBinding_.set(std::move(label), label_);
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

const std::wstring& FormField::label() const {
    return labelBinding_.get(label_);
}

void FormField::bindLabel(State<std::wstring>& state) {
    labelBinding_ = Binding<std::wstring>(state, [this] {
        applyChildAccessibility();
        updatePreferredSize();
        invalidate();
    });
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

void FormField::setHelperText(std::wstring text) {
    helperTextBinding_.set(std::move(text), helperText_);
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

const std::wstring& FormField::helperText() const {
    return helperTextBinding_.get(helperText_);
}

void FormField::bindHelperText(State<std::wstring>& state) {
    helperTextBinding_ = Binding<std::wstring>(state, [this] {
        applyChildAccessibility();
        updatePreferredSize();
        invalidate();
    });
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

void FormField::setErrorText(std::wstring text) {
    errorTextBinding_.set(std::move(text), errorText_);
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

const std::wstring& FormField::errorText() const {
    return errorTextBinding_.get(errorText_);
}

void FormField::bindErrorText(State<std::wstring>& state) {
    errorTextBinding_ = Binding<std::wstring>(state, [this] {
        applyChildAccessibility();
        updatePreferredSize();
        invalidate();
    });
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

void FormField::setRequired(bool required) {
    requiredBinding_.set(required, required_);
    applyChildAccessibility();
    invalidate();
}

bool FormField::required() const {
    return requiredBinding_.get(required_);
}

void FormField::bindRequired(State<bool>& state) {
    requiredBinding_ = Binding<bool>(state, [this] {
        applyChildAccessibility();
        invalidate();
    });
    applyChildAccessibility();
    invalidate();
}

void FormField::setInvalid(bool invalid) {
    invalidBinding_.set(invalid, invalid_);
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

bool FormField::invalid() const {
    return invalidBinding_.get(invalid_);
}

void FormField::bindInvalid(State<bool>& state) {
    invalidBinding_ = Binding<bool>(state, [this] {
        applyChildAccessibility();
        updatePreferredSize();
        invalidate();
    });
    applyChildAccessibility();
    updatePreferredSize();
    invalidate();
}

void FormField::setStyleOverride(FormFieldStyleOverride style) {
    styleOverride_ = std::move(style);
    updatePreferredSize();
    invalidate();
}

void FormField::clearStyleOverride() {
    styleOverride_.reset();
    updatePreferredSize();
    invalidate();
}

void FormField::setInvalidator(std::function<void()> invalidator) {
    Widget::setInvalidator(std::move(invalidator));
    if (child_) {
        child_->attachInvalidatorToOwner(this, [this] {
            updatePreferredSize();
            invalidate();
        });
    }
    applyChildAccessibility();
}

void FormField::setRectInvalidator(std::function<void(Rect)> invalidator) {
    Widget::setRectInvalidator(std::move(invalidator));
    if (child_) {
        child_->attachRectInvalidatorToOwner(
            this,
            [this](Rect rect) { invalidateRect(rect); });
    }
    applyChildAccessibility();
}

void FormField::setAnimationScheduler(std::function<void()> scheduler) {
    Widget::setAnimationScheduler(std::move(scheduler));
    if (child_) {
        child_->attachAnimationSchedulerToOwner(
            this,
            [this] { requestAnimationFrame(); });
    }
    applyChildAccessibility();
}

void FormField::paint(Canvas& canvas) {
    const FormFieldStyle style = resolvedStyle();
    const Rect content = frame().inset(style.padding);
    float cursorY = content.y;

    if (hasLabel()) {
        const Rect labelRect{content.x, cursorY, content.width, style.labelLineHeight};
        canvas.drawTextEllipsized(label(), labelRect, disabled() ? theme().textSubtle : style.labelColor, style.labelFontSize, TextAlign::Left);

        if (required()) {
            const float markerX = content.x + estimatedTextWidth(label(), style.labelFontSize) + 4.0f;
            canvas.drawText(L"*", Rect{markerX, cursorY, 12.0f, style.labelLineHeight}, disabled() ? theme().textSubtle : style.requiredMarkerColor, style.labelFontSize, TextAlign::Left);
        }

        cursorY += style.labelLineHeight + style.labelGap;
    }

    layoutChild();
    if (child_ && child_->visible()) {
        child_->paint(canvas);
        cursorY = child_->frame().y + child_->frame().height;
    }

    if (hasMessage()) {
        const Color messageColor = invalid() ? style.errorColor : style.helperColor;
        const Rect messageRect{content.x, cursorY + style.controlGap, content.width, style.messageLineHeight};
        canvas.drawTextEllipsized(activeMessageText(), messageRect, disabled() ? theme().textSubtle : messageColor, style.messageFontSize, TextAlign::Left);
    }
}

bool FormField::onMouseMove(const MouseEvent& event) {
    if (!interactive() || !child_) {
        resetInteractionState();
        return false;
    }
    layoutChild();
    if (!child_->hitTest(event.position)) {
        return child_->clearInteractionState();
    }
    return child_->onMouseMove(event);
}

bool FormField::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !child_) {
        return false;
    }
    layoutChild();
    return child_->hitTest(event.position) && child_->onMouseDown(event);
}

bool FormField::onMouseUp(const MouseEvent& event) {
    if (!interactive() || !child_) {
        return false;
    }
    layoutChild();
    return child_->onMouseUp(event);
}

bool FormField::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !child_) {
        return false;
    }
    layoutChild();
    return child_->hitTest(event.position) && child_->onMouseWheel(event);
}

bool FormField::onKeyDown(const KeyEvent& event) {
    return interactive() && child_ ? child_->onKeyDown(event) : false;
}

bool FormField::onTextInput(wchar_t character) {
    return interactive() && child_ ? child_->onTextInput(character) : false;
}

bool FormField::onTextInputText(const std::wstring& text) {
    return interactive() && child_ ? child_->onTextInputText(text) : false;
}

bool FormField::onFocusChanged(bool focused) {
    Widget::onFocusChanged(focused);
    if (child_) {
        child_->onFocusChanged(focused);
    }
    return true;
}

bool FormField::isFocusable() const {
    return interactive() && child_ && child_->isFocusable();
}

CursorKind FormField::cursor(Point point) const {
    if (!interactive() || !hitTest(point)) {
        return CursorKind::Default;
    }
    if (!child_) {
        return Widget::cursor(point);
    }

    const_cast<FormField*>(this)->layoutChild();
    return child_->hitTest(point) ? child_->cursor(point) : Widget::cursor(point);
}

void FormField::setFocusVisible(bool visible) {
    Widget::setFocusVisible(visible);
    if (child_) {
        child_->setFocusVisible(visible);
    }
}

bool FormField::tickAnimations(double nowMs) {
    return child_ && child_->visible() ? child_->tickAnimations(nowMs) : false;
}

void FormField::installChildCallbacks() {
    if (!child_) {
        return;
    }
    child_->attachToOwner(
        this,
        [this] {
            updatePreferredSize();
            invalidate();
        },
        [this](Rect rect) { invalidateRect(rect); },
        [this] { requestAnimationFrame(); });
}

FormFieldStyle FormField::resolvedStyle() const {
    FormFieldStyle style = theme().formField;
    if (styleOverride_) {
        applyFormFieldStyleOverride(style, *styleOverride_);
    }
    return style;
}

void FormField::layoutChild() {
    if (!child_) {
        return;
    }

    const FormFieldStyle style = resolvedStyle();
    const Rect content = frame().inset(style.padding);
    float y = content.y;
    if (hasLabel()) {
        y += style.labelLineHeight + style.labelGap;
    }

    const Size childSize = child_->preferredSize();
    child_->setFrame(Rect{content.x, y, content.width, std::max(0.0f, childSize.height)});
}

void FormField::updatePreferredSize() {
    const FormFieldStyle style = resolvedStyle();
    const Size childSize = child_ ? child_->preferredSize() : Size{};
    float height = style.padding.vertical();
    float width = childSize.width + style.padding.horizontal();

    if (hasLabel()) {
        height += style.labelLineHeight + style.labelGap;
        width = std::max(width, estimatedTextWidth(label(), style.labelFontSize) + (required() ? 16.0f : 0.0f) + style.padding.horizontal());
    }

    if (child_) {
        height += std::max(0.0f, childSize.height);
    }

    if (hasMessage()) {
        height += style.controlGap + style.messageLineHeight;
        width = std::max(width, estimatedTextWidth(activeMessageText(), style.messageFontSize) + style.padding.horizontal());
    }

    setPreferredSize(Size{width, height});
}

const std::wstring& FormField::activeMessageText() const {
    return invalid() ? errorText() : helperText();
}

bool FormField::hasLabel() const {
    return !label().empty();
}

bool FormField::hasMessage() const {
    return !activeMessageText().empty();
}

void FormField::applyChildAccessibility() {
    if (!child_) {
        propagatedAccessibleName_.clear();
        propagatedAccessibleDescription_.clear();
        return;
    }

    const std::wstring nextName = label();
    if (child_->accessibleName().empty() || child_->accessibleName() == propagatedAccessibleName_) {
        child_->setAccessibleName(nextName);
    }
    propagatedAccessibleName_ = nextName;

    const std::wstring nextDescription = activeMessageText();
    if (child_->accessibleDescription().empty() || child_->accessibleDescription() == propagatedAccessibleDescription_) {
        child_->setAccessibleDescription(nextDescription);
    }
    propagatedAccessibleDescription_ = nextDescription;

    auto state = child_->accessibilityState();
    state.required = required();
    state.invalid = invalid();
    child_->setAccessibilityState(state);
}

void FormField::resetInteractionState() {
    if (child_) {
        child_->clearInteractionState();
    }
}

} // namespace oneui
