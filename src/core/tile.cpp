#include "oneui/controls/tile.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace oneui {
namespace {

StyleSheet defaultTileSheet() {
    StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .tile {
            background: #2f6fed;
            border-color: #5b8cff;
            border-width: 1px;
            border-radius: 7px;
            color: #ffffff;
            padding: 12px 14px 12px 14px;
            box-shadow: 0px 8px 18px 0px #00000040;
        }
        .tile:hover {
            background: #3a7cff;
        }
        .tile:active {
            background: #265bd8;
        }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultTileSheet();
    return sheet;
}

void prewarmTileStyles(const StyleSheet& sheet, const StyleNode& node) {
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

Tile::Tile(std::wstring title, std::wstring subtitle)
    : title_(std::move(title))
    , subtitle_(std::move(subtitle)) {
    setPreferredSize(Size{0.0f, 88.0f});
    setAccessibleRole(AccessibilityRole::Button);
    updateAccessibility();
}

void Tile::setTitle(std::wstring title) {
    title_ = std::move(title);
    updateAccessibility();
    invalidate();
}

void Tile::setSubtitle(std::wstring subtitle) {
    subtitle_ = std::move(subtitle);
    updateAccessibility();
    invalidate();
}

void Tile::setLeadingSymbol(IconSymbol symbol) {
    leadingSymbol_ = symbol;
    invalidate();
}

void Tile::clearLeadingSymbol() {
    leadingSymbol_.reset();
    invalidate();
}

void Tile::setTrailingSymbol(IconSymbol symbol) {
    trailingSymbol_ = symbol;
    invalidate();
}

void Tile::clearTrailingSymbol() {
    trailingSymbol_.reset();
    invalidate();
}

void Tile::setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node) {
    styleSheet_ = std::move(sheet);
    styleNode_ = std::move(node);
    if (styleSheet_) {
        prewarmTileStyles(*styleSheet_, styleNode_);
    }
    invalidate();
}

void Tile::setOnClick(std::function<void()> callback) {
    onClick_ = std::move(callback);
}

void Tile::paint(Canvas& canvas) {
    const Rect rect = frame();
    const StyleBox box = resolvedStyle();
    const Color foreground = box.foreground.value_or(Color{255, 255, 255});
    const float fontSize = box.fontSize.value_or(13.0f);
    const int fontWeight = box.fontWeight.value_or(400);
    const Insets padding = box.padding.value_or(Insets{12.0f, 14.0f, 12.0f, 14.0f});

    paintStyleBox(canvas, rect, box);

    const float left = rect.x + padding.left;
    const float top = rect.y + padding.top;
    const float right = rect.x + std::max(0.0f, rect.width - padding.right);
    const float bottom = rect.y + std::max(0.0f, rect.height - padding.bottom);

    float titleX = left;
    if (leadingSymbol_) {
        const Rect iconRect{left, top + 2.0f, 18.0f, 18.0f};
        paintIcon(canvas, *leadingSymbol_, iconRect, foreground, Color{0, 0, 0, 0}, 1.4f);
        titleX += 26.0f;
    }

    if (trailingSymbol_) {
        paintIcon(canvas, *trailingSymbol_, Rect{right - 18.0f, top + 1.0f, 16.0f, 16.0f}, foreground, Color{0, 0, 0, 0}, 1.4f);
    }

    canvas.drawTextStyledEllipsized(title_, Rect{titleX, top, std::max(0.0f, right - titleX - 24.0f), 22.0f}, foreground, fontSize, TextAlign::Left, fontWeight);
    canvas.drawTextStyledEllipsized(subtitle_, Rect{left, bottom - 24.0f, std::max(0.0f, right - left), 22.0f}, foreground, fontSize, TextAlign::Left, fontWeight);
}

bool Tile::onMouseMove(const MouseEvent& event) {
    const bool next = interactive() && contains(event.position);
    if (hovered_ == next) {
        return false;
    }
    hovered_ = next;
    invalidate();
    return true;
}

bool Tile::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position)) {
        return false;
    }
    pressed_ = true;
    setFocused(true);
    invalidate();
    return true;
}

bool Tile::onMouseUp(const MouseEvent& event) {
    if (!pressed_) {
        return false;
    }
    pressed_ = false;
    invalidate();
    if (interactive() && contains(event.position) && onClick_) {
        onClick_();
    }
    return true;
}

bool Tile::onFocusChanged(bool focused) {
    if (!Widget::onFocusChanged(focused)) {
        return false;
    }
    invalidate();
    return true;
}

bool Tile::isFocusable() const {
    return !disabled();
}

void Tile::setDisabled(bool disabled) {
    Widget::setDisabled(disabled);
    updateAccessibility();
    invalidate();
}

StyleBox Tile::resolvedStyle() const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    StylePseudoMask state = disabled() ? StyleStateDisabled : StyleStateNone;
    if (!disabled() && hovered_) {
        state |= StyleStateHover;
    }
    if (!disabled() && pressed_) {
        state |= StyleStateActive;
    }
    if (focused() && focusVisible()) {
        state |= StyleStateFocus;
    }
    StyleNode node = styleNode_;
    node.state = state;
    return sheet.resolve(node);
}

void Tile::updateAccessibility() {
    setAccessibleName(title_);
    setAccessibleValue(subtitle_);
    auto state = accessibilityState();
    state.disabled = disabled();
    setAccessibilityState(state);
}

bool Tile::hasInteractionState() const {
    return hovered_ || pressed_;
}

void Tile::resetInteractionState() {
    hovered_ = false;
    pressed_ = false;
}

} // namespace oneui
