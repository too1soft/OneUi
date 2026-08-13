#include "oneui/controls/toast.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace oneui {
namespace {

StyleSheet defaultToastSheet() {
    StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .toast {
            background: #2b2b31;
            border-color: #3d3d46;
            border-width: 1px;
            border-radius: 8px;
            color: #f4f5f8;
            padding: 14px 16px 14px 16px;
            box-shadow: 0px 10px 24px 0px #00000055;
        }
        .toast-action {
            background: #00000000;
            border-color: #484a56;
            border-width: 1px;
            border-radius: 6px;
            color: #f4f5f8;
            font-size: 12px;
            font-weight: 500;
        }
        .toast-action:hover {
            background: #3a3a43;
            border-color: #545766;
        }
        .toast-action:active {
            background: #27272e;
            border-color: #3a3d48;
        }
        .toast-action.primary {
            background: #3370eb;
            border-color: #3370eb;
            color: #ffffff;
        }
        .toast-action.primary:hover {
            background: #3e7cf8;
            border-color: #3e7cf8;
        }
        .toast-action.primary:active {
            background: #2b5cc6;
            border-color: #2b5cc6;
        }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultToastSheet();
    return sheet;
}

} // namespace

Toast::Toast(std::wstring title, std::wstring message)
    : title_(std::move(title))
    , message_(std::move(message)) {
    setPreferredSize(Size{330.0f, 104.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(title_);
    setAccessibleValue(message_);
}

void Toast::setTitle(std::wstring title) {
    title_ = std::move(title);
    setAccessibleName(title_);
    invalidate();
}

void Toast::setMessage(std::wstring message) {
    message_ = std::move(message);
    setAccessibleValue(message_);
    invalidate();
}

void Toast::setPrimaryAction(std::wstring text) {
    primaryAction_ = std::move(text);
    invalidate();
}

void Toast::setSecondaryAction(std::wstring text) {
    secondaryAction_ = std::move(text);
    invalidate();
}

void Toast::setIconSymbol(IconSymbol symbol) {
    iconSymbol_ = symbol;
    invalidate();
}

void Toast::clearIconSymbol() {
    iconSymbol_.reset();
    invalidate();
}

void Toast::setCloseVisible(bool visible) {
    closeVisible_ = visible;
    invalidate();
}

void Toast::setOnPrimaryAction(std::function<void()> callback) {
    onPrimaryAction_ = std::move(callback);
}

void Toast::setOnSecondaryAction(std::function<void()> callback) {
    onSecondaryAction_ = std::move(callback);
}

void Toast::setOnClose(std::function<void()> callback) {
    onClose_ = std::move(callback);
}

void Toast::setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node) {
    styleSheet_ = std::move(sheet);
    styleNode_ = std::move(node);
    invalidate();
}

void Toast::paint(Canvas& canvas) {
    const StyleBox box = resolvedStyle();
    const Color foreground = box.foreground.value_or(Color{244, 245, 248});
    const int fontWeight = box.fontWeight.value_or(600);
    const Color muted{186, 188, 198};
    const Layout l = layout();

    paintStyleBox(canvas, frame(), box);

    if (iconSymbol_) {
        paintIcon(canvas, *iconSymbol_, l.icon, Color{255, 159, 56}, Color{0, 0, 0, 0});
    }
    canvas.drawTextStyledEllipsized(title_, l.title, foreground, 13.0f, TextAlign::Left, fontWeight);
    canvas.drawTextEllipsized(message_, l.message, muted, 12.0f, TextAlign::Left);

    auto paintButton = [&](Rect rect, const std::wstring& text, Action action, bool primary) {
        if (text.empty()) {
            return;
        }
        (void)primary;
        const StyleBox actionStyle = resolvedActionStyle(action);
        paintStyleBox(canvas, rect, actionStyle);
        canvas.drawTextStyledEllipsized(
            text,
            rect,
            actionStyle.foreground.value_or(foreground),
            actionStyle.fontSize.value_or(12.0f),
            TextAlign::Center,
            actionStyle.fontWeight.value_or(500));
    };

    paintButton(l.secondary, secondaryAction_, Action::Secondary, false);
    paintButton(l.primary, primaryAction_, Action::Primary, true);

    if (closeVisible_) {
        const StyleBox closeStyle = resolvedActionStyle(Action::Close);
        const Color closeColor = closeStyle.foreground.value_or(hoveredAction_ == Action::Close ? foreground : Color{169, 172, 183});
        paintIcon(canvas, IconSymbol::Close, l.close, closeColor);
    }
}

bool Toast::onMouseMove(const MouseEvent& event) {
    const Action next = interactive() ? actionAt(event.position) : Action::None;
    if (hoveredAction_ == next) {
        return false;
    }
    hoveredAction_ = next;
    if (hoveredAction_ == Action::None) {
        pressedAction_ = Action::None;
    }
    invalidate();
    return true;
}

bool Toast::onMouseDown(const MouseEvent& event) {
    const Action action = interactive() ? actionAt(event.position) : Action::None;
    if (action == Action::None) {
        return false;
    }
    pressedAction_ = action;
    setFocused(true);
    invalidate();
    return true;
}

bool Toast::onMouseUp(const MouseEvent& event) {
    if (pressedAction_ == Action::None) {
        return false;
    }
    const Action pressed = pressedAction_;
    pressedAction_ = Action::None;
    invalidate();
    if (!interactive() || actionAt(event.position) != pressed) {
        return true;
    }
    if (pressed == Action::Primary && onPrimaryAction_) {
        onPrimaryAction_();
    } else if (pressed == Action::Secondary && onSecondaryAction_) {
        onSecondaryAction_();
    } else if (pressed == Action::Close) {
        const auto callback = onClose_;
        if (callback) {
            callback();
        }
    }
    return true;
}

bool Toast::isFocusable() const {
    return !disabled();
}

bool Toast::onFocusChanged(bool focused) {
    Widget::onFocusChanged(focused);
    invalidate();
    return true;
}

Toast::Layout Toast::layout() const {
    const Rect rect = frame();
    const StyleBox box = resolvedStyle();
    const Insets padding = box.padding.value_or(Insets{14.0f, 16.0f, 14.0f, 16.0f});
    const Rect content = rect.inset(padding);
    const float iconWidth = iconSymbol_ ? 26.0f : 0.0f;
    const float closeWidth = closeVisible_ ? 24.0f : 0.0f;
    const float primaryWidth = primaryAction_.empty() ? 0.0f : 94.0f;
    const float secondaryWidth = secondaryAction_.empty() ? 0.0f : 58.0f;
    const float actionGap = secondaryWidth > 0.0f && primaryWidth > 0.0f ? 8.0f : 0.0f;
    const float actionsWidth = secondaryWidth + actionGap + primaryWidth;
    const float textLeft = content.x + iconWidth;
    const float inlineTextWidth = content.width - iconWidth - actionsWidth - (actionsWidth > 0.0f ? 12.0f : 0.0f);
    const float stackedContentHeight = 20.0f + 3.0f + 20.0f + 10.0f + 34.0f;
    const bool stackActions = actionsWidth > 0.0f && inlineTextWidth < 172.0f && content.height >= stackedContentHeight;
    const float actionsX = content.x + content.width - actionsWidth;
    const float textRight = stackActions
        ? content.x + content.width - closeWidth - (closeVisible_ ? 6.0f : 0.0f)
        : std::max(textLeft, actionsX - 12.0f);

    Layout result;
    result.icon = Rect{content.x, content.y + 4.0f, 18.0f, 18.0f};
    result.close = Rect{content.x + content.width - closeWidth + 4.0f, content.y - 4.0f, closeWidth, 24.0f};
    result.title = Rect{textLeft, content.y, std::max(0.0f, textRight - textLeft), 20.0f};
    result.message = Rect{textLeft, content.y + 23.0f, std::max(0.0f, textRight - textLeft), 20.0f};
    const float actionY = stackActions
        ? content.y + std::max(43.0f, content.height - 34.0f)
        : content.y + std::max(0.0f, (content.height - 34.0f) / 2.0f);
    result.secondary = Rect{actionsX, actionY, secondaryWidth, 34.0f};
    result.primary = Rect{actionsX + secondaryWidth + actionGap, result.secondary.y, primaryWidth, 34.0f};
    return result;
}

StyleBox Toast::resolvedStyle() const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    StylePseudoMask state = disabled() ? StyleStateDisabled : StyleStateNone;
    if (focused() && focusVisible()) {
        state |= StyleStateFocus;
    }
    StyleNode node = styleNode_;
    node.state = state;
    return sheet.resolve(node);
}

