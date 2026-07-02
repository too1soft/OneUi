#include "oneui/controls/nav_item.h"

#include <algorithm>
#include <chrono>

namespace oneui {
namespace {

StyleSheet defaultNavItemSheet() {
    StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .nav-item {
            background: #1f1f24;
            border-color: #1f1f24;
            border-width: 1px;
            border-radius: 5px;
            color: #dadbe1;
        }
        .nav-item:hover {
            background: #2b2b32;
            border-color: #41414a;
        }
        .nav-item:active {
            background: #33333b;
            border-color: #494952;
        }
        .nav-item:selected {
            background: #3b3843;
            border-color: #484451;
            color: #ff3870;
        }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultNavItemSheet();
    return sheet;
}

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

void prewarmNavItemStyles(const StyleSheet& sheet) {
    sheet.resolve(StyleNode{"button", {"nav-item"}, StyleStateNone});
    sheet.resolve(StyleNode{"button", {"nav-item"}, StyleStateHover});
    sheet.resolve(StyleNode{"button", {"nav-item"}, StyleStateActive});
    sheet.resolve(StyleNode{"button", {"nav-item", "selected"}, StyleStateSelected});
    sheet.resolve(StyleNode{"button", {"nav-item"}, StyleStateDisabled});
}

} // namespace

NavItem::NavItem(std::wstring text, IconSymbol symbol)
    : text_(std::move(text))
    , symbol_(symbol) {
    setPreferredSize(Size{0.0f, 38.0f});
    setAccessibleRole(AccessibilityRole::Button);
    updateAccessibility();
}

void NavItem::setText(std::wstring text) {
    text_ = std::move(text);
    updateAccessibility();
    invalidate();
}

void NavItem::setSymbol(IconSymbol symbol) {
    symbol_ = symbol;
    invalidate();
}

void NavItem::setSelected(bool selected) {
    if (selected_ == selected) {
        return;
    }
    const StyleBox previous = resolvedItemStyle();
    selected_ = selected;
    beginVisualTransition(previous, resolvedItemStyle());
    updateAccessibility();
    invalidate();
}

bool NavItem::selected() const {
    return selected_;
}

void NavItem::setStyleSheet(std::shared_ptr<StyleSheet> sheet) {
    const StyleBox previous = resolvedItemStyle();
    styleSheet_ = std::move(sheet);
    clearStyleCache();
    if (styleSheet_) {
        prewarmNavItemStyles(*styleSheet_);
    }
    cachedItemStyle(StyleStateNone);
    cachedItemStyle(StyleStateHover);
    cachedItemStyle(StyleStateActive);
    cachedItemStyle(StyleStateSelected);
    cachedItemStyle(StyleStateDisabled);
    beginVisualTransition(previous, resolvedItemStyle());
    invalidate();
}

void NavItem::setOnClick(std::function<void()> callback) {
    onClick_ = std::move(callback);
}

SidebarNavItemBridgeLayout NavItem::layout() const {
    const Rect bounds = frame();
    const StyleBox targetStyle = resolvedItemStyle();
    SidebarNavItemBridgeLayout item;
    item.frame = bounds;
    item.icon = Rect{bounds.x + 15.0f, bounds.y + 11.0f, 16.0f, 16.0f};
    item.symbol = symbol_;
    item.style = visualItemStyle(targetStyle);
    item.label = Rect{
        bounds.x + 39.0f,
        bounds.y,
        std::max(0.0f, bounds.width - 48.0f),
        bounds.height};
    const Color fallback = disabled() ? Color{139, 145, 158} :
        selected_ ? Color{255, 47, 105} : Color{202, 205, 214};
    item.foreground = item.style.foreground.value_or(targetStyle.foreground.value_or(fallback));
    if (item.foreground.a == 0) {
        item.foreground = targetStyle.foreground.value_or(fallback);
    }
    const float opacity = std::max(0.0f, std::min(1.0f, item.style.opacity.value_or(1.0f)));
    item.foreground.a = static_cast<unsigned char>(static_cast<float>(item.foreground.a) * opacity);
    return item;
}

void NavItem::paint(Canvas& canvas) {
    const auto item = layout();
    paintStyleBox(canvas, item.frame, item.style);
    paintIcon(canvas, item.symbol, item.icon, item.foreground);
    canvas.drawText(text_, item.label, item.foreground, 13.0f, TextAlign::Left);
}

bool NavItem::onMouseMove(const MouseEvent& event) {
    const bool next = interactive() && contains(event.position);
    if (hovered_ == next) {
        return false;
    }
    const StyleBox previous = resolvedItemStyle();
    hovered_ = next;
    beginVisualTransition(previous, resolvedItemStyle());
    invalidate();
    return true;
}

bool NavItem::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position)) {
        return false;
    }
    const StyleBox previous = resolvedItemStyle();
    pressed_ = true;
    setFocused(true);
    beginVisualTransition(previous, resolvedItemStyle());
    invalidate();
    return true;
}

