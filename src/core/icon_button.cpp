#include "oneui/controls/icon_button.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace oneui {
namespace {

StyleSheet defaultIconButtonSheet() {
    StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .icon-button {
            background: #00000000;
            border-color: #00000000;
            border-width: 1px;
            border-radius: 6px;
            color: #c9cbd3;
        }
        .icon-button:hover {
            background: #2a2a31;
            border-color: #383842;
            color: #ffffff;
        }
        .icon-button:active {
            background: #202027;
            border-color: #30303a;
            color: #ffffff;
        }
        .icon-button:disabled {
            background: #00000000;
            border-color: #00000000;
            color: #777984;
        }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultIconButtonSheet();
    return sheet;
}

void paintIconPrimitive(Canvas& canvas, const IconPrimitive& primitive) {
    switch (primitive.kind) {
    case IconPrimitiveKind::Line:
        canvas.drawLine(primitive.from, primitive.to, primitive.color, primitive.strokeWidth);
        break;
    case IconPrimitiveKind::Rect:
        if (primitive.filled) {
            canvas.fillRect(primitive.rect, primitive.color);
        } else {
            canvas.strokeRect(primitive.rect, primitive.color, 0.0f, primitive.strokeWidth);
        }
        break;
    case IconPrimitiveKind::RoundRect:
        if (primitive.filled) {
            canvas.fillRect(primitive.rect, primitive.color, primitive.radius);
        } else {
            canvas.strokeRect(primitive.rect, primitive.color, primitive.radius, primitive.strokeWidth);
        }
        break;
    case IconPrimitiveKind::Circle:
        if (primitive.filled) {
            canvas.fillEllipse(primitive.rect, primitive.color);
        } else {
            canvas.strokeEllipse(primitive.rect, primitive.color, primitive.strokeWidth);
        }
        break;
    case IconPrimitiveKind::Polyline:
    case IconPrimitiveKind::Polygon:
        for (int i = 1; i < primitive.pointCount; ++i) {
            canvas.drawLine(
                primitive.points[static_cast<std::size_t>(i - 1)],
                primitive.points[static_cast<std::size_t>(i)],
                primitive.color,
                primitive.strokeWidth);
        }
        if (primitive.closed && primitive.pointCount > 2) {
            canvas.drawLine(
                primitive.points[static_cast<std::size_t>(primitive.pointCount - 1)],
                primitive.points[0],
                primitive.color,
                primitive.strokeWidth);
        }
        break;
    }
}

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

void prewarmIconButtonStyles(const StyleSheet& sheet, const StyleNode& node) {
    for (StylePseudoMask state : {
             StyleStateNone,
             StyleStateHover,
             StyleStateActive,
             StyleStateFocus,
             StyleStateDisabled}) {
        StyleNode warmed = node;
        warmed.state = state;
        sheet.resolve(warmed);
    }
}

} // namespace

IconButton::IconButton(IconSymbol symbol)
    : symbol_(symbol) {
    setPreferredSize(Size{32.0f, 32.0f});
    setAccessibleRole(AccessibilityRole::Button);
    updateAccessibility();
}

void IconButton::setSymbol(IconSymbol symbol) {
    symbol_ = symbol;
    invalidate();
}

IconSymbol IconButton::symbol() const {
    return symbol_;
}

void IconButton::setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node) {
    const StyleBox previous = resolvedStyle();
    styleSheet_ = std::move(sheet);
    styleNode_ = std::move(node);
    if (styleSheet_) {
        prewarmIconButtonStyles(*styleSheet_, styleNode_);
    }
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
}

void IconButton::setOnClick(std::function<void()> callback) {
    onClick_ = std::move(callback);
}

void IconButton::paint(Canvas& canvas) {
    const Rect rect = frame();
    const StyleBox box = visualStyle(resolvedStyle());
    paintStyleBox(canvas, rect, box);

    const float iconSize = std::max(12.0f, std::min(rect.width, rect.height) - 14.0f);
    const Rect iconRect{
        rect.x + (rect.width - iconSize) * 0.5f,
        rect.y + (rect.height - iconSize) * 0.5f,
        iconSize,
        iconSize};
    const Color color = box.foreground.value_or(Color{201, 203, 211});
    const float stroke = box.borderWidth.value_or(1.5f);
    for (const auto& primitive : buildIconPrimitives(symbol_, iconRect, color, Color{0, 0, 0, 0}, stroke)) {
        paintIconPrimitive(canvas, primitive);
    }
}

bool IconButton::onMouseMove(const MouseEvent& event) {
    const bool next = interactive() && contains(event.position);
    if (hovered_ == next) {
        return false;
    }
    const StyleBox previous = resolvedStyle();
    hovered_ = next;
    if (!hovered_) {
        pressed_ = false;
    }
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    return true;
}

bool IconButton::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position)) {
        return false;
    }
    const StyleBox previous = resolvedStyle();
    pressed_ = true;
    setFocused(true);
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    return true;
}

bool IconButton::onMouseUp(const MouseEvent& event) {
    if (!pressed_) {
        return false;
    }
    const StyleBox previous = resolvedStyle();
    pressed_ = false;
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    if (interactive() && contains(event.position) && onClick_) {
        onClick_();
    }
    return true;
}

bool IconButton::onFocusChanged(bool focused) {
    const StyleBox previous = resolvedStyle();
    if (!Widget::onFocusChanged(focused)) {
        return false;
    }
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    return true;
}

CursorKind IconButton::cursor(Point point) const {
    return interactive() && contains(point) ? CursorKind::Pointer : CursorKind::Default;
}

bool IconButton::isFocusable() const {
    return !disabled();
}

void IconButton::setDisabled(bool disabled) {
    const StyleBox previous = resolvedStyle();
    Widget::setDisabled(disabled);
    beginVisualTransition(previous, resolvedStyle());
    updateAccessibility();
    invalidate();
}

StyleBox IconButton::resolvedStyle() const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    StyleNode node = styleNode_;
    node.state = StyleStateNone;
    if (disabled()) {
        node.state = StyleStateDisabled;
    } else if (pressed_) {
        node.state = StyleStateActive;
    } else if (hovered_) {
        node.state = StyleStateHover;
    } else if (focusVisible()) {
        node.state = StyleStateFocus;
    }
    return sheet.resolve(node);
}

StyleBox IconButton::visualStyle(StyleBox target) const {
    return visualTransition_.applyTo(std::move(target));
}

void IconButton::beginVisualTransition(StyleBox from, StyleBox target) {
    if (!hasAnimationScheduler()) {
        visualTransition_ = StyleBoxTransition{};
        return;
    }

    visualTransition_.animateTo(from, target, currentTimeMs());
    if (visualTransition_.running()) {
        requestAnimationFrame();
    }
}

bool IconButton::tickAnimations(double nowMs) {
    const bool running = visualTransition_.tick(nowMs);
    if (running) {
        invalidate();
    }
    return running;
}

void IconButton::updateAccessibility() {
    auto state = accessibilityState();
    state.disabled = disabled();
    state.pressed = pressed_;
    setAccessibilityState(state);
}

bool IconButton::hasInteractionState() const {
    return hovered_ || pressed_;
}

void IconButton::resetInteractionState() {
    hovered_ = false;
    pressed_ = false;
    visualTransition_.reset(resolvedStyle());
}

} // namespace oneui
