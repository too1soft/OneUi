#include "oneui/controls/button.h"

#include "oneui/color.h"
#include "oneui/style.h"

#include <algorithm>
#include <chrono>
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

void applyButtonStateOverride(ButtonStyle& style, const ButtonStateStyleOverride& override) {
    if (override.background) {
        style.background = *override.background;
    }
    if (override.foreground) {
        style.foreground = *override.foreground;
    }
    if (override.border) {
        style.border = *override.border;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.fontSize) {
        style.fontSize = *override.fontSize;
    }
    if (override.fontWeight) {
        style.fontWeight = *override.fontWeight;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
    if (override.transition) {
        style.transition = *override.transition;
    }
    if (override.shadows) {
        style.shadows = *override.shadows;
    }
}

ButtonStyle baseButtonStyle(ButtonVariant variant, bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    ButtonStyle style;
    style.background = colors::Panel;
    style.border = colors::Border;
    style.foreground = colors::Text;
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, t.focusOutlineOffset, t.radiusLg, true};

    if (disabled) {
        style.background = Color{238, 241, 245};
        style.border = Color{226, 232, 240};
        style.foreground = Color{148, 163, 184};
    } else if (variant == ButtonVariant::Primary) {
        style.background = pressed ? colors::PrimaryPressed : (hovered ? colors::PrimaryHover : colors::Primary);
        style.border = style.background;
        style.foreground = colors::White;
    } else if (pressed) {
        style.background = Color{232, 234, 238};
    } else if (hovered) {
        style.background = Color{242, 244, 247};
    }

    return style;
}

ButtonStyle resolveButtonStyle(
    ButtonVariant variant,
    bool disabled,
    bool hovered,
    bool pressed,
    bool focusVisible,
    const std::optional<ButtonStyleOverride>& override) {
    ButtonStyle style = baseButtonStyle(variant, disabled, hovered, pressed);
    if (!override) {
        return style;
    }

    if (override->normal) {
        applyButtonStateOverride(style, *override->normal);
    }
    if (disabled && override->disabled) {
        applyButtonStateOverride(style, *override->disabled);
    } else if (pressed && override->pressed) {
        applyButtonStateOverride(style, *override->pressed);
    } else if (hovered && override->hovered) {
        applyButtonStateOverride(style, *override->hovered);
    }
    if (focusVisible && override->focusVisible) {
        applyButtonStateOverride(style, *override->focusVisible);
    }
    return style;
}

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

} // namespace

Button::Button(std::wstring text) : text_(std::move(text)) {
    setPreferredSize(Size{96.0f, 36.0f});
}

void Button::setText(std::wstring text) {
    textBinding_.set(std::move(text), text_);
    invalidate();
}

const std::wstring& Button::text() const {
    return textBinding_.get(text_);
}

void Button::bindText(State<std::wstring>& state) {
    textBinding_ = Binding<std::wstring>(state, [this] {
        invalidate();
    });
    invalidate();
}

void Button::setVariant(ButtonVariant variant) {
    const ButtonStyle previous = resolvedStyle();
    variant_ = variant;
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
}

void Button::setIcon(IconSymbol symbol) {
    icon_ = symbol;
    invalidate();
}

void Button::clearIcon() {
    icon_.reset();
    invalidate();
}

void Button::setStyleOverride(ButtonStyleOverride style) {
    const ButtonStyle previous = resolvedStyle();
    styleOverride_ = std::move(style);
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
}

void Button::clearStyleOverride() {
    const ButtonStyle previous = resolvedStyle();
    styleOverride_.reset();
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
}

void Button::setDisabled(bool disabled) {
    const ButtonStyle previous = resolvedStyle();
    Widget::setDisabled(disabled);
    beginVisualTransition(previous, resolvedStyle());
}

void Button::setOnClick(std::function<void()> callback) {
    onClick_ = std::move(callback);
}

