#include "terminal_view_helpers.h"

namespace oneui {
using namespace terminal_detail;

void TerminalView::selectAll() {
    if (rows_ == 0 || columns_ == 0) {
        return;
    }
    const SelectionSnapshot previous = selectionSnapshot();
    selectionAnchor_ = TextPosition{0, 0};
    selectionCaret_ = TextPosition{
        static_cast<std::uint16_t>(rows_ - 1),
        columns_,
    };
    hasSelection_ = true;
    selecting_ = false;
    invalidateSelectionDelta(previous);
}

void TerminalView::setSelection(
    std::uint16_t startRow,
    std::uint16_t startColumn,
    std::uint16_t endRow,
    std::uint16_t endColumn) {
    if (rows_ == 0 || columns_ == 0) {
        clearSelection();
        return;
    }
    const SelectionSnapshot previous = selectionSnapshot();

    const auto clampPosition = [&](std::uint16_t row, std::uint16_t column) {
        TextPosition position{
            std::min<std::uint16_t>(row, static_cast<std::uint16_t>(rows_ - 1)),
            std::min<std::uint16_t>(column, columns_),
        };
        if (position.column < columns_) {
            position = normalizedCellPosition(position);
        }
        return position;
    };

    selectionAnchor_ = clampPosition(startRow, startColumn);
    selectionCaret_ = clampPosition(endRow, endColumn);
    hasSelection_ = !(selectionAnchor_ == selectionCaret_);
    selecting_ = false;
    invalidateSelectionDelta(previous);
}

void TerminalView::clearSelection() {
    const SelectionSnapshot previous = selectionSnapshot();
    selectionAnchor_ = {};
    selectionCaret_ = {};
    hasSelection_ = false;
    selecting_ = false;
    invalidateSelectionDelta(previous);
}

bool TerminalView::hasSelection() const {
    return hasSelection_ && !(selectionAnchor_ == selectionCaret_);
}

std::wstring TerminalView::selectedText() const {
    if (!hasSelection() || rows_ == 0 || columns_ == 0) {
        return {};
    }

    const TextPosition start = selectionStart();
    const TextPosition end = selectionEnd();
    std::wstring result;
    for (std::uint32_t rowValue = start.row; rowValue <= end.row && rowValue < rows_; ++rowValue) {
        const auto row = static_cast<std::uint16_t>(rowValue);
        std::uint16_t from = row == start.row ? std::min(start.column, columns_) : 0;
        const std::uint16_t to = row == end.row ? std::min(end.column, columns_) : columns_;
        if (from > 0) {
            const TerminalCell* first = cellAt(row, from);
            if (first && hasStyle(*first, TerminalCellWideContinuation)) {
                --from;
            }
        }

        std::wstring line;
        for (std::uint16_t column = from; column < to; ++column) {
            const TerminalCell* cell = cellAt(row, column);
            if (!cell || hasStyle(*cell, TerminalCellWideContinuation)) {
                continue;
            }
            if (cell->text.empty()) {
                line.push_back(L' ');
            } else {
                line += cell->text;
            }
        }
        while (!line.empty() && line.back() == L' ') {
            line.pop_back();
        }
        if (rowValue != start.row) {
            result += L"\r\n";
        }
        result += line;
    }
    return result;
}

bool TerminalView::copySelectionToClipboard() {
    if (!clipboard_ || !hasSelection()) {
        return false;
    }
    try { clipboard_->setText(selectedText()); } catch (...) { detail::recordCurrentException(__func__); return false; }
    return true;
}

bool TerminalView::pasteFromClipboard() {
    if (!clipboard_) {
        return false;
    }
    std::wstring text;
    try { text = clipboard_->text(); } catch (...) { detail::recordCurrentException(__func__); return false; }
    if (text.empty()) {
        return false;
    }
    return pasteText(text);
}

bool TerminalView::pasteText(const std::wstring& text) {
    if (text.empty()) {
        return false;
    }
    clearSelection();
    if (onPaste_) {
        onPaste_(text);
    } else if (onTextInput_) {
        onTextInput_(text);
    } else {
        return false;
    }
    restartCursorBlink();
    return true;
}

std::size_t TerminalView::cellIndex(std::uint16_t row, std::uint16_t column) const {
    if (row >= rows_ || column >= columns_) {
        return std::numeric_limits<std::size_t>::max();
    }
    return static_cast<std::size_t>(row) * columns_ + column;
}

TerminalView::TextPosition TerminalView::selectionStart() const {
    return selectionAnchor_ < selectionCaret_ ? selectionAnchor_ : selectionCaret_;
}

TerminalView::TextPosition TerminalView::selectionEnd() const {
    return selectionAnchor_ < selectionCaret_ ? selectionCaret_ : selectionAnchor_;
}

TerminalView::SelectionSnapshot TerminalView::selectionSnapshot() const {
    if (!hasSelection() || columns_ == 0 || cells_.empty()) {
        return {};
    }

    const auto linearIndex = [&](TextPosition position) {
        const std::size_t row = std::min<std::size_t>(position.row, rows_);
        const std::size_t column = std::min<std::size_t>(position.column, columns_);
        return std::min(cells_.size(), row * static_cast<std::size_t>(columns_) + column);
    };
    const std::size_t start = linearIndex(selectionStart());
    const std::size_t end = linearIndex(selectionEnd());
    return SelectionSnapshot{start, end, start < end};
}

void TerminalView::invalidateSelectionDelta(SelectionSnapshot previous) {
    const SelectionSnapshot current = selectionSnapshot();
    const auto invalidateRange = [&](std::size_t start, std::size_t end) {
        if (start < end) {
            invalidateCellRange(start, end - start);
        }
    };

    if (!previous.active) {
        if (current.active) {
            invalidateRange(current.start, current.end);
        }
        return;
    }
    if (!current.active) {
        invalidateRange(previous.start, previous.end);
        return;
    }
    if (previous.start == current.start && previous.end == current.end) {
        return;
    }
    if (previous.end <= current.start || current.end <= previous.start) {
        invalidateRange(previous.start, previous.end);
        invalidateRange(current.start, current.end);
        return;
    }

    invalidateRange(
        std::min(previous.start, current.start),
        std::max(previous.start, current.start));
    invalidateRange(
        std::min(previous.end, current.end),
        std::max(previous.end, current.end));
}

TerminalView::TextPosition TerminalView::positionFromPoint(Point point) const {
    if (rows_ == 0 || columns_ == 0) {
        return {};
    }
    const Rect bounds = frame();
    const float localX = std::max(0.0f, point.x - contentBounds(bounds, lastMetrics_).x);
    const float localY = std::max(0.0f, point.y - bounds.y);
    const float cellWidth = std::max(1.0f, lastMetrics_.cellWidth);
    const float cellHeight = std::max(1.0f, lastMetrics_.cellHeight);
    const auto row = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor(localY / cellHeight)),
        0,
        static_cast<int>(rows_ - 1)));
    const float exactColumn = localX / cellWidth;
    const auto column = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor(exactColumn + 0.5f)),
        0,
        static_cast<int>(columns_)));
    return TextPosition{row, column};
}

