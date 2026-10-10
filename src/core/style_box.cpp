#include "style_css_internal.h"
#include "internal/unicode.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace oneui {
using namespace style_detail;
StyleBox mergeStyleBox(StyleBox base, const StyleBox& overlay) {
    if(overlay.containerName)base.containerName=overlay.containerName;
    if(overlay.sizeContainment)base.sizeContainment=overlay.sizeContainment;
    if(overlay.display)base.display=overlay.display;
    if(overlay.flexDirection)base.flexDirection=overlay.flexDirection;
    if(overlay.flexWrap)base.flexWrap=overlay.flexWrap;
    if(overlay.flexBasis)base.flexBasis=overlay.flexBasis;
    if(overlay.flexGrow)base.flexGrow=overlay.flexGrow;
    if(overlay.flexShrink)base.flexShrink=overlay.flexShrink;
    if(overlay.minWidth)base.minWidth=overlay.minWidth;
    if(overlay.minHeight)base.minHeight=overlay.minHeight;
    if (overlay.backgroundOpacity) base.backgroundOpacity = overlay.backgroundOpacity;
    if (overlay.contentBackgroundOpacity) base.contentBackgroundOpacity = overlay.contentBackgroundOpacity;
    if (overlay.background.color || overlay.background.gradient || overlay.background.gradientStart) {
        base.background=overlay.background;
    }
    if (overlay.content.backgroundColor) {
        base.content.backgroundColor = overlay.content.backgroundColor;
    }
    if (overlay.content.radius) {
        base.content.radius = overlay.content.radius;
    }
    if (overlay.content.inset) {
        base.content.inset = overlay.content.inset;
    }
    if (overlay.foreground) {
        base.foreground = overlay.foreground;
    }
    if (overlay.placeholderColor) {
        base.placeholderColor = overlay.placeholderColor;
    }
    if (overlay.caretColor) {
        base.caretColor = overlay.caretColor;
    }
    if (overlay.selectionColor) {
        base.selectionColor = overlay.selectionColor;
    }
    if (overlay.borderColor) {
        base.borderColor = overlay.borderColor;
    }
    if (overlay.borderWidth) {
        base.borderWidth = overlay.borderWidth;
    }
    if (overlay.outlineColor) {
        base.outlineColor = overlay.outlineColor;
    }
    if (overlay.outlineWidth) {
        base.outlineWidth = overlay.outlineWidth;
    }
    if (overlay.outlineOffset) {
        base.outlineOffset = overlay.outlineOffset;
    }
    if (overlay.opacity) {
        base.opacity = overlay.opacity;
    }
    if (overlay.radius) {
        base.radius = overlay.radius;
    }
    if (overlay.padding) {
        base.padding = overlay.padding;
    }
    if (overlay.gap) {
        base.gap = overlay.gap;
    }
    if (overlay.width) {
        base.width = overlay.width;
    }
    if (overlay.height) {
        base.height = overlay.height;
    }
    if (overlay.gridMinColumnWidth) {
        base.gridMinColumnWidth = overlay.gridMinColumnWidth;
    }
    if (overlay.fontSize) {
        base.fontSize = overlay.fontSize;
    }
    if (overlay.fontFamily) {
        base.fontFamily = overlay.fontFamily;
    }
    if (overlay.fontWeight) {
        base.fontWeight = overlay.fontWeight;
    }
    if (overlay.detailFontSize) {
        base.detailFontSize = overlay.detailFontSize;
    }
    if (overlay.detailFontWeight) {
        base.detailFontWeight = overlay.detailFontWeight;
    }
    if (overlay.iconSize) base.iconSize = overlay.iconSize;
    if (overlay.letterSpacing) base.letterSpacing = overlay.letterSpacing;
    if (overlay.tabularNumbers) base.tabularNumbers = overlay.tabularNumbers;
    if (overlay.tabIndicatorColor) base.tabIndicatorColor = overlay.tabIndicatorColor;
    if (overlay.tabIndicatorInset) base.tabIndicatorInset = overlay.tabIndicatorInset;
    if (overlay.tabIndicatorHeight) base.tabIndicatorHeight = overlay.tabIndicatorHeight;
    if (overlay.tabOutlineWidth) base.tabOutlineWidth = overlay.tabOutlineWidth;
    if (overlay.tabTrailingInset) base.tabTrailingInset = overlay.tabTrailingInset;
    if (overlay.textInset) {
        base.textInset = overlay.textInset;
    }
    if (overlay.titleOffsetY) {
        base.titleOffsetY = overlay.titleOffsetY;
    }
    if (overlay.detailOffsetY) {
        base.detailOffsetY = overlay.detailOffsetY;
    }
    if (overlay.scrollbarColor) {
        base.scrollbarColor = overlay.scrollbarColor;
    }
    if (overlay.checkBackground) base.checkBackground = overlay.checkBackground;
    if (overlay.checkBorder) base.checkBorder = overlay.checkBorder;
    if (overlay.checkRadius) base.checkRadius = overlay.checkRadius;
    if (overlay.scrollbarWidth) {
        base.scrollbarWidth = overlay.scrollbarWidth;
    }
    if (overlay.transitionDurationMs) {
        base.transitionDurationMs = overlay.transitionDurationMs;
    }
    if (overlay.transitionEasing) {
        base.transitionEasing = overlay.transitionEasing;
    }
    if (overlay.shadowsSpecified || !overlay.shadows.empty()) {
        base.shadowsSpecified = true;
        base.shadows = overlay.shadows;
    }
    return base;
}

