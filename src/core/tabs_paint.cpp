#include "oneui/controls/tabs.h"

#include "oneui/icon.h"
#include "oneui/style.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {
void Tabs::paint(Canvas& canvas) {
    const Rect rect = frame();
    const int count = static_cast<int>(items_.size());
    const TabsStyle containerStyle = resolvedContainerStyle();

    canvas.fillRect(rect, containerStyle.background, containerStyle.radius);
    if (!documentMode_) canvas.strokeRect(rect, containerStyle.border, containerStyle.radius, containerStyle.borderWidth);

    if (!documentMode_ && focusVisible() && !disabled() && containerStyle.focusRing.visible) {
        const float offset = containerStyle.focusRing.offset;
        canvas.strokeRect(
            Rect{rect.x - offset, rect.y - offset, rect.width + offset * 2.0f, rect.height + offset * 2.0f},
            containerStyle.focusRing.color,
            containerStyle.focusRing.radius,
            containerStyle.focusRing.width);
    }

    if (count == 0) {
        return;
    }

    updateCompactMetrics(canvas);
    const int selected = std::clamp(selectedIndex(), 0, count - 1);
    ensureIndexVisible(editingIndex_ >= 0 ? editingIndex_ : selected);
    const int overflow = overflowItemCount();
    if (reportedOverflow_ != overflow) {
        reportedOverflow_ = overflow;
        if (onOverflowChanged_) onOverflowChanged_(overflow);
    }

    canvas.save();
    canvas.clipRect(rect);
    for (int i = 0; i < count; ++i) {
        if (documentMode_ && !itemFullyVisible(i)) continue;
        const TabsStyle itemStyle = resolvedItemStyle(i);
        const Rect itemCell = itemRect(i);
        Rect itemRect = documentMode_ ? itemCell : itemCell.inset(itemStyle.itemInset);
        if (documentMode_) itemRect.width = std::max(0.0f, itemRect.width - itemStyle.trailingInset);
        const bool active = i == selected;
        const bool outlined = documentMode_ && active && itemStyle.documentOutlineWidth > 0;
        if (outlined) {
            // Extend below the clip to round only the top corners. The outline
            // and indicator are independent, and neither changes hit geometry.
            canvas.save(); canvas.clipRect(itemRect);
            auto topRounded = itemRect;
            topRounded.height += itemStyle.itemRadius;
            canvas.fillRect(topRounded, itemStyle.itemBackground, itemStyle.itemRadius);
            const float half = itemStyle.documentOutlineWidth / 2;
            canvas.strokeRect(topRounded.inset(Insets{half}), itemStyle.selectedItemBorder,
                itemStyle.itemRadius, itemStyle.documentOutlineWidth);
            canvas.restore();
        } else if (itemStyle.itemBackground.a > 0) {
            // Unoutlined document tabs paint an inset pill (item padding +
            // item radius); hit geometry and text placement are unchanged.
            canvas.fillRect(documentMode_ ? itemRect.inset(itemStyle.itemInset) : itemRect,
                itemStyle.itemBackground, itemStyle.itemRadius);
        }
        if (documentMode_) {
            // Document tabs use one selection rule, not a rounded button border
            // painted over at the bottom. Selection and keyboard focus are distinct.
            if (active) {
                const float inset = std::min(itemStyle.indicatorInset, itemRect.width / 2);
                const float height = std::min(itemStyle.indicatorHeight, itemRect.height);
                if (height > 0 && itemRect.width > inset * 2) {
                    canvas.fillRect(Rect{itemRect.x + inset, itemRect.y + itemRect.height - height,
                        itemRect.width - inset * 2, height}, itemStyle.indicatorColor.value_or(itemStyle.selectedItemBorder));
                }
                if (focusVisible() && !disabled() && containerStyle.focusRing.visible) {
                    canvas.strokeRect(itemRect.inset(Insets{4.0f}), containerStyle.focusRing.color,
                        3.0f, containerStyle.focusRing.width);
                }
            } else if (i + 1 < count && i + 1 != selected && itemFullyVisible(i + 1)) {
                // Separate adjacent inactive tabs only; no orphan at the strip end.
                canvas.fillRect(Rect{itemRect.x + itemRect.width - 1.0f, itemRect.y + 10.0f,
                    1.0f, std::max(0.0f, itemRect.height - 20.0f)}, containerStyle.border);
            }
        } else if (active) {
            canvas.strokeRect(itemRect, itemStyle.selectedItemBorder, itemStyle.itemRadius, itemStyle.itemBorderWidth);
        }
        const bool showClose = closable_ && (active || i == hoveredIndex_ || i == hoveredCloseIndex_);
        const auto icon = static_cast<std::size_t>(i) < itemIcons_.size()
            ? itemIcons_[static_cast<std::size_t>(i)]
            : std::nullopt;
        const Rect textFrame = textRect(i);
        if (icon.has_value()) {
            const float iconSize = std::min(documentMode_ ? 15.0f : 16.0f, std::max(0.0f, itemRect.height - 8.0f));
            const Rect iconRect{
                itemRect.x + (documentMode_ ? 12.0f + (active ? 1.0f : 0.0f) : 8.0f),
                itemRect.y + (itemRect.height - iconSize) * 0.5f + (documentMode_ && active ? 0.5f : 0.0f),
                iconSize,
                iconSize};
            paintIcon(
                canvas,
                *icon,
                iconRect,
                itemStyle.itemForeground,
                Color{0, 0, 0, 0},
                1.35f);
        }
        if (i != editingIndex_) { canvas.drawTextStyledEllipsized(
            items_[static_cast<std::size_t>(i)],
            textFrame,
            itemStyle.itemForeground,
            itemStyle.fontSize,
            itemTextAlign(i),
            itemStyle.fontWeight); }
        if (documentMode_ && static_cast<std::size_t>(i) < itemStatusColors_.size() && itemStatusColors_[i].a > 0) {
            const float right = closable_ ? closeRect(i).x - 8.0f : itemRect.x + itemRect.width - 7.0f;
            const Color color = disabled() ? itemStyle.itemForeground : itemStatusColors_[i];
            canvas.fillEllipse(Rect{right - 6.0f, itemRect.y + (itemRect.height - 6.0f) * .5f + (active ? 0.5f : 0.0f), 6.0f, 6.0f}, color);
        }
        if (showClose) {
            const Rect iconRect = closeRect(i);
            if (i == hoveredCloseIndex_ || i == pressedCloseIndex_) {
                const Color hoverBackground = i == pressedCloseIndex_
                    ? itemStyle.selectedItemBorder
                    : itemStyle.itemBackground;
                if (hoverBackground.a > 0) {
                    canvas.fillRect(iconRect, hoverBackground, std::min(4.0f, itemStyle.itemRadius));
                }
            }
            paintIcon(
                canvas,
                documentMode_ ? IconSymbol::OutlineWorkbenchClose : IconSymbol::Close,
                iconRect.inset(Insets{documentMode_ ? 5.5f : 4.0f}),
                itemStyle.itemForeground,
                Color{0, 0, 0, 0},
                1.35f);
        }
    }
    if (editingIndex_ >= 0) {
        updateEditorFrame();
        editor_->paint(canvas);
    }
    if (dragging_ && dragSourceIndex_ >= 0 && dragTargetIndex_ >= 0
        && dragSourceIndex_ != dragTargetIndex_) {
        const Rect target = itemRect(dragTargetIndex_);
        const float x = dragSourceIndex_ < dragTargetIndex_ ? target.x + target.width : target.x;
        const float inset = 4.0f;
        canvas.drawLine(
            Point{x, rect.y + inset},
            Point{x, rect.y + rect.height - inset},
            theme().primary,
            2.0f);
    }
    canvas.restore();
}

} // namespace oneui