TerminalView::TextPosition TerminalView::cellPositionFromPoint(Point point) const {
    if (rows_ == 0 || columns_ == 0) {
        return {};
    }
    const Rect bounds = frame();
    const float cellWidth = std::max(1.0f, lastMetrics_.cellWidth);
    const float cellHeight = std::max(1.0f, lastMetrics_.cellHeight);
    const auto row = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor((point.y - bounds.y) / cellHeight)),
        0,
        static_cast<int>(rows_ - 1)));
    const auto column = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor(
            (point.x - contentBounds(bounds, lastMetrics_).x) / cellWidth)),
        0,
        static_cast<int>(columns_ - 1)));
    return TextPosition{row, column};
}

TerminalView::TextPosition TerminalView::normalizedCellPosition(TextPosition position) const {
    if (position.row >= rows_ || position.column >= columns_) {
        return position;
    }
    const TerminalCell* cell = cellAt(position.row, position.column);
    if (cell && hasStyle(*cell, TerminalCellWideContinuation) && position.column > 0) {
        --position.column;
    }
    return position;
}

TerminalView::CellWordClass TerminalView::wordClassAt(TextPosition position) const {
    position = normalizedCellPosition(position);
    const TerminalCell* cell = cellAt(position.row, position.column);
    if (!cell || cell->text.empty()) {
        return CellWordClass::Whitespace;
    }
    const wchar_t character = cell->text.front();
    if (std::iswspace(character)) {
        return CellWordClass::Whitespace;
    }
    if (character == L'_' || character >= 0x80 || std::iswalnum(character)) {
        return CellWordClass::Word;
    }
    return CellWordClass::Punctuation;
}