StyleBox resolveBackgroundOpacity(StyleBox box) {
    auto multiply = [](std::optional<Color>& color, std::optional<float> opacity) {
        if (color && opacity) color = applyOpacity(*color, opacity);
    };
    multiply(box.background.color, box.backgroundOpacity);
    multiply(box.background.gradientStart, box.backgroundOpacity);
    multiply(box.background.gradientEnd, box.backgroundOpacity);
    multiply(box.checkBackground, box.backgroundOpacity);
    if (box.background.gradient && box.backgroundOpacity) {
        for (auto& stop : box.background.gradient->stops)
            stop.color = applyOpacity(stop.color, box.backgroundOpacity);
    }
    multiply(box.content.backgroundColor, box.contentBackgroundOpacity);
    box.backgroundOpacity.reset();
    box.contentBackgroundOpacity.reset();
    return box;
}

Rect styleContentRect(Rect rect, const StyleBox& box) {
    return box.content.inset ? rect.inset(*box.content.inset) : rect;
}

Rect stylePaintBounds(Rect rect,const StyleBox& box) {
    float left=0,right=0,top=0,bottom=0;
    for(const auto& s:box.shadows)if(!s.inset && s.color.a) {
        const float pad=std::max(0.0f,s.blurRadius*3+s.spreadRadius+4);
        left=std::max(left,pad-s.offset.x);right=std::max(right,pad+s.offset.x);
        top=std::max(top,pad-s.offset.y);bottom=std::max(bottom,pad+s.offset.y);
    }
    return {rect.x-left,rect.y-top,rect.width+left+right,rect.height+top+bottom};
}

