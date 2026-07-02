#include "oneui/controls/icon_badge.h"

#include "oneui/canvas.h"
#include "oneui/style.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

// 未接 StyleSheet 时的默认外观：浅色中性底 + 中性图标（docs/03-style.md）。
StyleBox defaultIconBadgeStyle() {
    const auto& t = theme();
    StyleBox box;
    box.background.color = Color{241, 245, 249};
    box.borderColor = t.border;
    box.borderWidth = 1.0f;
    box.radius = 10.0f;
    box.foreground = t.textMuted;
    return box;
}

} // namespace

IconBadge::IconBadge(IconSymbol symbol) : symbol_(symbol) {
    setPreferredSize(Size{36.0f, 36.0f});
    setAccessibleRole(AccessibilityRole::Custom);
}

void IconBadge::setSymbol(IconSymbol symbol) {
    if (symbol_ == symbol) {
        return;
    }
    symbol_ = symbol;
    invalidate();
}

IconSymbol IconBadge::symbol() const {
    return symbol_;
}

void IconBadge::setAccent(Color accent) {
    accent_ = accent;
    invalidate();
}

void IconBadge::setStrokeWidth(float width) {
    strokeWidth_ = std::max(1.0f, width);
    invalidate();
}

void IconBadge::setStyleBox(StyleBox box) {
    styleBox_ = std::move(box);
    invalidate();
}

void IconBadge::paint(Canvas& canvas) {
    const StyleBox box = mergeStyleBox(defaultIconBadgeStyle(), styleBox_);
    const Rect rect = frame();
    paintStyleBox(canvas, rect, box);

    // 图标取内容区（扣除 padding；无 padding 时按底面尺寸留 26% 呼吸边距）的内切正方形。
    Rect content = rect;
    if (box.padding) {
        content = rect.inset(*box.padding);
    } else {
        const float breathing = std::min(rect.width, rect.height) * 0.26f;
        content = rect.inset(Insets{breathing});
    }
    const float side = std::max(0.0f, std::min(content.width, content.height));
    const Rect iconRect{
        content.x + (content.width - side) / 2.0f,
        content.y + (content.height - side) / 2.0f,
        side,
        side};

    Color color = box.foreground.value_or(theme().textMuted);
    Color accent = accent_;
    if (disabled()) {
        color = theme().disabledForeground;
        accent = Color{0, 0, 0, 0};
    }
    paintIcon(canvas, symbol_, iconRect, color, accent, strokeWidth_);
}

} // namespace oneui
