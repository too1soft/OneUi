#include "oneui/controls/dialog.h"

#include "oneui/canvas.h"
#include "oneui/style.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

constexpr float kIconBadgeSize = 38.0f;
constexpr float kHeaderGap = 10.0f;
constexpr float kCloseSize = 28.0f;
constexpr float kCloseInset = 12.0f;
constexpr float kSlotGap = 12.0f;

// 未接 StyleSheet 时的默认外观：浅色中性（docs/03-style.md）。
StyleSheet defaultDialogSheet() {
    StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        dialog {
            background: #ffffff;
            border-color: #e2e5ea;
            border-width: 1px;
            border-radius: 14px;
            color: #202124;
            padding: 16px 20px;
            font-size: 17px;
            font-weight: 600;
            box-shadow: 0px 18px 42px 0px #0f172a30;
        }
        icon-badge {
            background: #f1f5f9;
            border-color: #e2e5ea;
            border-width: 1px;
            border-radius: 10px;
            color: #616772;
        }
        .dialog-close { background: #00000000; border-color: #00000000; color: #98a2b3; }
        .dialog-close:hover { background: #eef0f3; color: #202124; }
        .dialog-close:active { background: #e2e5ea; color: #202124; }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultDialogSheet();
    return sheet;
}

} // namespace

Dialog::Dialog(std::wstring title, std::wstring subtitle)
    : title_(std::move(title))
    , subtitle_(std::move(subtitle)) {
    setPreferredSize(Size{520.0f, 360.0f});
    setAccessibleRole(AccessibilityRole::Window);
    setAccessibleName(title_);
}

void Dialog::setTitle(std::wstring title) {
    title_ = std::move(title);
    setAccessibleName(title_);
    invalidate();
}

void Dialog::setSubtitle(std::wstring subtitle) {
    subtitle_ = std::move(subtitle);
    invalidate();
}

void Dialog::setIconSymbol(IconSymbol symbol) {
    icon_ = symbol;
    invalidate();
}

void Dialog::clearIconSymbol() {
    icon_.reset();
    invalidate();
}

void Dialog::setCloseVisible(bool visible) {
    closeVisible_ = visible;
    invalidate();
}

void Dialog::setOnClose(std::function<void()> callback) {
    onClose_ = std::move(callback);
}

void Dialog::setContent(std::shared_ptr<Widget> content) {
    content_ = std::move(content);
    rebuildChildren();
}

void Dialog::setActions(std::shared_ptr<Widget> actions) {
    actions_ = std::move(actions);
    rebuildChildren();
}

void Dialog::rebuildChildren() {
    clearChildren();
    if (content_) {
        add(content_);
    }
    if (actions_) {
        add(actions_);
    }
    invalidate();
}

void Dialog::setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node) {
    styleSheet_ = std::move(sheet);
    styleNode_ = std::move(node);
    invalidate();
}

StyleBox Dialog::resolvedSurface() const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    StyleNode node = styleNode_;
    node.state = disabled() ? StyleStateDisabled : StyleStateNone;
    return sheet.resolve(node);
}

Dialog::HeaderLayout Dialog::headerLayout() const {
    const StyleBox surface = resolvedSurface();
    const Rect content = frame().inset(surface.padding.value_or(Insets{16.0f, 20.0f}));

    HeaderLayout layout;
    layout.height = 48.0f;
    float textLeft = content.x;
    if (icon_) {
        layout.icon = Rect{content.x, content.y + (layout.height - kIconBadgeSize) / 2.0f, kIconBadgeSize, kIconBadgeSize};
        textLeft += kIconBadgeSize + kHeaderGap;
    }
    // 关闭键贴弹窗右上角（不随头部行垂直居中），符合桌面弹窗惯例。
    const Rect bounds = frame();
    layout.close = closeVisible_
        ? Rect{bounds.x + bounds.width - kCloseInset - kCloseSize, bounds.y + kCloseInset, kCloseSize, kCloseSize}
        : Rect{};
    const float textRight = closeVisible_
        ? std::min(content.x + content.width, layout.close.x - 8.0f)
        : content.x + content.width;
    const float textWidth = std::max(0.0f, textRight - textLeft);
    if (subtitle_.empty()) {
        layout.title = Rect{textLeft, content.y, textWidth, layout.height};
    } else {
        layout.title = Rect{textLeft, content.y + 2.0f, textWidth, 25.0f};
        layout.subtitle = Rect{textLeft, content.y + 27.0f, textWidth, 19.0f};
    }
    return layout;
}

