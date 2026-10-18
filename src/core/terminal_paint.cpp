#include "terminal_view_helpers.h"

namespace oneui {
using namespace terminal_detail;

namespace {
void drawTerminalUnderline(
    Canvas& canvas,
    Rect rect,
    Color color,
    TerminalUnderlineStyle style,
    float strokeWidth) {
    if (style == TerminalUnderlineStyle::None || rect.width <= 0.0f) {
        return;
    }

    const float stroke = std::max(1.0f, strokeWidth);
    const float left = rect.x + 1.0f;
    const float right = rect.x + std::max(1.0f, rect.width - 1.0f);
    const float baseline = rect.y + std::max(stroke, rect.height - stroke * 1.5f);
    const auto line = [&](float startX, float y, float endX) {
        canvas.drawLine(Point{startX, y}, Point{endX, y}, color, stroke);
    };

    switch (style) {
    case TerminalUnderlineStyle::None:
        return;
    case TerminalUnderlineStyle::Single:
        line(left, baseline, right);
        return;
    case TerminalUnderlineStyle::Double:
        line(left, baseline - stroke * 2.0f, right);
        line(left, baseline, right);
        return;
    case TerminalUnderlineStyle::Curly: {
        const float amplitude = std::max(1.0f, stroke);
        const float step = std::max(2.0f, rect.height * 0.16f);
        float x = left;
        float y = baseline - amplitude;
        bool descending = true;
        while (x < right) {
            const float nextX = std::min(right, x + step);
            const float nextY = descending ? baseline + amplitude : baseline - amplitude;
            canvas.drawLine(Point{x, y}, Point{nextX, nextY}, color, stroke);
            x = nextX;
            y = nextY;
            descending = !descending;
        }
        return;
    }
    case TerminalUnderlineStyle::Dotted: {
        const float mark = stroke;
        const float advance = stroke * 3.0f;
        for (float x = left; x < right; x += advance) {
            line(x, baseline, std::min(right, x + mark));
        }
        return;
    }
    case TerminalUnderlineStyle::Dashed: {
        const float mark = std::max(stroke * 3.0f, rect.height * 0.28f);
        const float advance = mark + stroke * 2.0f;
        for (float x = left; x < right; x += advance) {
            line(x, baseline, std::min(right, x + mark));
        }
        return;
    }
    }
}

}
void TerminalView::paint(Canvas& canvas) {
    const Rect bounds = frame();
    Rect paintBounds = bounds;
    if (const auto clip = canvas.clipBounds()) {
        const float left = std::max(bounds.x, clip->x);
        const float top = std::max(bounds.y, clip->y);
        const float right = std::min(bounds.x + bounds.width, clip->x + clip->width);
        const float bottom = std::min(bounds.y + bounds.height, clip->y + clip->height);
        paintBounds = Rect{
            left,
            top,
            std::max(0.0f, right - left),
            std::max(0.0f, bottom - top),
        };
    }
    if (paintBounds.width <= 0.0f || paintBounds.height <= 0.0f) {
        return;
    }
    canvas.fillRect(paintBounds, background_);
    if (rows_ == 0 || columns_ == 0) {
        return;
    }

    const GridMetrics metrics = gridMetrics(canvas);
    const Rect terminalBounds = contentBounds(bounds, metrics);
    const bool caretMetricsChanged = lastMetrics_.cellWidth != metrics.cellWidth ||
                                    lastMetrics_.cellHeight != metrics.cellHeight;
    lastMetrics_ = metrics;
    // The first cursor update can arrive before font metrics are available.
    // Notify anchored UI again after real cell measurements replace defaults.
    if (caretMetricsChanged && onCursorChanged_) onCursorChanged_();
    hasGridMetrics_ = true;
    reportViewport(bounds, metrics);
    canvas.save();
    canvas.clipRect(bounds);

    const auto firstRow = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor((paintBounds.y - bounds.y) / metrics.cellHeight)),
        0,
        static_cast<int>(rows_ - 1)));
    const auto lastRow = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::ceil(
            (paintBounds.y + paintBounds.height - bounds.y) / metrics.cellHeight)),
        static_cast<int>(firstRow + 1),
        static_cast<int>(rows_)));
    const auto firstColumn = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor((paintBounds.x - terminalBounds.x) / metrics.cellWidth)),
        0,
        static_cast<int>(columns_ - 1)));
    const auto lastColumn = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::ceil(
            (paintBounds.x + paintBounds.width - terminalBounds.x) / metrics.cellWidth)),
        static_cast<int>(firstColumn + 1),
        static_cast<int>(columns_)));

    if (lineNumbersVisible_ && firstVisibleLineNumber_ != 0) {
        const float gutterWidth = lineNumberGutterWidth(metrics);
        const Color gutterText = dim(dim(foreground_));
        const Color separator = dim(gutterText);
        const float numberSize = std::max(8.0f, fontSize_ * 0.78f);
        for (std::uint16_t row = firstRow; row < lastRow; ++row) {
            const auto number = std::to_wstring(firstVisibleLineNumber_ + row);
            canvas.drawTextCells(
                number,
                Rect{
                    bounds.x,
                    bounds.y + metrics.cellHeight * static_cast<float>(row),
                    std::max(1.0f, gutterWidth - metrics.cellWidth * 0.55f),
                    metrics.cellHeight,
                },
                gutterText,
                numberSize,
                TextAlign::Right,
                fontFamily_,
                TextFontFamily::Monospace,
                400);
        }
        const float separatorX = terminalBounds.x - std::max(2.0f, metrics.cellWidth * 0.28f);
        canvas.drawLine(
            Point{separatorX, bounds.y},
            Point{separatorX, bounds.y + bounds.height},
            separator,
            1.0f);
    }

    struct PaintStyle {
        Color foreground;
        Color background;
        Color underlineColor;
        int weight = 400;
        TerminalUnderlineStyle underlineStyle = TerminalUnderlineStyle::None;
        bool strikethrough = false;
        bool overline = false;
        bool textVisible = true;
        bool cursor = false;
    };

    const auto styleFor = [&](const TerminalCell& cell, std::uint16_t row, std::uint16_t column) {
        PaintStyle style{cell.foreground, cell.background, cell.foreground};
        if (hasStyle(cell, TerminalCellInverse)) {
            std::swap(style.foreground, style.background);
        }
        if (hasStyle(cell, TerminalCellDim)) {
            style.foreground = dim(style.foreground);
        }
        if (cellSelected(row, column)) {
            style.background = selectionBackground_;
        }
        style.cursor = cursorPaintVisible() && cursor_.row == row && cursor_.column == column;
        if (style.cursor && focused() && cursorStyle_ == TerminalCursorStyle::Block) {
            style.background = cursorColor_;
            style.foreground = background_;
        }
        style.weight = hasStyle(cell, TerminalCellBold) ? 700 : 400;
        style.underlineStyle = cell.underlineStyle;
        if (style.underlineStyle == TerminalUnderlineStyle::None &&
            hasStyle(cell, TerminalCellUnderline)) {
            style.underlineStyle = TerminalUnderlineStyle::Single;
        }
        if (style.underlineStyle == TerminalUnderlineStyle::None && cell.hyperlinkId != 0 &&
            cell.hyperlinkId == hoveredHyperlink_) {
            style.underlineStyle = TerminalUnderlineStyle::Single;
        }
        style.underlineColor = cell.underlineColorSet ? cell.underlineColor : style.foreground;
        style.strikethrough = hasStyle(cell, TerminalCellStrikethrough);
        style.overline = hasStyle(cell, TerminalCellOverline);
        style.textVisible = textPaintVisible(cell) && !hasStyle(cell, TerminalCellConceal);
        return style;
    };

    const auto samePaintStyle = [&](const PaintStyle& left, const PaintStyle& right) {
        return sameColor(left.foreground, right.foreground) &&
               sameColor(left.background, right.background) &&
               sameColor(left.underlineColor, right.underlineColor) &&
               left.weight == right.weight &&
               left.underlineStyle == right.underlineStyle &&
               left.strikethrough == right.strikethrough &&
               left.overline == right.overline &&
               left.textVisible == right.textVisible &&
               left.cursor == right.cursor;
    };

    const auto paintTextRun = [&](std::uint16_t row,
                                  std::uint16_t column,
                                  std::uint16_t span,
                                  const std::wstring& text,
                                  const PaintStyle& style) {
        const Rect runRect{
            terminalBounds.x + metrics.cellWidth * static_cast<float>(column),
            bounds.y + metrics.cellHeight * static_cast<float>(row),
            metrics.cellWidth * static_cast<float>(span),
            metrics.cellHeight,
        };
        if (!sameColor(style.background, background_) ||
            (style.cursor && focused() && cursorStyle_ == TerminalCursorStyle::Block)) {
            canvas.fillRect(runRect, style.background);
        }
        if (style.textVisible && !text.empty()) {
            canvas.drawTextCells(
                text,
                runRect,
                style.foreground,
                fontSize_,
                TextAlign::Left,
                fontFamily_,
                TextFontFamily::Monospace,
                style.weight);
        }
        const float decorationStroke = std::max(1.0f, fontSize_ / 14.0f);
        if (style.textVisible && style.underlineStyle != TerminalUnderlineStyle::None) {
            drawTerminalUnderline(
                canvas,
                runRect,
                style.underlineColor,
                style.underlineStyle,
                decorationStroke);
        }
        if (style.textVisible && style.strikethrough) {
            const float strikeY = runRect.y + runRect.height * 0.52f;
            canvas.drawLine(
                Point{runRect.x + 1.0f, strikeY},
                Point{runRect.x + std::max(1.0f, runRect.width - 1.0f), strikeY},
                style.foreground,
                decorationStroke);
        }
        if (style.textVisible && style.overline) {
            const float overlineY = runRect.y + decorationStroke;
            canvas.drawLine(
                Point{runRect.x + 1.0f, overlineY},
                Point{runRect.x + std::max(1.0f, runRect.width - 1.0f), overlineY},
                style.foreground,
                decorationStroke);
        }
        if (style.cursor && !focused() && cursorStyle_ == TerminalCursorStyle::Block) {
            canvas.strokeRect(runRect.inset(Insets{0.5f}), cursorColor_, 0.0f, 1.0f);
        } else if (style.cursor && cursorStyle_ == TerminalCursorStyle::Bar) {
            const float cursorX = runRect.x + 1.0f;
            canvas.drawLine(
                Point{cursorX, runRect.y + 1.0f},
                Point{cursorX, runRect.y + std::max(1.0f, runRect.height - 1.0f)},
                cursorColor_,
                2.0f);
        } else if (style.cursor && cursorStyle_ == TerminalCursorStyle::Underline) {
            const float cursorY = runRect.y + std::max(1.0f, runRect.height - 2.0f);
            canvas.drawLine(
                Point{runRect.x + 1.0f, cursorY},
                Point{runRect.x + std::max(1.0f, runRect.width - 1.0f), cursorY},
                cursorColor_,
                2.0f);
        }
    };

    std::wstring textRun;
    textRun.reserve(lastColumn - firstColumn);
    for (std::uint16_t row = firstRow; row < lastRow; ++row) {
        std::uint16_t column = firstColumn;
        if (column > 0) {
            const TerminalCell* first = cellAt(row, column);
            if (first && hasStyle(*first, TerminalCellWideContinuation)) {
                --column;
            }
        }
        for (; column < lastColumn;) {
            const TerminalCell* cell = cellAt(row, column);
            if (!cell || hasStyle(*cell, TerminalCellWideContinuation)) {
                ++column;
                continue;
            }

            const bool wide = hasStyle(*cell, TerminalCellWide);
            const PaintStyle style = styleFor(*cell, row, column);
            const bool ascii = !wide && !style.cursor && cell->text.size() == 1 &&
                               cell->text.front() >= L' ' && cell->text.front() <= L'~';
            if (!ascii) {
                const std::uint16_t span = wide ? 2 : 1;
                paintTextRun(row, column, span, cell->text, style);
                column = static_cast<std::uint16_t>(std::min<std::uint32_t>(columns_, column + span));
                continue;
            }

            textRun.assign(cell->text);
            std::uint16_t end = static_cast<std::uint16_t>(column + 1);
            while (letterSpacing_ == 0.0f && end < lastColumn) {
                const TerminalCell* next = cellAt(row, end);
                if (!next || hasStyle(*next, TerminalCellWide) ||
                    hasStyle(*next, TerminalCellWideContinuation) || next->text.size() != 1 ||
                    next->text.front() < L' ' || next->text.front() > L'~') {
                    break;
                }
                const PaintStyle nextStyle = styleFor(*next, row, end);
                if (nextStyle.cursor || !samePaintStyle(style, nextStyle)) {
                    break;
                }
                textRun += next->text;
                ++end;
            }
            paintTextRun(
                row,
                column,
                static_cast<std::uint16_t>(end - column),
                textRun,
                style);
            column = end;
        }
    }
    canvas.restore();
    if (bellAlpha_ != 0) {
        canvas.fillRect(bounds, Color{foreground_.r, foreground_.g, foreground_.b, bellAlpha_});
    }
    const auto thumb = scrollbackThumbRect();
    if (thumb.height > 0.0f) canvas.fillRect(thumb, Color{foreground_.r, foreground_.g, foreground_.b, static_cast<std::uint8_t>(draggingScrollback_ ? 110 : 65)}, 3.0f);
}

} // namespace oneui
