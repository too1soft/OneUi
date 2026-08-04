#include "oneui/controls/checkbox.h"

#include "oneui/style.h"

#include <utility>

namespace oneui {
namespace {

void applyFocusRingOverride(FocusRingStyle& style, const FocusRingStyleOverride& override) {
    if (override.color) {
        style.color = *override.color;
    }
    if (override.width) {
        style.width = *override.width;
    }
    if (override.offset) {
        style.offset = *override.offset;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.visible) {
        style.visible = *override.visible;
    }
}

void applyCheckboxStateOverride(CheckboxStyle& style, const CheckboxStateStyleOverride& override) {
    if (override.boxBackground) {
        style.boxBackground = *override.boxBackground;
    }
    if (override.boxBorder) {
        style.boxBorder = *override.boxBorder;
    }
    if (override.checkColor) {
        style.checkColor = *override.checkColor;
    }
    if (override.labelColor) {
        style.labelColor = *override.labelColor;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
}

CheckboxStyle baseCheckboxStyle(bool active, bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    CheckboxStyle style;
    style.boxBorder = disabled ? t.border : (active ? t.primary : t.border);
    style.boxBackground = disabled ? t.surfaceMuted : (active ? t.primary : t.surface);
    style.checkColor = disabled ? t.textSubtle : colors::White;
    style.labelColor = disabled ? t.textSubtle : t.textMuted;
    style.borderWidth = 1.0f;
    style.radius = t.radiusSm;
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, t.focusOutlineOffset + 1.0f, 6.0f, true};

    if (!disabled && !active && hovered) {
        style.boxBackground = t.surfaceMuted;
    }
    if (!disabled && pressed) {
        style.boxBackground = active ? t.primaryPressed : Color{232, 236, 242};
        style.boxBorder = active ? t.primaryPressed : t.borderStrong;
    }

    return style;
}

CheckboxStyle resolveCheckboxStyle(
    bool active,
    bool disabled,
    bool hovered,
    bool pressed,
    bool focusVisible,
    const std::optional<CheckboxStyleOverride>& override) {
    CheckboxStyle style = baseCheckboxStyle(active, disabled, hovered, pressed);
    if (!override) {
        return style;
    }

    if (override->normal) {
        applyCheckboxStateOverride(style, *override->normal);
    }
    if (disabled && override->disabled) {
        applyCheckboxStateOverride(style, *override->disabled);
    } else if (pressed && override->pressed) {
        applyCheckboxStateOverride(style, *override->pressed);
    } else if (active && override->selected) {
        applyCheckboxStateOverride(style, *override->selected);
    } else if (hovered && override->hovered) {
        applyCheckboxStateOverride(style, *override->hovered);
    }
    if (focusVisible && override->focusVisible) {
        applyCheckboxStateOverride(style, *override->focusVisible);
    }

    return style;
}

} // namespace

Checkbox::Checkbox(std::wstring text) : text_(std::move(text)) {
    setPreferredSize(Size{220.0f, 28.0f});
}

void Checkbox::setText(std::wstring text) {
    text_ = std::move(text);
    invalidate();
}

void Checkbox::setChecked(bool checked) {
    assignChecked(checked);
}

bool Checkbox::checked() const {
    return checkedBinding_.get(checked_);
}

void Checkbox::bindChecked(State<bool>& state) {
    checkedBinding_ = Binding<bool>(state, [this] {
        invalidate();
    });
    invalidate();
}

void Checkbox::setStyleOverride(CheckboxStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Checkbox::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void Checkbox::setOnChanged(std::function<void(bool)> callback) {
    onChanged_ = std::move(callback);
}

void Checkbox::paint(Canvas& canvas) {
    const Rect rect = frame();
    const Rect box{rect.x, rect.y + (rect.height - 16.0f) / 2.0f, 16.0f, 16.0f};
    const bool active = checked();
    const CheckboxStyle style = resolveCheckboxStyle(active, disabled(), hovered_, pressed_, focusVisible(), styleOverride_);

    if (focusVisible() && !disabled() && style.focusRing.visible) {
        const float offset = style.focusRing.offset;
        canvas.strokeRect(
            Rect{box.x - offset, box.y - offset, box.width + offset * 2.0f, box.height + offset * 2.0f},
            style.focusRing.color,
            style.focusRing.radius,
            style.focusRing.width);
    }

    canvas.fillRect(box, style.boxBackground, style.radius);
    canvas.strokeRect(box, style.boxBorder, style.radius, style.borderWidth);
    if (active) {
        canvas.drawLine(Point{box.x + 4.0f, box.y + 8.0f}, Point{box.x + 7.0f, box.y + 11.0f}, style.checkColor, 2.0f);
        canvas.drawLine(Point{box.x + 7.0f, box.y + 11.0f}, Point{box.x + 12.0f, box.y + 5.0f}, style.checkColor, 2.0f);
    }
    canvas.drawTextEllipsized(text_, Rect{rect.x + 26.0f, rect.y, rect.width - 26.0f, rect.height}, style.labelColor, theme().fontMd, TextAlign::Left);
}

bool Checkbox::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const bool next = contains(event.position);
    if (next == hovered_) {
        return false;
    }
    hovered_ = next;
    invalidate();
    return true;
}

bool Checkbox::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position)) {
        return false;
    }
    pressed_ = true;
    invalidate();
    return true;
}

bool Checkbox::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const bool wasPressed = pressed_;
    pressed_ = false;
    invalidate();
    if (wasPressed && contains(event.position)) {
        toggle();
    }
    return wasPressed;
}

bool Checkbox::onKeyDown(const KeyEvent& event) {
    if (!interactive() || (event.key != Key::Space && event.key != Key::Enter)) {
        return false;
    }
    toggle();
    return true;
}

bool Checkbox::isFocusable() const {
    return interactive();
}

AccessibilityInfo Checkbox::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::CheckBox;
    }
    if (info.name.empty()) {
        info.name = text_;
    }
    info.value = checked() ? L"checked" : L"unchecked";
    info.state.checked = checked();
    return info;
}

void Checkbox::toggle() {
    assignChecked(!checked());
}

void Checkbox::assignChecked(bool checked) {
    const bool previous = this->checked();
    checkedBinding_.set(checked, checked_);
    invalidate();
    if (previous != checked && onChanged_) {
        onChanged_(checked);
    }
}

bool Checkbox::hasInteractionState() const {
    return hovered_ || pressed_;
}

void Checkbox::resetInteractionState() {
    hovered_ = false;
    pressed_ = false;
}

} // namespace oneui
