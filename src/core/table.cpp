#include "oneui/controls/table.h"

#include "oneui/style.h"

#include <algorithm>
#include <string>
#include <utility>

namespace oneui {
namespace {

void applyTableStyleOverride(TableStyle& style, const TableStyleOverride& override) {
    if (override.background) {
        style.background = *override.background;
    }
    if (override.border) {
        style.border = *override.border;
    }
    if (override.headerBackground) {
        style.headerBackground = *override.headerBackground;
    }
    if (override.headerForeground) {
        style.headerForeground = *override.headerForeground;
    }
    if (override.cellForeground) {
        style.cellForeground = *override.cellForeground;
    }
    if (override.gridLine) {
        style.gridLine = *override.gridLine;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.headerHeight) {
        style.headerHeight = *override.headerHeight;
    }
    if (override.cellPadding) {
        style.cellPadding = *override.cellPadding;
    }
}

} // namespace

Table::Table() {
    setPreferredSize(Size{320.0f, 140.0f});
}

void Table::setColumns(std::vector<TableColumn> columns) {
    columns_ = std::move(columns);
    invalidate();
}

void Table::setRows(std::vector<std::vector<std::wstring>> rows) {
    rows_ = std::move(rows);
    invalidate();
}

void Table::setStyleOverride(TableStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Table::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void Table::paint(Canvas& canvas) {
    const TableStyle style = resolvedStyle();
    const Rect rect = frame();
    canvas.fillRect(rect, style.background, style.radius);
    canvas.strokeRect(rect, style.border, style.radius, style.borderWidth);

    if (columns_.empty()) {
        return;
    }

    const float headerHeight = std::max(1.0f, style.headerHeight);
    const float rowHeight = rows_.empty() ? 0.0f : (rect.height - headerHeight) / static_cast<float>(rows_.size());
    canvas.fillRect(Rect{rect.x, rect.y, rect.width, headerHeight}, style.headerBackground, style.radius);
    canvas.drawLine(Point{rect.x, rect.y + headerHeight}, Point{rect.x + rect.width, rect.y + headerHeight}, style.gridLine, 1.0f);

    float fixedWidth = 0.0f;
    int flexibleCount = 0;
    for (const auto& column : columns_) {
        if (column.width > 0.0f) {
            fixedWidth += column.width;
        } else {
            ++flexibleCount;
        }
    }

    const float remainingWidth = std::max(0.0f, rect.width - fixedWidth);
    float x = rect.x;
    for (int columnIndex = 0; columnIndex < static_cast<int>(columns_.size()); ++columnIndex) {
        const float width = columnWidth(columnIndex, remainingWidth, flexibleCount);
        const Rect cell{x, rect.y, width, headerHeight};
        if (columnIndex > 0) {
            canvas.drawLine(Point{x, rect.y}, Point{x, rect.y + rect.height}, style.gridLine, 1.0f);
        }
        canvas.drawTextEllipsized(columns_[static_cast<std::size_t>(columnIndex)].header, cell.inset(style.cellPadding), style.headerForeground, theme().fontSm, TextAlign::Left);

        for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size()); ++rowIndex) {
            const float y = rect.y + headerHeight + rowHeight * static_cast<float>(rowIndex);
            const Rect rowCell{x, y, width, rowHeight};
            if (columnIndex == 0 && rowIndex > 0) {
                canvas.drawLine(Point{rect.x, y}, Point{rect.x + rect.width, y}, style.gridLine, 1.0f);
            }

            const auto& row = rows_[static_cast<std::size_t>(rowIndex)];
            const std::wstring empty;
            const std::wstring& text = columnIndex < static_cast<int>(row.size()) ? row[static_cast<std::size_t>(columnIndex)] : empty;
            canvas.drawTextEllipsized(text, rowCell.inset(style.cellPadding), style.cellForeground, theme().fontMd, TextAlign::Left);
        }

        x += width;
    }
}

AccessibilityInfo Table::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::Table;
    }
    info.value = std::to_wstring(rows_.size()) + L" rows, " + std::to_wstring(columns_.size()) + L" columns";
    return info;
}

TableStyle Table::resolvedStyle() const {
    const auto& t = theme();
    TableStyle style;
    style.background = disabled() ? t.surfaceMuted : t.surface;
    style.border = t.border;
    style.headerBackground = disabled() ? t.surfaceMuted : t.surfaceMuted;
    style.headerForeground = disabled() ? t.textSubtle : t.textMuted;
    style.cellForeground = disabled() ? t.textSubtle : t.text;
    style.gridLine = t.border;
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.headerHeight = 30.0f;
    style.cellPadding = Insets{0.0f, 10.0f};

    if (styleOverride_) {
        applyTableStyleOverride(style, *styleOverride_);
    }
    return style;
}

float Table::columnWidth(int index, float remainingWidth, int flexibleCount) const {
    const auto& column = columns_[static_cast<std::size_t>(index)];
    if (column.width > 0.0f) {
        return column.width;
    }

    if (flexibleCount <= 0) {
        return 0.0f;
    }

    return remainingWidth / static_cast<float>(flexibleCount);
}

} // namespace oneui