void Button::paint(Canvas& canvas) {
    const auto rect = frame();
    const ButtonStyle target = resolvedStyle();
    const ButtonStyle style = visualStyle(target);

    if (focusVisible() && style.focusRing.visible) {
        const float offset = style.focusRing.offset;
        canvas.strokeRect(
            Rect{rect.x - offset, rect.y - offset, rect.width + offset * 2.0f, rect.height + offset * 2.0f},
            style.focusRing.color,
            style.focusRing.radius,
            style.focusRing.width);
    }

    for (const auto& shadow : style.shadows) {
        if (!shadow.inset) {
            canvas.drawBoxShadow(
                Rect{rect.x + shadow.offset.x, rect.y + shadow.offset.y, rect.width, rect.height},
                BoxShadow{shadow.color, Point{0.0f, 0.0f}, shadow.blurRadius, shadow.spreadRadius},
                style.radius);
        }
    }
    canvas.fillRect(rect, style.background, style.radius);
    // border-width: 0 语义为“无边框”：宽度 0 传给 Skia 会画 1px 发丝线，必须显式跳过。
    if (style.borderWidth > 0.0f && style.border.a > 0) {
        canvas.strokeRect(rect, style.border, style.radius, style.borderWidth);
    }
    for (const auto& shadow : style.shadows) {
        if (shadow.inset) {
            canvas.strokeRect(rect, shadow.color, style.radius, std::max(1.0f, shadow.blurRadius));
        }
    }
    const std::wstring& label = this->text();
    if (!icon_) {
        canvas.drawTextStyled(label, rect, style.foreground, style.fontSize, TextAlign::Center, style.fontWeight);
        return;
    }

    // 图标 + 文字作为整体水平居中：图标为字号等大的正方形，随前景色着色。
    const float iconSide = style.fontSize + 2.0f;
    const float gap = label.empty() ? 0.0f : 6.0f;
    const float textWidth = label.empty() ? 0.0f : canvas.measureTextWidth(label, style.fontSize, style.fontWeight);
    const float total = iconSide + gap + textWidth;
    const float startX = rect.x + std::max(0.0f, (rect.width - total) / 2.0f);
    const Rect iconRect{startX, rect.y + (rect.height - iconSide) / 2.0f, iconSide, iconSide};
    paintIcon(canvas, *icon_, iconRect, style.foreground, Color{0, 0, 0, 0}, 1.6f);
    if (!label.empty()) {
        const Rect textRect{
            iconRect.x + iconSide + gap,
            rect.y,
            std::max(0.0f, rect.x + rect.width - (iconRect.x + iconSide + gap)),
            rect.height};
        canvas.drawTextStyled(label, textRect, style.foreground, style.fontSize, TextAlign::Left, style.fontWeight);
    }
}

bool Button::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }

    const bool nextHovered = contains(event.position);
    if (nextHovered == hovered_) {
        return false;
    }

    const ButtonStyle previous = resolvedStyle();
    hovered_ = nextHovered;
    if (!hovered_) {
        pressed_ = false;
    }
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    return true;
}

bool Button::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (!contains(event.position)) {
        return false;
    }

    const ButtonStyle previous = resolvedStyle();
    pressed_ = true;
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    return true;
}

bool Button::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }

    const bool wasPressed = pressed_;
    const ButtonStyle previous = resolvedStyle();
    pressed_ = false;
    beginVisualTransition(previous, resolvedStyle());
    invalidate();

    if (wasPressed && contains(event.position)) {
        click();
    }

    return wasPressed;
}

bool Button::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (event.key != Key::Enter && event.key != Key::Space) {
        return false;
    }
    click();
    return true;
}

CursorKind Button::cursor(Point point) const {
    return interactive() && contains(point) ? CursorKind::Pointer : CursorKind::Default;
}

bool Button::isFocusable() const {
    return interactive();
}

bool Button::tickAnimations(double nowMs) {
    bool running = false;
    running = backgroundTransition_.tick(nowMs) || running;
    running = foregroundTransition_.tick(nowMs) || running;
    running = borderTransition_.tick(nowMs) || running;
    if (running) {
        invalidate();
    }
    return running;
}

AccessibilityInfo Button::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::Button;
    }
    if (info.name.empty()) {
        info.name = text();
    }
    info.state.pressed = pressed_;
    return info;
}

void Button::click() {
    if (onClick_) {
        onClick_();
    }
}

ButtonStyle Button::resolvedStyle() const {
    return resolveButtonStyle(variant_, disabled(), hovered_, pressed_, focusVisible(), styleOverride_);
}

ButtonStyle Button::visualStyle(ButtonStyle target) const {
    if (!visualInitialized_) {
        return target;
    }

    target.background = backgroundTransition_.value();
    target.foreground = foregroundTransition_.value();
    target.border = borderTransition_.value();
    return target;
}

void Button::beginVisualTransition(ButtonStyle from, ButtonStyle target) {
    if (!hasAnimationScheduler()) {
        visualInitialized_ = false;
        return;
    }

    if (!visualInitialized_) {
        backgroundTransition_.reset(from.background);
        foregroundTransition_.reset(from.foreground);
        borderTransition_.reset(from.border);
        visualInitialized_ = true;
    }

    const double nowMs = currentTimeMs();
    backgroundTransition_.animateTo(target.background, nowMs, target.transition);
    foregroundTransition_.animateTo(target.foreground, nowMs, target.transition);
    borderTransition_.animateTo(target.border, nowMs, target.transition);
    if (backgroundTransition_.running() || foregroundTransition_.running() || borderTransition_.running()) {
        requestAnimationFrame();
    }
}

bool Button::hasInteractionState() const {
    return hovered_ || pressed_;
}

void Button::resetInteractionState() {
    hovered_ = false;
    pressed_ = false;
    const ButtonStyle target = resolvedStyle();
    backgroundTransition_.reset(target.background);
    foregroundTransition_.reset(target.foreground);
    borderTransition_.reset(target.border);
    visualInitialized_ = true;
}

} // namespace oneui