void paintStyleBox(Canvas& canvas, Rect rect, const StyleBox& input) {
    if (input.backgroundOpacity || input.contentBackgroundOpacity) {
        paintStyleBox(canvas, rect, resolveBackgroundOpacity(input));
        return;
    }
    const auto& box = input;
    const float radius = box.radius.value_or(0.0f);
    for (const auto& shadow : box.shadows) {
        if (!shadow.inset) {
            StyleShadow paintedShadow = shadow;
            paintedShadow.color = applyOpacity(paintedShadow.color, box.opacity);
            const auto drawShadow = [&] {
                canvas.drawBoxShadow(
                    Rect{rect.x + paintedShadow.offset.x, rect.y + paintedShadow.offset.y, rect.width, rect.height},
                    BoxShadow{paintedShadow.color, Point{0.0f, 0.0f}, paintedShadow.blurRadius, paintedShadow.spreadRadius}, radius);
            };
            // Do not blend pixels that the following opaque fill overwrites.
            // Keep a conservative rounded-corner/antialias fringe; transparent
            // surfaces still receive the complete shadow, including its center.
            const bool opaque = box.background.color && applyOpacity(*box.background.color, box.opacity).a == 255;
            const float inset = std::max(1.0f, radius + 1.0f);
            if (opaque && rect.width > inset * 2 && rect.height > inset * 2) {
                canvas.save();
                canvas.clipOutRect(rect.inset(Insets{inset}));
                drawShadow();
                canvas.restore();
            } else {
                drawShadow();
            }
        }
    }

    if (box.background.color) {
        canvas.fillRect(rect, applyOpacity(*box.background.color, box.opacity), radius);
    } else if (box.background.gradient) {
        auto gradient=*box.background.gradient;
        for(auto& stop:gradient.stops)stop.color=applyOpacity(stop.color,box.opacity);
        canvas.fillGradient(rect,gradient,radius);
    } else if (box.background.gradientStart && box.background.gradientEnd) {
        if (box.background.radialCenter) {
            canvas.fillRadialGradient(
                rect,
                applyOpacity(*box.background.gradientStart, box.opacity),
                applyOpacity(*box.background.gradientEnd, box.opacity),
                *box.background.radialCenter,
                box.background.radialRadius.value_or(0.75f),
                radius);
        } else {
            canvas.fillLinearGradient(
                rect,
                applyOpacity(*box.background.gradientStart, box.opacity),
                applyOpacity(*box.background.gradientEnd, box.opacity),
                box.background.gradientAngleDegrees.value_or(180.0f),
                radius);
        }
    } else if (box.background.gradientStart) {
        canvas.fillRect(rect, applyOpacity(*box.background.gradientStart, box.opacity), radius);
    }
    if (box.content.backgroundColor) {
        canvas.fillRect(styleContentRect(rect, box), applyOpacity(*box.content.backgroundColor, box.opacity), box.content.radius.value_or(radius));
    }
    // border-width: 0 语义为“无边框”：宽度 0 传给 Skia 会画 1px 发丝线（hairline），必须显式跳过。
    if (box.borderColor && box.borderWidth.value_or(1.0f) > 0.0f) {
        // CSS borders occupy the inside of the border box. Centering a stroke
        // on its edge clips half of a 1px border and spreads the rest over two
        // pixels, making translucent workspace outlines visibly too faint.
        const float width = std::min(box.borderWidth.value_or(1.0f), std::min(rect.width, rect.height));
        if (width > 0.0f) {
            const float inset = width * 0.5f;
            canvas.strokeRect(
                Rect{rect.x + inset, rect.y + inset, rect.width - width, rect.height - width},
                applyOpacity(*box.borderColor, box.opacity), std::max(0.0f, radius - inset), width);
        }
    }
    for (const auto& shadow : box.shadows) {
        if (shadow.inset) {
            canvas.drawInsetShadow(rect,BoxShadow{applyOpacity(shadow.color,box.opacity),shadow.offset,shadow.blurRadius,shadow.spreadRadius},radius);
        }
    }
    if (box.outlineColor && box.outlineWidth) {
        const float width = std::max(1.0f, *box.outlineWidth);
        const float offset = box.outlineOffset.value_or(0.0f) + width * 0.5f;
        canvas.strokeRect(
            Rect{rect.x - offset, rect.y - offset, rect.width + offset * 2.0f, rect.height + offset * 2.0f},
            applyOpacity(*box.outlineColor, box.opacity),
            radius + offset,
            width);
    }
}
}
