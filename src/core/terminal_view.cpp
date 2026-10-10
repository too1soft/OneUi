#include "terminal_view_helpers.h"

namespace oneui {
using namespace terminal_detail;

TerminalView::TerminalView() {
    setPreferredSize(Size{640.0f, 400.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"Terminal");
    setAccessibleDescription(L"Interactive terminal output");
}

void TerminalView::setGrid(std::uint16_t rows, std::uint16_t columns, std::vector<TerminalCell> cells) {
    const bool wasBlinking = hasBlinkingText();
    std::size_t requested = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
    if (requested > kMaximumCells) {
        rows = 0;
        columns = 0;
        requested = 0;
        cells.clear();
    } else {
        cells.resize(requested);
    }

    const bool dimensionsChanged = rows != rows_ || columns != columns_;
    if (dimensionsChanged) {
        rows_ = rows;
        columns_ = columns;
        cells_ = std::move(cells);
        clearSelection();
        if (cursor_.row >= rows_ || cursor_.column >= columns_) {
            cursor_.visible = false;
        }
        hoveredHyperlink_ = 0;
        pressedHyperlink_ = 0;
        rebuildBlinkCellCounts();
        refreshTextBlinkAnimation(wasBlinking);
        invalidate();
        return;
    }

    std::vector<std::pair<std::size_t, std::size_t>> dirtyRanges;
    dirtyRanges.reserve(4);
    std::size_t dirtyStart = std::numeric_limits<std::size_t>::max();
    std::size_t firstChanged = std::numeric_limits<std::size_t>::max();
    std::size_t lastChanged = 0;
    bool collapsed = false;
    for (std::size_t index = 0; index < requested; ++index) {
        if (sameTerminalCell(cells_[index], cells[index])) {
            if (dirtyStart != std::numeric_limits<std::size_t>::max()) {
                if (!collapsed) {
                    dirtyRanges.emplace_back(dirtyStart, index - dirtyStart);
                    if (dirtyRanges.size() > kMaximumSparseInvalidations) {
                        dirtyRanges.clear();
                        collapsed = true;
                    }
                }
                dirtyStart = std::numeric_limits<std::size_t>::max();
            }
            continue;
        }
        firstChanged = std::min(firstChanged, index);
        lastChanged = index;
        if (dirtyStart == std::numeric_limits<std::size_t>::max()) {
            dirtyStart = index;
        }
        updateBlinkCellCounts(cells_[index], cells[index]);
        cells_[index] = std::move(cells[index]);
    }
    if (dirtyStart != std::numeric_limits<std::size_t>::max() && !collapsed) {
        dirtyRanges.emplace_back(dirtyStart, requested - dirtyStart);
        if (dirtyRanges.size() > kMaximumSparseInvalidations) {
            collapsed = true;
        }
    }
    if (firstChanged == std::numeric_limits<std::size_t>::max()) {
        return;
    }
    if (hoveredHyperlink_ != 0 &&
        std::none_of(cells_.begin(), cells_.end(), [&](const TerminalCell& cell) {
            return cell.hyperlinkId == hoveredHyperlink_;
        })) {
        hoveredHyperlink_ = 0;
    }
    if (pressedHyperlink_ != 0 &&
        std::none_of(cells_.begin(), cells_.end(), [&](const TerminalCell& cell) {
            return cell.hyperlinkId == pressedHyperlink_;
        })) {
        pressedHyperlink_ = 0;
    }
    refreshTextBlinkAnimation(wasBlinking);
    if (collapsed) {
        invalidateCellRange(firstChanged, lastChanged - firstChanged + 1);
        return;
    }
    for (const auto& [first, count] : dirtyRanges) {
        invalidateCellRange(first, count);
    }
}

void TerminalView::updateCells(std::size_t firstCell, std::vector<TerminalCell> cells) {
    if (cells.empty() || firstCell >= cells_.size()) {
        return;
    }

    const bool wasBlinking = hasBlinkingText();
    const std::size_t count = std::min(cells.size(), cells_.size() - firstCell);
    std::size_t dirtyStart = std::numeric_limits<std::size_t>::max();
    for (std::size_t offset = 0; offset < count; ++offset) {
        const std::size_t target = firstCell + offset;
        if (sameTerminalCell(cells_[target], cells[offset])) {
            if (dirtyStart != std::numeric_limits<std::size_t>::max()) {
                invalidateCellRange(dirtyStart, target - dirtyStart);
                dirtyStart = std::numeric_limits<std::size_t>::max();
            }
            continue;
        }
        if (dirtyStart == std::numeric_limits<std::size_t>::max()) {
            dirtyStart = target;
        }
        updateBlinkCellCounts(cells_[target], cells[offset]);
        cells_[target] = std::move(cells[offset]);
    }
    if (dirtyStart != std::numeric_limits<std::size_t>::max()) {
        invalidateCellRange(dirtyStart, firstCell + count - dirtyStart);
    }
    if (hoveredHyperlink_ != 0 &&
        std::none_of(cells_.begin(), cells_.end(), [&](const TerminalCell& cell) {
            return cell.hyperlinkId == hoveredHyperlink_;
        })) {
        hoveredHyperlink_ = 0;
    }
    if (pressedHyperlink_ != 0 &&
        std::none_of(cells_.begin(), cells_.end(), [&](const TerminalCell& cell) {
            return cell.hyperlinkId == pressedHyperlink_;
        })) {
        pressedHyperlink_ = 0;
    }
    refreshTextBlinkAnimation(wasBlinking);
}

Size TerminalView::gridSize() const {
    return Size{static_cast<float>(columns_), static_cast<float>(rows_)};
}

std::size_t TerminalView::cellCount() const {
    return cells_.size();
}

const TerminalCell* TerminalView::cellAt(std::uint16_t row, std::uint16_t column) const {
    const std::size_t index = cellIndex(row, column);
    return index < cells_.size() ? &cells_[index] : nullptr;
}

void TerminalView::setCursor(TerminalCursor cursor) {
    const TerminalCursor previous = cursor_;
    cursor_ = cursor;
    if (cursor_.row >= rows_ || cursor_.column >= columns_) {
        cursor_.visible = false;
    }
    const bool changed = previous.row != cursor_.row || previous.column != cursor_.column ||
                         previous.visible != cursor_.visible;
    if (!changed) {
        return;
    }
    if (previous.visible) {
        invalidateCell(TextPosition{previous.row, previous.column});
    }
    if (cursor_.visible) {
        invalidateCell(TextPosition{cursor_.row, cursor_.column});
    }
    restartCursorBlink();
    if (onCursorChanged_) onCursorChanged_();
}

TerminalCursor TerminalView::cursorState() const {
    return cursor_;
}

void TerminalView::setCursorStyle(TerminalCursorStyle style) {
    if (cursorStyle_ == style) {
        return;
    }
    cursorStyle_ = style;
    invalidateCell(TextPosition{cursor_.row, cursor_.column});
}

TerminalCursorStyle TerminalView::cursorStyle() const {
    return cursorStyle_;
}

void TerminalView::setCursorBlinking(bool enabled) {
    if (cursorBlinking_ == enabled) {
        return;
    }
    cursorBlinking_ = enabled;
    cursorBlinkVisible_ = true;
    invalidateCell(TextPosition{cursor_.row, cursor_.column});
    if (enabled && focused() && cursor_.visible) {
        restartCursorBlink();
    }
}

bool TerminalView::cursorBlinking() const {
    return cursorBlinking_;
}

void TerminalView::setFontSize(float size) {
    const float clamped = std::max(6.0f, std::min(size, 72.0f));
    if (fontSize_ == clamped) {
        return;
    }
    fontSize_ = clamped;
    viewport_ = {};
    hasGridMetrics_ = false;
    invalidate();
}

float TerminalView::fontSize() const {
    return fontSize_;
}

void TerminalView::setFontFamily(std::wstring family) {
    if (fontFamily_ == family) {
        return;
    }
    fontFamily_ = std::move(family);
    viewport_ = {};
    hasGridMetrics_ = false;
    invalidate();
}

const std::wstring& TerminalView::fontFamily() const {
    return fontFamily_;
}

void TerminalView::setLineHeight(float multiplier) {
    const float clamped = std::clamp(multiplier, 1.0f, 2.0f);
    if (lineHeight_ == clamped) {
        return;
    }
    lineHeight_ = clamped;
    viewport_ = {};
    hasGridMetrics_ = false;
    invalidate();
}

float TerminalView::lineHeight() const {
    return lineHeight_;
}

void TerminalView::setLetterSpacing(float pixels) {
    const float clamped = std::clamp(pixels, 0.0f, 8.0f);
    if (letterSpacing_ == clamped) {
        return;
    }
    letterSpacing_ = clamped;
    viewport_ = {};
    hasGridMetrics_ = false;
    invalidate();
}

float TerminalView::letterSpacing() const {
    return letterSpacing_;
}

void TerminalView::setLineNumbersVisible(bool visible) {
    if (lineNumbersVisible_ == visible) {
        return;
    }
    lineNumbersVisible_ = visible;
    viewport_ = {};
    hasGridMetrics_ = false;
    invalidate();
}

bool TerminalView::lineNumbersVisible() const {
    return lineNumbersVisible_;
}

void TerminalView::setFirstVisibleLineNumber(std::uint64_t lineNumber) {
    if (firstVisibleLineNumber_ == lineNumber) {
        return;
    }
    if (!lineNumbersVisible_) {
        firstVisibleLineNumber_ = lineNumber;
        return;
    }

    const float previousGutter = hasGridMetrics_ ? lineNumberGutterWidth(lastMetrics_) : -1.0f;
    firstVisibleLineNumber_ = lineNumber;
    const float nextGutter = hasGridMetrics_ ? lineNumberGutterWidth(lastMetrics_) : -1.0f;
    if (!hasGridMetrics_ || previousGutter != nextGutter) {
        viewport_ = {};
        invalidate();
        return;
    }

    const Rect bounds = frame();
    invalidateRect(Rect{bounds.x, bounds.y, nextGutter, bounds.height});
}

std::uint64_t TerminalView::firstVisibleLineNumber() const {
    return firstVisibleLineNumber_;
}

void TerminalView::setPalette(Color background, Color foreground, Color cursor) {
    background_ = background;
    foreground_ = foreground;
    cursorColor_ = cursor;
    invalidate();
}

void TerminalView::setSelectionBackground(Color color) {
    selectionBackground_ = color;
    invalidate();
}

void TerminalView::setStyleBox(const StyleBox& style) {
    if (style.background.color) {
        background_ = *style.background.color;
    }
    if (style.foreground) {
        foreground_ = *style.foreground;
    }
    if (style.caretColor) {
        cursorColor_ = *style.caretColor;
    }
    if (style.selectionColor) {
        selectionBackground_ = *style.selectionColor;
    }
    if (style.fontSize) {
        setFontSize(*style.fontSize);
    }
    invalidate();
}

void TerminalView::setCopyOnSelect(bool enabled) {
    copyOnSelect_ = enabled;
}

bool TerminalView::copyOnSelect() const {
    return copyOnSelect_;
}

void TerminalView::setScrollRowsPerWheel(float rows) {
    scrollRowsPerWheel_ = std::clamp(rows, 0.25f, 100.0f);
    wheelRowRemainder_ = 0.0f;
}

void TerminalView::setMouseReporting(bool enabled) {
    if (mouseReporting_ == enabled) {
        return;
    }
    mouseReporting_ = enabled;
    reportedButton_ = MouseButton::None;
    pointerWheelRemainder_ = 0.0f;
}

void TerminalView::setClipboard(std::shared_ptr<Clipboard> clipboard) {
    clipboard_ = std::move(clipboard);
}

} // namespace oneui