StyleBox Toast::resolvedActionStyle(Action action) const {
    StylePseudoMask state = disabled() ? StyleStateDisabled : StyleStateNone;
    if (!disabled() && pressedAction_ == action) {
        state |= StyleStateActive;
    } else if (!disabled() && hoveredAction_ == action) {
        state |= StyleStateHover;
    }
    if (focused() && focusVisible()) {
        state |= StyleStateFocus;
    }

    switch (action) {
    case Action::Primary:
        return (styleSheet_ ? *styleSheet_ : fallbackSheet())
            .resolve(childStyleNode({"toast-action", "primary"}, state));
    case Action::Secondary:
        return (styleSheet_ ? *styleSheet_ : fallbackSheet())
            .resolve(childStyleNode({"toast-action", "secondary"}, state));
    case Action::Close:
        return (styleSheet_ ? *styleSheet_ : fallbackSheet())
            .resolve(childStyleNode({"toast-action", "close"}, state));
    case Action::None:
        break;
    }
    return {};
}

StyleNode Toast::childStyleNode(const std::vector<std::string>& classes, StylePseudoMask state) const {
    StyleNode node{"button", {}, state};
    for (const auto& klass : styleNode_.classes) {
        if (klass != styleNode_.tag) {
            node.classes.push_back(klass);
        }
    }
    node.classes.insert(node.classes.end(), classes.begin(), classes.end());
    return node;
}

Toast::Action Toast::actionAt(Point point) const {
    const Layout l = layout();
    if (!primaryAction_.empty() && l.primary.contains(point)) {
        return Action::Primary;
    }
    if (!secondaryAction_.empty() && l.secondary.contains(point)) {
        return Action::Secondary;
    }
    if (closeVisible_ && l.close.contains(point)) {
        return Action::Close;
    }
    return Action::None;
}

void Toast::resetInteractionState() {
    hoveredAction_ = Action::None;
    pressedAction_ = Action::None;
}

} // namespace oneui
