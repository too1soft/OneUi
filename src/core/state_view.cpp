#include "oneui/controls/state_view.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

StyleSheet defaultStateViewSheet() {
    StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .state-view {
            background: #00000000;
            color: #eeeff5;
            padding: 24px;
        }
        .state-view-icon {
            color: #818cf8;
        }
        .state-view-title {
            color: #eeeff5;
            font-size: 18px;
            font-weight: 600;
        }
        .state-view-message {
            color: #aeb1bc;
            font-size: 13px;
            font-weight: 400;
        }
        .state-view-action {
            background: #6155e7;
            border-color: #7065ee;
            border-width: 1px;
            border-radius: 6px;
            color: #ffffff;
            font-size: 13px;
            font-weight: 500;
        }
        .state-view-action:hover {
            background: #6f63ef;
        }
        .state-view-action:active {
            background: #5549d4;
        }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultStateViewSheet();
    return sheet;
}

} // namespace

StateView::StateView(std::wstring title, std::wstring message)
    : title_(std::move(title))
    , message_(std::move(message)) {
    setPreferredSize(Size{0.0f, 220.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(title_);
    setAccessibleDescription(message_);
}

void StateView::setTitle(std::wstring title) {
    title_ = std::move(title);
    setAccessibleName(title_);
    invalidate();
}

void StateView::setMessage(std::wstring message) {
    message_ = std::move(message);
    setAccessibleDescription(message_);
    invalidate();
}

void StateView::setIcon(IconSymbol symbol) {
    icon_ = symbol;
    invalidate();
}

void StateView::setAction(std::wstring text) {
    action_ = std::move(text);
    if (action_.empty()) {
        hovered_ = false;
        pressed_ = false;
    }
    invalidate();
}

void StateView::setOnAction(std::function<void()> callback) {
    onAction_ = std::move(callback);
}

void StateView::setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node) {
    styleSheet_ = std::move(sheet);
    styleNode_ = std::move(node);
    invalidate();
}

void StateView::paint(Canvas& canvas) {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    const StyleBox box = resolvedStyle();
    paintStyleBox(canvas, frame(), box);
    const Layout l = layout();

    const StyleBox iconStyle = sheet.resolve(childStyleNode({"state-view-icon"}, StyleStateNone));
    paintIcon(
        canvas,
        icon_,
        l.icon,
        iconStyle.foreground.value_or(Color{129, 140, 248}),
        Color{0, 0, 0, 0},
        1.6f);

    const StyleBox titleStyle = sheet.resolve(childStyleNode({"state-view-title"}, StyleStateNone));
    canvas.drawTextStyled(
        title_,
        l.title,
        titleStyle.foreground.value_or(box.foreground.value_or(Color{238, 239, 245})),
        titleStyle.fontSize.value_or(18.0f),
        TextAlign::Center,
        titleStyle.fontWeight.value_or(600));

    const StyleBox messageStyle = sheet.resolve(childStyleNode({"state-view-message"}, StyleStateNone));
    canvas.drawTextStyled(
        message_,
        l.message,
        messageStyle.foreground.value_or(Color{174, 177, 188}),
        messageStyle.fontSize.value_or(13.0f),
        TextAlign::Center,
        messageStyle.fontWeight.value_or(400));

    if (!action_.empty()) {
        const StyleBox actionStyle = resolvedActionStyle();
        paintStyleBox(canvas, l.action, actionStyle);
        canvas.drawTextStyled(
            action_,
            l.action,
            actionStyle.foreground.value_or(Color{255, 255, 255}),
            actionStyle.fontSize.value_or(13.0f),
            TextAlign::Center,
            actionStyle.fontWeight.value_or(500));
    }
}

bool StateView::onMouseMove(const MouseEvent& event) {
    const bool hovered = interactive() && actionAt(event.position);
    if (hovered_ == hovered) {
        return false;
    }
    hovered_ = hovered;
    if (!hovered_) {
        pressed_ = false;
    }
    invalidate();
    return true;
}

bool StateView::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !actionAt(event.position)) {
        return false;
    }
    pressed_ = true;
    setFocused(true);
    invalidate();
    return true;
}

bool StateView::onMouseUp(const MouseEvent& event) {
    if (!pressed_) {
        return false;
    }
    pressed_ = false;
    invalidate();
    if (interactive() && actionAt(event.position) && onAction_) {
        onAction_();
    }
    return true;
}

bool StateView::isFocusable() const {
    return !disabled() && !action_.empty();
}

bool StateView::onFocusChanged(bool focused) {
    if (!Widget::onFocusChanged(focused)) {
        return false;
    }
    invalidate();
    return true;
}

StateView::Layout StateView::layout() const {
    const Rect content = frame().inset(resolvedStyle().padding.value_or(Insets{24.0f, 24.0f, 24.0f, 24.0f}));
    const float width = std::min(520.0f, std::max(0.0f, content.width));
    const float x = content.x + (content.width - width) * 0.5f;
    const float groupHeight = action_.empty() ? 122.0f : 172.0f;
    const float y = content.y + std::max(0.0f, (content.height - groupHeight) * 0.5f);

    Layout result;
    result.icon = Rect{content.x + (content.width - 40.0f) * 0.5f, y, 40.0f, 40.0f};
    result.title = Rect{x, y + 52.0f, width, 28.0f};
    result.message = Rect{x, y + 84.0f, width, 34.0f};
    result.action = Rect{content.x + (content.width - 120.0f) * 0.5f, y + 136.0f, 120.0f, 36.0f};
    return result;
}

StyleBox StateView::resolvedStyle() const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    StyleNode node = styleNode_;
    node.state = disabled() ? StyleStateDisabled : StyleStateNone;
    return sheet.resolve(node);
}

StyleBox StateView::resolvedActionStyle() const {
    StylePseudoMask state = disabled() ? StyleStateDisabled : StyleStateNone;
    if (!disabled() && pressed_) {
        state |= StyleStateActive;
    } else if (!disabled() && hovered_) {
        state |= StyleStateHover;
    }
    if (focused() && focusVisible()) {
        state |= StyleStateFocus;
    }
    return (styleSheet_ ? *styleSheet_ : fallbackSheet())
        .resolve(childStyleNode({"state-view-action", "primary"}, state));
}

StyleNode StateView::childStyleNode(const std::vector<std::string>& classes, StylePseudoMask state) const {
    StyleNode node{"state-view-part", {}, state};
    for (const auto& klass : styleNode_.classes) {
        if (klass != styleNode_.tag) {
            node.classes.push_back(klass);
        }
    }
    node.classes.insert(node.classes.end(), classes.begin(), classes.end());
    return node;
}

bool StateView::actionAt(Point point) const {
    return !action_.empty() && layout().action.contains(point);
}

bool StateView::hasInteractionState() const {
    return hovered_ || pressed_;
}

void StateView::resetInteractionState() {
    hovered_ = false;
    pressed_ = false;
}

} // namespace oneui
