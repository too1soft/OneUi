#include "terminal_view_helpers.h"

namespace oneui {
using namespace terminal_detail;

bool TerminalView::tickAnimations(double nowMs) {
    const bool cursorActive = focused() && cursorBlinking_ && cursor_.visible;
    if (cursorActive) {
        const double elapsed = std::fmod(
            std::max(0.0, nowMs - cursorBlinkStartMs_),
            kCursorBlinkPeriodMs);
        const bool visible = elapsed < kCursorBlinkOnMs;
        if (visible != cursorBlinkVisible_) {
            cursorBlinkVisible_ = visible;
            invalidateCell(TextPosition{cursor_.row, cursor_.column});
        }
    } else if (!cursorBlinkVisible_) {
        cursorBlinkVisible_ = true;
        invalidateCell(TextPosition{cursor_.row, cursor_.column});
    }

    const bool textBlinkActive = hasBlinkingText();
    if (textBlinkActive) {
        if (textBlinkStartMs_ <= 0.0) {
            textBlinkStartMs_ = nowMs;
        }
        const double elapsed = std::max(0.0, nowMs - textBlinkStartMs_);
        if (slowBlinkCellCount_ != 0) {
            const bool visible = std::fmod(elapsed, kSlowTextBlinkPeriodMs) < kSlowTextBlinkOnMs;
            if (visible != slowBlinkVisible_) {
                slowBlinkVisible_ = visible;
                invalidateBlinkingCells(TerminalCellBlinkSlow);
            }
        }
        if (rapidBlinkCellCount_ != 0) {
            const bool visible = std::fmod(elapsed, kRapidTextBlinkPeriodMs) < kRapidTextBlinkOnMs;
            if (visible != rapidBlinkVisible_) {
                rapidBlinkVisible_ = visible;
                invalidateBlinkingCells(TerminalCellBlinkRapid);
            }
        }
    }
    return cursorActive || textBlinkActive;
}

void TerminalView::restartCursorBlink() {
    cursorBlinkStartMs_ = currentTimeMs();
    const bool changed = !cursorBlinkVisible_;
    cursorBlinkVisible_ = true;
    if (changed && cursor_.visible) {
        invalidateCell(TextPosition{cursor_.row, cursor_.column});
    }
    if (focused() && cursorBlinking_ && cursor_.visible) {
        requestAnimationFrame();
    }
}

void TerminalView::rebuildBlinkCellCounts() {
    slowBlinkCellCount_ = 0;
    rapidBlinkCellCount_ = 0;
    for (const TerminalCell& cell : cells_) {
        slowBlinkCellCount_ += hasStyle(cell, TerminalCellBlinkSlow) ? 1u : 0u;
        rapidBlinkCellCount_ += hasStyle(cell, TerminalCellBlinkRapid) ? 1u : 0u;
    }
}

void TerminalView::updateBlinkCellCounts(
    const TerminalCell& previous,
    const TerminalCell& current) {
    const auto updateCount = [&](TerminalCellStyle style, std::size_t& count) {
        const bool hadStyle = hasStyle(previous, style);
        const bool hasCurrentStyle = hasStyle(current, style);
        if (hadStyle == hasCurrentStyle) {
            return;
        }
        if (hasCurrentStyle) {
            ++count;
        } else if (count != 0) {
            --count;
        }
    };
    updateCount(TerminalCellBlinkSlow, slowBlinkCellCount_);
    updateCount(TerminalCellBlinkRapid, rapidBlinkCellCount_);
}

void TerminalView::refreshTextBlinkAnimation(bool wasActive) {
    const bool active = hasBlinkingText();
    if (!active) {
        textBlinkStartMs_ = 0.0;
        slowBlinkVisible_ = true;
        rapidBlinkVisible_ = true;
        return;
    }
    if (!wasActive) {
        textBlinkStartMs_ = 0.0;
        slowBlinkVisible_ = true;
        rapidBlinkVisible_ = true;
        requestAnimationFrame();
    }
    if (slowBlinkCellCount_ == 0) {
        slowBlinkVisible_ = true;
    }
    if (rapidBlinkCellCount_ == 0) {
        rapidBlinkVisible_ = true;
    }
}

void TerminalView::invalidateBlinkingCells(TerminalCellStyle blinkStyle) {
    if (cells_.empty() || columns_ == 0) {
        return;
    }
    for (std::size_t row = 0; row < rows_; ++row) {
        const std::size_t rowStart = row * columns_;
        std::size_t column = 0;
        while (column < columns_) {
            while (column < columns_ && !hasStyle(cells_[rowStart + column], blinkStyle)) {
                ++column;
            }
            const std::size_t first = column;
            while (column < columns_ && hasStyle(cells_[rowStart + column], blinkStyle)) {
                ++column;
            }
            if (first < column) {
                invalidateCellRange(rowStart + first, column - first);
            }
        }
    }
}

bool TerminalView::textPaintVisible(const TerminalCell& cell) const {
    return (!hasStyle(cell, TerminalCellBlinkSlow) || slowBlinkVisible_) &&
           (!hasStyle(cell, TerminalCellBlinkRapid) || rapidBlinkVisible_);
}

bool TerminalView::hasBlinkingText() const {
    return slowBlinkCellCount_ != 0 || rapidBlinkCellCount_ != 0;
}

bool TerminalView::cursorPaintVisible() const {
    return cursor_.visible && (!focused() || !cursorBlinking_ || cursorBlinkVisible_);
}

Rect TerminalView::cellRect(TextPosition position) const {
    const Rect bounds = frame();
    const float width = std::max(1.0f, lastMetrics_.cellWidth);
    const float height = std::max(1.0f, lastMetrics_.cellHeight);
    std::uint16_t span = 1;
    const TerminalCell* cell = cellAt(position.row, position.column);
    if (cell && hasStyle(*cell, TerminalCellWide)) {
        span = 2;
    }
    return Rect{
        contentBounds(bounds, lastMetrics_).x + width * static_cast<float>(position.column),
        bounds.y + height * static_cast<float>(position.row),
        width * static_cast<float>(span),
        height,
    };
}

void TerminalView::invalidateCell(TextPosition position) {
    if (!hasGridMetrics_ || position.row >= rows_ || position.column >= columns_) {
        invalidate();
        return;
    }
    invalidateRect(cellRect(normalizedCellPosition(position)));
}

void TerminalView::invalidateCellRange(std::size_t firstCell, std::size_t count) {
    if (count == 0) {
        return;
    }
    if (!hasGridMetrics_ || columns_ == 0 || firstCell >= cells_.size()) {
        invalidate();
        return;
    }

    const std::size_t lastCell = std::min(cells_.size() - 1, firstCell + count - 1);
    const std::size_t firstRow = firstCell / columns_;
    const std::size_t lastRow = lastCell / columns_;
    const std::size_t firstColumn = firstCell % columns_;
    const std::size_t lastColumn = lastCell % columns_;
    const Rect bounds = frame();
    const float contentX = contentBounds(bounds, lastMetrics_).x;
    const float width = std::max(1.0f, lastMetrics_.cellWidth);
    const float height = std::max(1.0f, lastMetrics_.cellHeight);

    if (firstRow == lastRow) {
        invalidateRect(Rect{
            contentX + width * static_cast<float>(firstColumn),
            bounds.y + height * static_cast<float>(firstRow),
            width * static_cast<float>(lastColumn - firstColumn + 1),
            height,
        });
        return;
    }

    invalidateRect(Rect{
        contentX + width * static_cast<float>(firstColumn),
        bounds.y + height * static_cast<float>(firstRow),
        width * static_cast<float>(columns_ - firstColumn),
        height,
    });
    if (lastRow > firstRow + 1) {
        invalidateRect(Rect{
            contentX,
            bounds.y + height * static_cast<float>(firstRow + 1),
            width * static_cast<float>(columns_),
            height * static_cast<float>(lastRow - firstRow - 1),
        });
    }
    invalidateRect(Rect{
        contentX,
        bounds.y + height * static_cast<float>(lastRow),
        width * static_cast<float>(lastColumn + 1),
        height,
    });
}

} // namespace oneui