Rect Dialog::contentArea() const {
    const StyleBox surface = resolvedSurface();
    return frame().inset(surface.padding.value_or(Insets{16.0f, 20.0f}));
}

void Dialog::layoutChildren() {
    const Rect content = contentArea();
    const float headerHeight = headerLayout().height;
    float actionsHeight = 0.0f;
    if (actions_ && actions_->visible()) {
        actionsHeight = actions_->preferredSize().height;
        if (actionsHeight <= 0.0f) {
            actionsHeight = 38.0f;
        }
        actions_->setFrame(Rect{
            content.x,
            content.y + content.height - actionsHeight,
            content.width,
            actionsHeight});
    }
    if (content_ && content_->visible()) {
        const float top = content.y + headerHeight + kSlotGap;
        const float bottom = content.y + content.height - actionsHeight - (actionsHeight > 0.0f ? kSlotGap : 0.0f);
        content_->setFrame(Rect{content.x, top, content.width, std::max(0.0f, bottom - top)});
    }
}

void Dialog::paint(Canvas& canvas) {
    const StyleBox surface = resolvedSurface();
    paintStyleBox(canvas, frame(), surface);

    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    const HeaderLayout header = headerLayout();

    if (icon_) {
        const StyleBox badge = sheet.resolve(StyleNode{"icon-badge", {"dialog-icon"}, StyleStateNone});
        paintStyleBox(canvas, header.icon, badge);
        const float inset = kIconBadgeSize * 0.26f;
        paintIcon(
            canvas,
            *icon_,
            header.icon.inset(Insets{inset}),
            badge.foreground.value_or(Color{97, 103, 114}),
            Color{0, 0, 0, 0},
            1.6f);
    }

    const Color titleColor = surface.foreground.value_or(Color{32, 33, 36});
    canvas.drawTextStyled(
        title_,
        header.title,
        titleColor,
        surface.fontSize.value_or(17.0f),
        TextAlign::Left,
        surface.fontWeight.value_or(600));
    if (!subtitle_.empty()) {
        canvas.drawText(subtitle_, header.subtitle, Color{90, 100, 114}, 12.0f, TextAlign::Left);
    }

    if (closeVisible_) {
        StylePseudoMask state = StyleStateNone;
        if (closePressed_) {
            state |= StyleStateActive;
        } else if (closeHovered_) {
            state |= StyleStateHover;
        }
        const StyleBox close = sheet.resolve(StyleNode{"button", {"dialog-close"}, state});
        paintStyleBox(canvas, header.close, close);
        const Rect closeIcon = header.close.inset(Insets{7.0f});
        paintIcon(canvas, IconSymbol::Close, closeIcon, close.foreground.value_or(Color{152, 162, 179}), Color{0, 0, 0, 0}, 1.5f);
    }

    View::paint(canvas);
}

bool Dialog::onMouseMove(const MouseEvent& event) {
    bool handled = View::onMouseMove(event);
    const bool hovered = interactive() && closeVisible_ && headerLayout().close.contains(event.position);
    if (hovered != closeHovered_) {
        closeHovered_ = hovered;
        if (!closeHovered_) {
            closePressed_ = false;
        }
        invalidate();
        handled = true;
    }
    return handled;
}

bool Dialog::onMouseDown(const MouseEvent& event) {
    if (interactive() && closeVisible_ && headerLayout().close.contains(event.position)) {
        closePressed_ = true;
        invalidate();
        return true;
    }
    return View::onMouseDown(event);
}

bool Dialog::onMouseUp(const MouseEvent& event) {
    if (closePressed_) {
        closePressed_ = false;
        invalidate();
        if (interactive() && headerLayout().close.contains(event.position) && onClose_) {
            onClose_();
        }
        return true;
    }
    return View::onMouseUp(event);
}

CursorKind Dialog::cursor(Point point) const {
    if (interactive() && closeVisible_ && headerLayout().close.contains(point)) {
        return CursorKind::Pointer;
    }
    return View::cursor(point);
}

bool Dialog::hasInteractionState() const {
    return closeHovered_ || closePressed_ || View::hasInteractionState();
}

void Dialog::resetInteractionState() {
    closeHovered_ = false;
    closePressed_ = false;
    View::resetInteractionState();
}

} // namespace oneui