bool NavItem::onMouseUp(const MouseEvent& event) {
    if (!pressed_) {
        return false;
    }
    const StyleBox previous = resolvedItemStyle();
    pressed_ = false;
    beginVisualTransition(previous, resolvedItemStyle());
    invalidate();
    if (interactive() && contains(event.position) && onClick_) {
        onClick_();
    }
    return true;
}

bool NavItem::onFocusChanged(bool focused) {
    const StyleBox previous = resolvedItemStyle();
    if (!Widget::onFocusChanged(focused)) {
        return false;
    }
    beginVisualTransition(previous, resolvedItemStyle());
    invalidate();
    return true;
}

CursorKind NavItem::cursor(Point point) const {
    return interactive() && contains(point) ? CursorKind::Pointer : CursorKind::Default;
}

bool NavItem::isFocusable() const {
    return !disabled();
}

void NavItem::setDisabled(bool disabled) {
    const StyleBox previous = resolvedItemStyle();
    Widget::setDisabled(disabled);
    beginVisualTransition(previous, resolvedItemStyle());
    updateAccessibility();
    invalidate();
}

StyleBox NavItem::resolvedItemStyle() const {
    return cachedItemStyle(currentStateMask());
}

StyleBox NavItem::cachedItemStyle(StylePseudoMask state) const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    if (styleCacheVersion_ != sheet.version()) {
        clearStyleCache();
        styleCacheVersion_ = sheet.version();
    }

    const int key = static_cast<int>(state);
    if (auto cached = styleCache_.find(key); cached != styleCache_.end()) {
        return cached->second;
    }

    StyleNode node = (state & StyleStateSelected) != 0
        ? StyleNode{"button", {"nav-item", "selected"}, StyleStateSelected}
        : StyleNode{"button", {"nav-item"}, StyleStateNone};
    node.state |= state;
    StyleBox style = sheet.resolve(node);
    styleCache_.emplace(key, style);
    return style;
}

StylePseudoMask NavItem::currentStateMask() const {
    StylePseudoMask state = StyleStateNone;
    if (selected_) {
        state |= StyleStateSelected;
    }
    if (hovered_ && !disabled()) {
        state |= StyleStateHover;
    }
    if (pressed_ && !disabled()) {
        state |= StyleStateActive;
    }
    if (disabled()) {
        state |= StyleStateDisabled;
    }
    return state;
}

void NavItem::clearStyleCache() const {
    styleCache_.clear();
}

StyleBox NavItem::visualItemStyle(StyleBox target) const {
    return visualTransition_.applyTo(std::move(target));
}

void NavItem::beginVisualTransition(StyleBox from, StyleBox target) {
    if (!hasAnimationScheduler()) {
        visualTransition_ = StyleBoxTransition{};
        return;
    }

    visualTransition_.animateTo(from, target, currentTimeMs());
    if (visualTransition_.running()) {
        requestAnimationFrame();
    }
}

bool NavItem::tickAnimations(double nowMs) {
    const bool running = visualTransition_.tick(nowMs);
    if (running) {
        invalidate();
    }
    return running;
}

void NavItem::updateAccessibility() {
    setAccessibleName(text_);
    auto state = accessibilityState();
    state.selected = selected_;
    state.disabled = disabled();
    setAccessibilityState(state);
}

bool NavItem::hasInteractionState() const {
    return hovered_ || pressed_;
}

void NavItem::resetInteractionState() {
    hovered_ = false;
    pressed_ = false;
    visualTransition_.reset(resolvedItemStyle());
}

} // namespace oneui
