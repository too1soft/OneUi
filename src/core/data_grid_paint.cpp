#include "data_grid_surface.h"

#include "oneui/icon.h"

#include <algorithm>
#include <cmath>

namespace oneui {
namespace {

bool has(const std::string& classes, const char* token) {
    const std::string word(token);
    std::size_t at = 0;
    while ((at = classes.find(word, at)) != std::string::npos) {
        const bool start = at == 0 || classes[at - 1] == ' ';
        const bool end = at + word.size() == classes.size() || classes[at + word.size()] == ' ';
        if (start && end) return true;
        at += word.size();
    }
    return false;
}

Color withAlpha(Color color, std::uint8_t alpha) { return Color{color.r, color.g, color.b, alpha}; }

} // namespace

TableStyle DataGridSurface::style() const {
    const auto& t = theme();
    TableStyle style;
    style.background = t.surface;
    style.border = t.border;
    style.headerBackground = t.surfaceMuted;
    style.headerForeground = t.textMuted;
    style.cellForeground = disabled() ? t.textSubtle : t.text;
    style.detailForeground = t.textSubtle;
    style.gridLine = withAlpha(t.border, 160);
    style.rowHovered = t.surfaceMuted;
    style.rowPressed = t.pressedBackground;
    style.rowSelected = t.selectedBackground;
    style.scrollbarColor = withAlpha(t.borderStrong, 170);
    style.fontSize = 13.0f;
    style.headerFontSize = 12.0f;
    style.cellPadding = Insets{0.0f, 8.0f};
    return style;
}

void DataGridSurface::paint(Canvas& canvas) {
    const TableStyle s = style();
    const Rect f = frame();
    canvas.fillRect(f, s.background);
    if (autofitColumn_ >= 0 && autofitColumn_ < static_cast<int>(columns.size())) {
        // Fit to the header and the rows on screen; measured here because only paint has a canvas.
        auto& column = columns[autofitColumn_];
        float width = canvas.measureTextWidth(column.title, s.headerFontSize, 600) + 30.0f;
        const auto first = firstVisible();
        for (auto index = first; index < first + visibleCount(); ++index)
            if (const auto* r = row(index); r && autofitColumn_ < static_cast<int>(r->cells.size()))
                width = std::max(width, canvas.measureTextWidth(r->cells[autofitColumn_].text, s.fontSize) + 20.0f);
        column.width = std::clamp(width, 24.0f, 600.0f);
        const int fitted = autofitColumn_;
        autofitColumn_ = -1;
        if (onResize) onResize(fitted, column.width);
        owner_.layoutChildren();
    }
    paintRows(canvas, s);
    paintHeader(canvas, s);
    paintScrollbars(canvas, s);
    if (focusVisible() || focused())
        canvas.strokeRect(f, focused() ? withAlpha(theme().focusOutline, 140) : s.border, 0.0f, 1.0f);
    else
        canvas.strokeRect(f, s.border, 0.0f, 1.0f);
}

void DataGridSurface::paintHeader(Canvas& canvas, const TableStyle& s) {
    const Rect f = frame();
    const Rect header{f.x, f.y, f.width, headerHeight};
    canvas.fillRect(header, s.headerBackground);
    canvas.drawLine({f.x, f.y + headerHeight - 0.5f}, {f.x + f.width, f.y + headerHeight - 0.5f}, s.gridLine);
    const float gutter = gutterWidth();
    const float frozenRight = f.x + gutter + frozenWidth();
    const auto paintColumn = [&](int index) {
        const auto& column = columns[index];
        const float x = columnScreenX(index);
        const Rect cell{x, f.y, column.width, headerHeight};
        if (cell.x + cell.width < f.x + gutter || cell.x > f.x + f.width) return;
        const bool selected = selection.kind == DataGridSelectKind::Columns && selection.contains(0, index);
        if (selected) canvas.fillRect(cell, s.rowSelected);
        canvas.save();
        canvas.clipRect(cell);
        Rect label = cell.inset(Insets{0.0f, 8.0f});
        label.width = std::max(0.0f, label.width - 16.0f);
        float leading = label.x;
        if (column.key) {
            paintIcon(canvas, IconSymbol::Key, Rect{leading, f.y + (headerHeight - 12.0f) * 0.5f, 12.0f, 12.0f}, theme().warning);
            leading += 16.0f;
        }
        const float titleWidth = canvas.measureTextWidth(column.title, s.headerFontSize, 600);
        const Rect titleRect{leading, cell.y, std::max(0.0f, label.x + label.width - leading), cell.height};
        canvas.drawTextStyledEllipsized(column.title, titleRect, theme().text, s.headerFontSize, TextAlign::Left, 600);
        if (!column.detail.empty() && titleWidth + 12.0f < titleRect.width) {
            const Rect detail{leading + titleWidth + 6.0f, cell.y, titleRect.width - titleWidth - 6.0f, cell.height};
            canvas.drawTextStyledEllipsized(column.detail, detail, s.detailForeground, 11.0f, TextAlign::Left, 400);
        }
        if (column.sort != 0)
            paintIcon(canvas, column.sort == 1 ? IconSymbol::ChevronUp : IconSymbol::ChevronDown,
                      Rect{cell.x + cell.width - 16.0f, f.y + (headerHeight - 10.0f) * 0.5f, 10.0f, 10.0f}, s.headerForeground);
        canvas.restore();
        canvas.drawLine({x + column.width - 0.5f, f.y + 6.0f}, {x + column.width - 0.5f, f.y + headerHeight - 6.0f}, s.gridLine);
    };
    canvas.save();
    canvas.clipRect(Rect{frozenRight, f.y, std::max(0.0f, f.x + f.width - frozenRight), headerHeight});
    for (int index = frozenColumns; index < static_cast<int>(columns.size()); ++index) paintColumn(index);
    canvas.restore();
    for (int index = 0; index < std::min<int>(frozenColumns, static_cast<int>(columns.size())); ++index) paintColumn(index);
    if (gutter > 0.0f) {
        canvas.fillRect(Rect{f.x, f.y, gutter, headerHeight}, s.headerBackground);
        canvas.drawLine({f.x + gutter - 0.5f, f.y}, {f.x + gutter - 0.5f, f.y + headerHeight}, s.gridLine);
    }
}

void DataGridSurface::paintRows(Canvas& canvas, const TableStyle& s) {
    const Rect f = frame();
    const Rect body = bodyRect();
    if (body.height <= 0.0f || rowHeight <= 0.0f) return;
    const float gutter = gutterWidth();
    const float frozenRight = f.x + gutter + frozenWidth();
    const std::int64_t first = firstVisible();
    const std::int64_t last = std::min(rowCount, first + visibleCount());
    const auto& t = theme();
    canvas.save();
    canvas.clipRect(body);
    for (std::int64_t index = first; index < last; ++index) {
        const float y = static_cast<float>(body.y + index * static_cast<double>(rowHeight) - scroll_);
        const Rect line{f.x, y, body.width, rowHeight};
        const DataGridRow* r = row(index);
        if (r && has(r->classes, "deleted")) canvas.fillRect(line, withAlpha(t.formField.errorColor, 28));
        else if (r && has(r->classes, "inserted")) canvas.fillRect(line, withAlpha(t.success, 26));
        const auto paintCell = [&](int column) {
            const float x = columnScreenX(column);
            const Rect cell{x, y, columns[column].width, rowHeight};
            if (cell.x + cell.width < f.x + gutter || cell.x > body.x + body.width) return;
            if (selection.contains(index, column)) canvas.fillRect(cell, s.rowSelected);
            this->paintCell(canvas, s, r && column < static_cast<int>(r->cells.size()) ? &r->cells[column] : nullptr, cell, column);
            if (!r) canvas.fillRect(Rect{cell.x + 8.0f, y + rowHeight * 0.5f - 3.0f, std::min(48.0f, cell.width - 16.0f), 6.0f},
                                    withAlpha(t.border, 120), 3.0f);
            canvas.drawLine({x + cell.width - 0.5f, y}, {x + cell.width - 0.5f, y + rowHeight}, s.gridLine);
            if (!selection.empty() && selection.focusRow == index && selection.focusColumn == column)
                canvas.strokeRect(cell.inset(Insets{1.0f}), t.focusOutline, 0.0f, 2.0f);
        };
        canvas.save();
        canvas.clipRect(Rect{frozenRight, y, std::max(0.0f, body.x + body.width - frozenRight), rowHeight});
        for (int column = frozenColumns; column < static_cast<int>(columns.size()); ++column) paintCell(column);
        canvas.restore();
        for (int column = 0; column < std::min<int>(frozenColumns, static_cast<int>(columns.size())); ++column) paintCell(column);
        canvas.drawLine({f.x, y + rowHeight - 0.5f}, {body.x + body.width, y + rowHeight - 0.5f}, s.gridLine);
        if (gutter > 0.0f) {
            const bool rowSelected = selection.kind == DataGridSelectKind::Rows && selection.contains(index, 0);
            canvas.fillRect(Rect{f.x, y, gutter, rowHeight}, rowSelected ? s.rowSelected : s.headerBackground);
            const std::wstring number = r && !r->number.empty() ? r->number : std::to_wstring(index + 1);
            canvas.drawTextStyledEllipsized(number, Rect{f.x, y, gutter - 8.0f, rowHeight}, s.detailForeground, 11.0f, TextAlign::Right, 400);
            canvas.drawLine({f.x + gutter - 0.5f, y}, {f.x + gutter - 0.5f, y + rowHeight}, s.gridLine);
        }
    }
    canvas.restore();
}

void DataGridSurface::paintCell(Canvas& canvas, const TableStyle& s, const DataGridCell* cell, Rect rect, int column) {
    if (!cell) return;
    const auto& t = theme();
    const std::string& classes = cell->classes;
    if (has(classes, "modified")) canvas.fillRect(rect, withAlpha(t.warning, 40));
    else if (has(classes, "error")) canvas.fillRect(rect, withAlpha(t.formField.errorColor, 40));
    Color color = s.cellForeground;
    if (has(classes, "null") || has(classes, "muted")) color = t.textSubtle;
    TextAlign align = columns[column].alignment;
    if (has(classes, "number") && !has(classes, "null")) align = TextAlign::Right;
    const Rect text = rect.inset(s.cellPadding);
    canvas.save();
    canvas.clipRect(rect);
    canvas.drawTextStyledEllipsized(cell->text, text, color, s.fontSize, align, 400);
    if (has(classes, "truncated"))
        canvas.fillRect(Rect{rect.x + rect.width - 6.0f, rect.y + 4.0f, 3.0f, rect.height - 8.0f}, withAlpha(t.warning, 160), 1.5f);
    canvas.restore();
}

void DataGridSurface::paintScrollbars(Canvas& canvas, const TableStyle& s) {
    const Rect vertical = verticalThumb();
    if (vertical.width > 0.0f) canvas.fillRect(vertical.inset(Insets{2.0f, 2.0f}), s.scrollbarColor, 3.0f);
    const Rect horizontal = horizontalThumb();
    if (horizontal.width > 0.0f) canvas.fillRect(horizontal.inset(Insets{2.0f, 2.0f}), s.scrollbarColor, 3.0f);
}

} // namespace oneui
