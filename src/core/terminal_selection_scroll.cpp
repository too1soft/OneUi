#include "terminal_view_helpers.h"

namespace oneui {
using namespace terminal_detail;

void TerminalView::updateSelectionCaret(Point point) {
    const auto previous = selectionSnapshot();
    switch (selectionMode_) {
    case SelectionMode::Word: extendWordSelection(cellPositionFromPoint(point)); break;
    case SelectionMode::Line: extendLineSelection(cellPositionFromPoint(point).row); break;
    case SelectionMode::Character: selectionCaret_ = positionFromPoint(point); break;
    }
    hasSelection_ = !(selectionAnchor_ == selectionCaret_);
    invalidateSelectionDelta(previous);
}

void TerminalView::rememberSelectionViewport() {
    const auto top = static_cast<std::int64_t>(historyRows_ - scrollbackOffset_);
    const auto first = top + std::min<std::int64_t>(0, selectionStart().row);
    const auto last = top + std::max<std::int64_t>(rows_ - 1, selectionEnd().row);
    selectionRows_.erase(selectionRows_.begin(), selectionRows_.lower_bound(first));
    selectionRows_.erase(selectionRows_.upper_bound(last), selectionRows_.end());
    for (std::uint16_t row = 0; row < rows_; ++row) {
        auto& line = selectionRows_[top + row];
        line.clear();
        line.reserve(columns_);
        for (std::uint16_t column = 0; column < columns_; ++column) {
            const auto* cell = cellAt(row, column);
            line.push_back(!cell || hasStyle(*cell, TerminalCellWideContinuation) ? L""
                : cell->text.empty() ? L" " : cell->text);
        }
    }
}

void TerminalView::selectionScrolled(std::uint64_t historyRows, std::uint64_t offset) {
    selectionScrollPending_ = false;
    if (!hasSelection_ && !selecting_) return;
    if (historyRows < historyRows_) { clearSelection(); return; }
    const auto oldTop = static_cast<std::int64_t>(historyRows_ - scrollbackOffset_);
    const auto newTop = static_cast<std::int64_t>(historyRows - offset);
    if (oldTop == newTop) return;
    // A programmatic jump can skip rows the view has never received. Do not
    // fabricate copied text for that gap or iterate an unbounded row range.
    if (std::abs(oldTop - newTop) > rows_) { clearSelection(); return; }
    // Call before replacing the old frame. Coordinates may move outside the
    // viewport; painting clips them, copying uses the remembered rows.
    rememberSelectionViewport();
    const auto delta = oldTop - newTop;
    for (auto* position : {&selectionAnchor_, &selectionCaret_, &selectionOriginStart_, &selectionOriginEnd_}) {
        position->row += delta;
    }
    invalidate();
    if (selecting_) requestAnimationFrame();
}

std::wstring TerminalView::selectedRowText(std::int64_t row, std::uint16_t from, std::uint16_t to) const {
    const auto top = static_cast<std::int64_t>(historyRows_ - scrollbackOffset_);
    const bool visible = row >= 0 && row < rows_;
    const auto cached = selectionRows_.find(top + row);
    const auto textAt = [&](std::uint16_t column) -> std::wstring {
        if (visible) {
            const auto* cell = cellAt(static_cast<std::uint16_t>(row), column);
            if (!cell || hasStyle(*cell, TerminalCellWideContinuation)) return {};
            return cell->text.empty() ? L" " : cell->text;
        }
        return cached != selectionRows_.end() && column < cached->second.size() ? cached->second[column] : L"";
    };
    if (from > 0 && from < columns_ && textAt(from).empty()) --from;
    std::wstring result;
    for (auto column = from; column < to; ++column) result += textAt(column);
    while (!result.empty() && result.back() == L' ') result.pop_back();
    return result;
}

bool TerminalView::tickSelectionScroll(double nowMs) {
    if (!selecting_ || !onScroll_ || !rows_ || !columns_) return false;
    updateSelectionCaret(selectionPointer_);
    const auto bounds = frame();
    const int direction = selectionPointer_.y < bounds.y ? 1
        : selectionPointer_.y >= bounds.y + bounds.height ? -1 : 0;
    if (!direction || (direction > 0 ? scrollbackOffset_ >= historyRows_ : scrollbackOffset_ == 0)) return false;
    if (selectionScrollPending_ || (selectionScrollLastMs_ > 0.0 && nowMs - selectionScrollLastMs_ < 50.0)) return true;
    selectionScrollLastMs_ = nowMs;
    // One row per acknowledged frame preserves all text even with a slow PTY.
    selectionScrollPending_ = true;
    onScroll_(direction);
    return true;
}

} // namespace oneui
