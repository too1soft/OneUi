#include "oneui/controls/badge.h"

#include "oneui/style.h"

#include <utility>

namespace oneui {
namespace {

struct BadgeColors {
    Color background;
    Color foreground;
    Color border;
};

BadgeStyle styleFor(BadgeVariant variant) {
    const auto& t = theme();
    switch (variant) {
    case BadgeVariant::Success:
        return BadgeStyle{t.successSoft, t.success, Color{187, 247, 208}, 1.0f, 12.0f, Insets{0.0f, 8.0f}, t.fontSm};
    case BadgeVariant::Warning:
        return BadgeStyle{t.warningSoft, t.warning, Color{253, 230, 138}, 1.0f, 12.0f, Insets{0.0f, 8.0f}, t.fontSm};
    case BadgeVariant::Danger:
        return BadgeStyle{t.errorSoft, Color{185, 28, 28}, Color{252, 165, 165}, 1.0f, 12.0f, Insets{0.0f, 8.0f}, t.fontSm};
    case BadgeVariant::Accent:
        return BadgeStyle{t.primarySoft, t.primary, Color{191, 219, 254}, 1.0f, 12.0f, Insets{0.0f, 8.0f}, t.fontSm};
    case BadgeVariant::Neutral:
    default:
        return BadgeStyle{Color{241, 245, 249}, t.textMuted, t.border, 1.0f, 12.0f, Insets{0.0f, 8.0f}, t.fontSm};
    }
}

void applyBadgeStyleOverride(BadgeStyle& style, const BadgeStyleOverride& overrideStyle) {
    if (overrideStyle.background) {
        style.background = *overrideStyle.background;
    }
    if (overrideStyle.foreground) {
        style.foreground = *overrideStyle.foreground;
    }
    if (overrideStyle.border) {
        style.border = *overrideStyle.border;
    }
    if (overrideStyle.borderWidth) {
        style.borderWidth = *overrideStyle.borderWidth;
    }
    if (overrideStyle.radius) {
        style.radius = *overrideStyle.radius;
    }
    if (overrideStyle.padding) {
        style.padding = *overrideStyle.padding;
    }
    if (overrideStyle.fontSize) {
        style.fontSize = *overrideStyle.fontSize;
    }
}

} // namespace

Badge::Badge(std::wstring text, BadgeVariant variant) : text_(std::move(text)), variant_(variant) {
    setPreferredSize(Size{72.0f, 24.0f});
}

void Badge::setText(std::wstring text) {
    textBinding_.set(std::move(text), text_);
    invalidate();
}

const std::wstring& Badge::text() const {
    return textBinding_.get(text_);
}

void Badge::bindText(State<std::wstring>& state) {
    textBinding_ = Binding<std::wstring>(state, [this] {
        invalidate();
    });
    invalidate();
}

void Badge::setVariant(BadgeVariant variant) {
    variant_ = variant;
    invalidate();
}

void Badge::setStyleOverride(BadgeStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Badge::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void Badge::paint(Canvas& canvas) {
    const auto style = resolvedStyle();
    const Rect rect = frame();
    canvas.fillRect(rect, style.background, style.radius);
    canvas.strokeRect(rect, style.border, style.radius, style.borderWidth);
    canvas.drawText(text(), rect.inset(style.padding), style.foreground, style.fontSize);
}

BadgeStyle Badge::resolvedStyle() const {
    auto style = styleFor(variant_);
    if (disabled()) {
        style.background = theme().disabledBackground;
        style.foreground = theme().disabledForeground;
        style.border = theme().disabledBorder;
    }
    if (styleOverride_) {
        applyBadgeStyleOverride(style, *styleOverride_);
    }
    return style;
}

} // namespace oneui
