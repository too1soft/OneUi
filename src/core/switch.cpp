#include "oneui/controls/switch.h"

#include "oneui/style.h"

#include <utility>

namespace oneui {

Switch::Switch(std::wstring text) : text_(std::move(text)) {
    setPreferredSize(Size{132.0f, 32.0f});
}

void Switch::setText(std::wstring text) {
    text_ = std::move(text);
    invalidate();
}

void Switch::setChecked(bool checked) {
    assignChecked(checked);
}

bool Switch::checked() const {
    return checkedBinding_.get(checked_);
}

void Switch::bindChecked(State<bool>& state) {
    checkedBinding_ = Binding<bool>(state, [this] {
        invalidate();
    });
    invalidate();
}

void Switch::setOnChanged(std::function<void(bool)> callback) {
    onChanged_ = std::move(callback);
}

void Switch::setStyleOverride(SwitchStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Switch::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

namespace {

void applySwitchOverride(SwitchStyle& style, const std::optional<SwitchStateStyleOverride>& override) {
    if (!override) {
        return;
    }
    if (override->trackBackground) {
        style.trackBackground = *override->trackBackground;
    }
    if (override->thumbBackground) {
        style.thumbBackground = *override->thumbBackground;
    }
    if (override->labelColor) {
        style.labelColor = *override->labelColor;
    }
    if (override->border) {
        style.border = *override->border;
    }
    if (override->borderWidth) {
        style.borderWidth = *override->borderWidth;
    }
    if (override->radius) {
        style.radius = *override->radius;
    }
    if (override->focusRing) {
        if (override->focusRing->color) {
            style.focusRing.color = *override->focusRing->color;
        }
        if (override->focusRing->width) {
            style.focusRing.width = *override->focusRing->width;
        }
        if (override->focusRing->offset) {
            style.focusRing.offset = *override->focusRing->offset;
        }
        if (override->focusRing->radius) {
            style.focusRing.radius = *override->focusRing->radius;
        }
        if (override->focusRing->visible) {
            style.focusRing.visible = *override->focusRing->visible;
        }
    }
}

} // namespace

void Switch::paint(Canvas& canvas) {
    const auto& t = theme();
    const Rect rect = frame();
    const Rect track{rect.x, rect.y + (rect.height - 24.0f) / 2.0f, 46.0f, 24.0f};
    const bool active = checked();

    SwitchStyle style;
    style.trackBackground = disabled() ? Color{226, 232, 240} : (active ? t.primary : Color{203, 213, 225});
    style.thumbBackground = disabled() ? Color{248, 250, 252} : colors::White;
    style.labelColor = disabled() ? t.textSubtle : t.textMuted;
    if (!disabled() && hovered_) {
        style.trackBackground = active ? t.primaryHover : Color{180, 190, 204};
    }
    if (!disabled() && pressed_) {
        style.trackBackground = active ? t.primaryPressed : Color{148, 163, 184};
    }

    if (styleOverride_) {
        applySwitchOverride(style, styleOverride_->normal);
        if (active) {
            applySwitchOverride(style, styleOverride_->selected);
        }
        if (!disabled() && hovered_) {
            applySwitchOverride(style, styleOverride_->hovered);
        }
        if (!disabled() && pressed_) {
            applySwitchOverride(style, styleOverride_->pressed);
        }
        if (disabled()) {
            applySwitchOverride(style, styleOverride_->disabled);
        }
        if (focusVisible() && !disabled()) {
            applySwitchOverride(style, styleOverride_->focusVisible);
        }
    }

    if (focusVisible() && !disabled() && style.focusRing.visible) {
        const float offset = style.focusRing.offset;
        canvas.strokeRect(Rect{track.x - offset, track.y - offset, track.width + offset * 2.0f, track.height + offset * 2.0f},
            style.focusRing.color,
            style.focusRing.radius,
            style.focusRing.width);
    }

    canvas.fillRect(track, style.trackBackground, style.radius);
    if (style.borderWidth > 0.0f && style.border.a != 0) {
        canvas.strokeRect(track, style.border, style.radius, style.borderWidth);
    }
    canvas.fillRect(Rect{track.x + (active ? 24.0f : 4.0f), track.y + 4.0f, 16.0f, 16.0f}, style.thumbBackground, 8.0f);
    canvas.drawTextEllipsized(text_, Rect{rect.x + 58.0f, rect.y, rect.width - 58.0f, rect.height}, style.labelColor, t.fontMd, TextAlign::Left);
}

bool Switch::onMouseMove(const MouseEvent& event) {
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

bool Switch::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position)) {
        return false;
    }
    pressed_ = true;
    invalidate();
    return true;
}

bool Switch::onMouseUp(const MouseEvent& event) {
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

bool Switch::onKeyDown(const KeyEvent& event) {
    if (!interactive() || (event.key != Key::Space && event.key != Key::Enter)) {
        return false;
    }
    toggle();
    return true;
}

CursorKind Switch::cursor(Point point) const {
    return interactive() && contains(point) ? CursorKind::Pointer : CursorKind::Default;
}

bool Switch::isFocusable() const {
    return interactive();
}

void Switch::toggle() {
    assignChecked(!checked());
}

void Switch::assignChecked(bool checked) {
    const bool previous = this->checked();
    checkedBinding_.set(checked, checked_);
    invalidate();
    if (previous != checked && onChanged_) {
        onChanged_(checked);
    }
}

bool Switch::hasInteractionState() const {
    return hovered_ || pressed_;
}

void Switch::resetInteractionState() {
    hovered_ = false;
    pressed_ = false;
}

} // namespace oneui
