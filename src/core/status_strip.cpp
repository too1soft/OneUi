#include "oneui/controls/status_strip.h"

#include "oneui/icon.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

StyleSheet defaultStatusStripSheet() {
    StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .status-strip {
            background: #1f1f24;
            border-color: #383840;
            border-width: 1px;
            border-radius: 7px;
            color: #eeeff5;
            padding: 12px 18px 12px 18px;
        }
        .status-strip-action {
            background: #00000000;
            border-color: #383a44;
            border-width: 1px;
            border-radius: 6px;
            color: #eeeff5;
            font-size: 12px;
            font-weight: 500;
        }
        .status-strip-action:hover {
            background: #2b2b32;
            border-color: #484a56;
        }
        .status-strip-action:active {
            background: #23232a;
            border-color: #3f414c;
        }
        .status-strip-action.link {
            background: #00000000;
            border-width: 0px;
            color: #3158d4;
        }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultStatusStripSheet();
    return sheet;
}

} // namespace

StatusStrip::StatusStrip(std::wstring title, std::wstring message)
    : title_(std::move(title))
    , message_(std::move(message)) {
    setPreferredSize(Size{0.0f, 62.0f});
    setAccessibleRole(AccessibilityRole::Custom);
}

void StatusStrip::setTitle(std::wstring title) {
    title_ = std::move(title);
    invalidate();
}

void StatusStrip::setMessage(std::wstring message) {
    message_ = std::move(message);
    invalidate();
}

void StatusStrip::setIconSymbol(IconSymbol symbol) {
    if (iconSymbol_ == symbol) {
        return;
    }
    iconSymbol_ = symbol;
    invalidate();
}

void StatusStrip::setPrimaryAction(std::wstring text) {
    primaryAction_ = std::move(text);
    invalidate();
}

void StatusStrip::setSecondaryAction(std::wstring text) {
    secondaryAction_ = std::move(text);
    invalidate();
}

void StatusStrip::setPrimaryActionPresentation(StatusStripActionPresentation presentation) {
    if (primaryActionPresentation_ == presentation) {
        return;
    }
    primaryActionPresentation_ = presentation;
    invalidate();
}

void StatusStrip::setPrimaryActionTrailingIcon(std::optional<IconSymbol> symbol) {
    if (primaryActionTrailingIcon_ == symbol) {
        return;
    }
    primaryActionTrailingIcon_ = symbol;
    invalidate();
}

void StatusStrip::setOnPrimaryAction(std::function<void()> callback) {
    onPrimaryAction_ = std::move(callback);
}

void StatusStrip::setOnSecondaryAction(std::function<void()> callback) {
    onSecondaryAction_ = std::move(callback);
}

void StatusStrip::setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node) {
    styleSheet_ = std::move(sheet);
    styleNode_ = std::move(node);
    invalidate();
}

void StatusStrip::paint(Canvas& canvas) {
    const StyleBox box = resolvedStyle();
    const Color foreground = box.foreground.value_or(Color{238, 239, 245});
    const int fontWeight = box.fontWeight.value_or(400);
    paintStyleBox(canvas, frame(), box);

    const Layout l = layout();
    paintIcon(canvas, iconSymbol_, l.icon, Color{49, 88, 212}, Color{0, 0, 0, 0}, 1.4f);
    canvas.drawTextStyledEllipsized(title_, l.title, foreground, 13.0f, TextAlign::Left, std::max(500, fontWeight));
    canvas.drawTextEllipsized(message_, l.message, Color{190, 193, 203}, 12.0f, TextAlign::Left);

    auto paintAction = [&](Rect rect, const std::wstring& text, Action action) {
        if (text.empty()) {
            return;
        }
        const StyleBox actionStyle = resolvedActionStyle(action);
        paintStyleBox(canvas, rect, actionStyle);
        Rect textRect = rect;
        if (action == Action::Primary && primaryActionTrailingIcon_) {
            textRect.width = std::max(0.0f, rect.width - 18.0f);
            paintIcon(
                canvas,
                *primaryActionTrailingIcon_,
                Rect{rect.x + rect.width - 16.0f, rect.y + (rect.height - 14.0f) / 2.0f, 14.0f, 14.0f},
                actionStyle.foreground.value_or(foreground),
                Color{0, 0, 0, 0},
                1.2f);
        }
        canvas.drawTextStyledEllipsized(
            text,
            textRect,
            actionStyle.foreground.value_or(foreground),
            actionStyle.fontSize.value_or(12.0f),
            TextAlign::Center,
            actionStyle.fontWeight.value_or(500));
    };
    paintAction(l.primary, primaryAction_, Action::Primary);
    paintAction(l.secondary, secondaryAction_, Action::Secondary);
}