TerminalView::SelectionRange TerminalView::wordRangeAt(TextPosition position) const {
    position = normalizedCellPosition(position);
    if (position.row >= rows_ || position.column >= columns_) {
        return SelectionRange{position, position};
    }

    const CellWordClass targetClass = wordClassAt(position);
    std::uint16_t left = position.column;
    while (left > 0) {
        TextPosition candidate = normalizedCellPosition(
            TextPosition{position.row, static_cast<std::uint16_t>(left - 1)});
        if (candidate.column >= left || wordClassAt(candidate) != targetClass) {
            break;
        }
        left = candidate.column;
    }

    const TerminalCell* selectedCell = cellAt(position.row, position.column);
    std::uint16_t right = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        columns_,
        position.column + (selectedCell && hasStyle(*selectedCell, TerminalCellWide) ? 2u : 1u)));
    while (right < columns_) {
        TextPosition candidate = normalizedCellPosition(TextPosition{position.row, right});
        if (candidate.column < right) {
            ++right;
            continue;
        }
        if (wordClassAt(candidate) != targetClass) {
            break;
        }
        const TerminalCell* cell = cellAt(candidate.row, candidate.column);
        right = static_cast<std::uint16_t>(std::min<std::uint32_t>(
            columns_,
            right + (cell && hasStyle(*cell, TerminalCellWide) ? 2u : 1u)));
    }
    return SelectionRange{
        TextPosition{position.row, left},
        TextPosition{position.row, right},
    };
}

void TerminalView::selectWordAt(TextPosition position) {
    const SelectionRange range = wordRangeAt(position);
    selectionAnchor_ = range.start;
    selectionCaret_ = range.end;
    selectionOriginStart_ = range.start;
    selectionOriginEnd_ = range.end;
    hasSelection_ = !(selectionAnchor_ == selectionCaret_);
}

void TerminalView::selectLineAt(std::uint16_t row) {
    row = std::min<std::uint16_t>(row, static_cast<std::uint16_t>(rows_ - 1));
    selectionAnchor_ = TextPosition{row, 0};
    selectionCaret_ = TextPosition{row, columns_};
    selectionOriginStart_ = selectionAnchor_;
    selectionOriginEnd_ = selectionCaret_;
    hasSelection_ = columns_ != 0;
}

void TerminalView::extendWordSelection(TextPosition position) {
    const SelectionRange range = wordRangeAt(position);
    if (range.end < selectionOriginStart_ || range.end == selectionOriginStart_) {
        selectionAnchor_ = selectionOriginEnd_;
        selectionCaret_ = range.start;
    } else {
        selectionAnchor_ = selectionOriginStart_;
        selectionCaret_ = range.end;
    }
}

void TerminalView::extendLineSelection(std::uint16_t row) {
    row = std::min<std::uint16_t>(row, static_cast<std::uint16_t>(rows_ - 1));
    if (row < selectionOriginStart_.row) {
        selectionAnchor_ = selectionOriginEnd_;
        selectionCaret_ = TextPosition{row, 0};
    } else {
        selectionAnchor_ = selectionOriginStart_;
        selectionCaret_ = TextPosition{row, columns_};
    }
}

bool TerminalView::cellSelected(std::uint16_t row, std::uint16_t column) const {
    if (!hasSelection()) {
        return false;
    }
    const TextPosition start = selectionStart();
    const TextPosition end = selectionEnd();
    const auto selectedAt = [&](std::uint16_t candidate) {
        const TextPosition position{row, candidate};
        return !(position < start) && position < end;
    };
    if (selectedAt(column)) {
        return true;
    }
    const TerminalCell* cell = cellAt(row, column);
    return cell && hasStyle(*cell, TerminalCellWide) && column < columns_ - 1 &&
           selectedAt(static_cast<std::uint16_t>(column + 1));
}

} // namespace oneui