bool StatusStrip::onMouseMove(const MouseEvent& event) {
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

bool StatusStrip::onMouseDown(const MouseEvent& event) {
    const Action action = interactive() ? actionAt(event.position) : Action::None;
    if (action == Action::None) {
        return false;
    }
    pressedAction_ = action;
    setFocused(true);
    invalidate();
    return true;
}

bool StatusStrip::onMouseUp(const MouseEvent& event) {
    if (pressedAction_ == Action::None) {
        return false;
    }
    const Action pressed = pressedAction_;
    pressedAction_ = Action::None;
    invalidate();
    if (interactive() && actionAt(event.position) == pressed) {
        if (pressed == Action::Primary && onPrimaryAction_) {
            onPrimaryAction_();
        } else if (pressed == Action::Secondary && onSecondaryAction_) {
            onSecondaryAction_();
        }
    }
    return true;
}

bool StatusStrip::isFocusable() const {
    return !disabled();
}

bool StatusStrip::onFocusChanged(bool focused) {
    if (!Widget::onFocusChanged(focused)) {
        return false;
    }
    invalidate();
    return true;
}

StatusStrip::Layout StatusStrip::layout() const {
    const Rect rect = frame();
    const StyleBox box = resolvedStyle();
    const Insets padding = box.padding.value_or(Insets{12.0f, 18.0f, 12.0f, 18.0f});
    const Rect content = rect.inset(padding);
    const float actionHeight = std::min(28.0f, std::max(22.0f, content.height));
    const auto actionWidth = [](const std::wstring& text, bool link, bool hasIcon) {
        if (text.empty()) {
            return 0.0f;
        }
        const float chrome = link ? 8.0f : 24.0f;
        const float icon = hasIcon ? 18.0f : 0.0f;
        // CJK link actions need roughly one em per code point.  The previous
        // seven-pixel estimate was tuned for Latin labels and clipped actions
        // such as "查看历史监控" before the trailing chevron.
        return std::clamp(chrome + icon + static_cast<float>(text.size()) * (link ? 12.0f : 10.0f), 40.0f, 148.0f);
    };
    const float secondaryWidth = actionWidth(secondaryAction_, false, false);
    const bool primaryLink = primaryActionPresentation_ == StatusStripActionPresentation::Link;
    const float primaryWidth = actionWidth(primaryAction_, primaryLink, primaryActionTrailingIcon_.has_value());
    const float secondaryX = content.x + content.width - secondaryWidth;
    const float primaryX = secondaryX - (secondaryWidth > 0.0f && primaryWidth > 0.0f ? 8.0f : 0.0f) - primaryWidth;
    const float textRight = std::max(content.x, primaryX - 14.0f);

    Layout result;
    result.icon = Rect{content.x, content.y + 8.0f, 16.0f, 16.0f};
    result.title = Rect{content.x + 26.0f, content.y, std::max(0.0f, textRight - content.x - 26.0f), 20.0f};
    result.message = Rect{content.x + 26.0f, content.y + 22.0f, std::max(0.0f, textRight - content.x - 26.0f), 20.0f};
    result.primary = Rect{primaryX, content.y + 2.0f, primaryWidth, actionHeight};
    result.secondary = Rect{secondaryX, content.y + 2.0f, secondaryWidth, actionHeight};
    return result;
}

StyleBox StatusStrip::resolvedStyle() const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    StyleNode node = styleNode_;
    node.state = disabled() ? StyleStateDisabled : StyleStateNone;
    return sheet.resolve(node);
}

StyleBox StatusStrip::resolvedActionStyle(Action action) const {
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
        if (primaryActionPresentation_ == StatusStripActionPresentation::Link) {
            return (styleSheet_ ? *styleSheet_ : fallbackSheet())
                .resolve(childStyleNode({"status-strip-action", "primary", "link"}, state));
        }
        return (styleSheet_ ? *styleSheet_ : fallbackSheet())
            .resolve(childStyleNode({"status-strip-action", "primary"}, state));
    case Action::Secondary:
        return (styleSheet_ ? *styleSheet_ : fallbackSheet())
            .resolve(childStyleNode({"status-strip-action", "secondary"}, state));
    case Action::None:
        break;
    }
    return {};
}

StyleNode StatusStrip::childStyleNode(const std::vector<std::string>& classes, StylePseudoMask state) const {
    StyleNode node{"button", {}, state};
    for (const auto& klass : styleNode_.classes) {
        if (klass != styleNode_.tag) {
            node.classes.push_back(klass);
        }
    }
    node.classes.insert(node.classes.end(), classes.begin(), classes.end());
    return node;
}

StatusStrip::Action StatusStrip::actionAt(Point point) const {
    const Layout l = layout();
    if (!primaryAction_.empty() && l.primary.contains(point)) {
        return Action::Primary;
    }
    if (!secondaryAction_.empty() && l.secondary.contains(point)) {
        return Action::Secondary;
    }
    return Action::None;
}

bool StatusStrip::hasInteractionState() const {
    return hoveredAction_ != Action::None || pressedAction_ != Action::None;
}

void StatusStrip::resetInteractionState() {
    hoveredAction_ = Action::None;
    pressedAction_ = Action::None;
}

} // namespace oneui
