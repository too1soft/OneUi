#include "terminal_view_helpers.h"

namespace oneui {
using namespace terminal_detail;

void TerminalView::setScrollback(std::uint64_t historyRows, std::uint64_t offset) {
    historyRows = std::min<std::uint64_t>(historyRows, 1000000000);
    offset = std::min(draggingScrollback_ ? scrollbackOffset_ : offset, historyRows);
    if (historyRows_ == historyRows && scrollbackOffset_ == offset) return;
    historyRows_ = historyRows;
    scrollbackOffset_ = offset;
    if (!historyRows_) draggingScrollback_ = false;
    invalidate();
}

Rect TerminalView::scrollbackThumbRect() const {
    const auto r = frame();
    if (!historyRows_ || r.height <= 0.0f || r.width < 10.0f) return {};
    const float h = std::min(r.height, std::max(20.0f, static_cast<float>(r.height * rows_ / (historyRows_ + static_cast<double>(rows_)))));
    const float progress = 1.0f - static_cast<double>(scrollbackOffset_) / historyRows_;
    return {r.x + r.width - 9.0f, r.y + progress * (r.height - h), 7.0f, h};
}

void TerminalView::dragScrollback(float y) {
    const auto thumb = scrollbackThumbRect();
    const float travel = frame().height - thumb.height;
    if (travel <= 0.0f || !historyRows_) return;
    const double position = std::clamp((y - frame().y - scrollbackGrab_) / travel, 0.0f, 1.0f);
    const auto target = static_cast<std::uint64_t>(std::llround((1.0 - position) * historyRows_));
    const int delta = static_cast<int>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(scrollbackOffset_));
    if (delta && onScroll_) {
        // Publish the requested offset immediately so consecutive drag events
        // don't duplicate deltas while a background session posts its frame.
        scrollbackOffset_ = target;
        invalidate();
        onScroll_(delta);
    }
}

bool TerminalView::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position) || rows_ == 0 || columns_ == 0) {
        return false;
    }
    const auto thumb = scrollbackThumbRect();
    if (event.button == MouseButton::Left && thumb.height > 0.0f &&
        event.position.x >= frame().x + frame().width - 10.0f && onScroll_) {
        draggingScrollback_ = true;
        scrollbackGrab_ = event.position.y >= thumb.y && event.position.y <= thumb.y + thumb.height
            ? event.position.y - thumb.y : thumb.height * .5f;
        dragScrollback(event.position.y);
        return true;
    }
    const std::uint32_t hyperlinkId = hyperlinkAt(event.position);
    if (event.button == MouseButton::Left && event.control && hyperlinkId != 0 && onHyperlink_) {
        pressedHyperlink_ = hyperlinkId;
        return true;
    }
    if (mouseReporting_ && !event.shift && onPointer_) {
        reportedButton_ = event.button;
        reportPointer(
            TerminalPointerAction::Press,
            event.button,
            event.position,
            0,
            event.shift,
            event.control,
            event.alt);
        return true;
    }
    if (handleAuxiliaryButton(event)) {
        return true;
    }
    if (event.button != MouseButton::Left) {
        return false;
    }

    const SelectionSnapshot previous = selectionSnapshot();
    const TextPosition cellPosition = normalizedCellPosition(cellPositionFromPoint(event.position));
    const double nowMs = currentTimeMs();
    if (cellPosition == lastClickPosition_ && nowMs - lastClickMs_ <= kMultiClickIntervalMs) {
        clickCount_ = clickCount_ >= 3 ? 1 : clickCount_ + 1;
    } else {
        clickCount_ = 1;
    }
    lastClickPosition_ = cellPosition;
    lastClickMs_ = nowMs;

    if (event.shift && hasSelection()) {
        selectionMode_ = SelectionMode::Character;
        selectionCaret_ = positionFromPoint(event.position);
        hasSelection_ = !(selectionAnchor_ == selectionCaret_);
    } else if (clickCount_ == 2) {
        selectionMode_ = SelectionMode::Word;
        selectWordAt(cellPosition);
    } else if (clickCount_ == 3) {
        selectionMode_ = SelectionMode::Line;
        selectLineAt(cellPosition.row);
    } else {
        selectionMode_ = SelectionMode::Character;
        selectionAnchor_ = positionFromPoint(event.position);
        selectionCaret_ = selectionAnchor_;
        selectionOriginStart_ = selectionAnchor_;
        selectionOriginEnd_ = selectionAnchor_;
        hasSelection_ = true;
    }
    selecting_ = true;
    invalidateSelectionDelta(previous);
    return true;
}

bool TerminalView::handleAuxiliaryButton(const MouseEvent& event) {
    const TerminalAuxiliaryButtonAction action = event.button == MouseButton::Right
        ? rightButtonAction_
        : (event.button == MouseButton::Middle
               ? middleButtonAction_
               : TerminalAuxiliaryButtonAction::Ignore);
    switch (action) {
    case TerminalAuxiliaryButtonAction::Copy:
        return copySelectionToClipboard();
    case TerminalAuxiliaryButtonAction::Paste:
        return pasteFromClipboard();
    case TerminalAuxiliaryButtonAction::Callback:
        if (!onPointer_) {
            return false;
        }
        reportPointer(
            TerminalPointerAction::Press,
            event.button,
            event.position,
            0,
            event.shift,
            event.control,
            event.alt);
        return true;
    case TerminalAuxiliaryButtonAction::Ignore:
        return false;
    }
    return false;
}

bool TerminalView::onMouseMove(const MouseEvent& event) {
    if (draggingScrollback_) { dragScrollback(event.position.y); return true; }
    const std::uint32_t hyperlinkId = interactive() && contains(event.position)
                                          ? hyperlinkAt(event.position)
                                          : 0;
    const bool hyperlinkChanged = hyperlinkId != hoveredHyperlink_;
    if (hyperlinkChanged) {
        setHoveredHyperlink(hyperlinkId);
    }
    if (selecting_) {
        const SelectionSnapshot previous = selectionSnapshot();
        const TextPosition previousAnchor = selectionAnchor_;
        const TextPosition previousCaret = selectionCaret_;
        switch (selectionMode_) {
        case SelectionMode::Word:
            extendWordSelection(cellPositionFromPoint(event.position));
            break;
        case SelectionMode::Line:
            extendLineSelection(cellPositionFromPoint(event.position).row);
            break;
        case SelectionMode::Character:
            selectionCaret_ = positionFromPoint(event.position);
            break;
        }
        if (previousAnchor == selectionAnchor_ && previousCaret == selectionCaret_) {
            return hyperlinkChanged;
        }
        hasSelection_ = !(selectionAnchor_ == selectionCaret_);
        invalidateSelectionDelta(previous);
        return true;
    }
    if (mouseReporting_ && onPointer_ && contains(event.position) &&
        (reportedButton_ != MouseButton::None || !event.shift)) {
        reportPointer(
            TerminalPointerAction::Move,
            reportedButton_,
            event.position,
            0,
            event.shift,
            event.control,
            event.alt);
        return true;
    }
    return hyperlinkChanged;
}

bool TerminalView::onMouseUp(const MouseEvent& event) {
    if (draggingScrollback_) { dragScrollback(event.position.y); draggingScrollback_ = false; return true; }
    if (pressedHyperlink_ != 0) {
        const std::uint32_t hyperlinkId = pressedHyperlink_;
        pressedHyperlink_ = 0;
        if (event.button == MouseButton::Left && hyperlinkAt(event.position) == hyperlinkId &&
            onHyperlink_) {
            onHyperlink_(hyperlinkId);
        }
        return true;
    }
    if (selecting_) {
        selecting_ = false;
        if (selectionAnchor_ == selectionCaret_) {
            hasSelection_ = false;
        }
        if (copyOnSelect_ && hasSelection()) {
            copySelectionToClipboard();
        }
        return true;
    }
    if (mouseReporting_ && onPointer_ &&
        (reportedButton_ != MouseButton::None || !event.shift)) {
        reportPointer(
            TerminalPointerAction::Release,
            event.button,
            event.position,
            0,
            event.shift,
            event.control,
            event.alt);
        reportedButton_ = MouseButton::None;
        return true;
    }
    return false;
}

bool TerminalView::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !contains(event.position)) {
        return false;
    }

    if (mouseReporting_ && !event.shift && onPointer_) {
        pointerWheelRemainder_ += std::clamp(event.deltaY, -100.0f, 100.0f);
        const int steps = static_cast<int>(pointerWheelRemainder_);
        if (steps != 0) {
            pointerWheelRemainder_ -= static_cast<float>(steps);
            reportPointer(
                TerminalPointerAction::Wheel,
                MouseButton::None,
                event.position,
                steps,
                event.shift,
                event.control,
                event.alt);
        }
        return true;
    }
    if (!onScroll_) {
        return false;
    }

    wheelRowRemainder_ += std::clamp(event.deltaY, -100.0f, 100.0f) * scrollRowsPerWheel_;
    const int rows = static_cast<int>(wheelRowRemainder_);
    if (rows == 0) {
        return true;
    }
    wheelRowRemainder_ -= static_cast<float>(rows);
    clearSelection();
    onScroll_(rows);
    return true;
}

void TerminalView::reportPointer(
    TerminalPointerAction action,
    MouseButton button,
    Point point,
    int wheelDelta,
    bool shift,
    bool control,
    bool alt) {
    if (!onPointer_) {
        return;
    }
    const TextPosition position = cellPositionFromPoint(point);
    onPointer_(TerminalPointerEvent{
        action,
        button,
        point,
        position.row,
        position.column,
        wheelDelta,
        shift,
        control,
        alt,
    });
}

std::uint32_t TerminalView::hyperlinkAt(Point point) const {
    if (!contains(point) || rows_ == 0 || columns_ == 0) {
        return 0;
    }
    const TextPosition position = normalizedCellPosition(cellPositionFromPoint(point));
    const TerminalCell* cell = cellAt(position.row, position.column);
    return cell ? cell->hyperlinkId : 0;
}

void TerminalView::setHoveredHyperlink(std::uint32_t hyperlinkId) {
    if (hyperlinkId == hoveredHyperlink_) {
        return;
    }
    const std::uint32_t previous = hoveredHyperlink_;
    hoveredHyperlink_ = hyperlinkId;
    invalidateHyperlink(previous);
    invalidateHyperlink(hyperlinkId);
}

void TerminalView::invalidateHyperlink(std::uint32_t hyperlinkId) {
    if (hyperlinkId == 0 || rows_ == 0 || columns_ == 0) {
        return;
    }
    for (std::uint16_t row = 0; row < rows_; ++row) {
        std::uint16_t column = 0;
        while (column < columns_) {
            while (column < columns_ &&
                   cells_[cellIndex(row, column)].hyperlinkId != hyperlinkId) {
                ++column;
            }
            const std::uint16_t first = column;
            while (column < columns_ &&
                   cells_[cellIndex(row, column)].hyperlinkId == hyperlinkId) {
                ++column;
            }
            if (first < column) {
                invalidateCellRange(cellIndex(row, first), column - first);
            }
        }
    }
}

bool TerminalView::hasInteractionState() const {
    return draggingScrollback_ || selecting_ || reportedButton_ != MouseButton::None || pressedHyperlink_ != 0;
}

void TerminalView::resetInteractionState() {
    draggingScrollback_ = false;
    selecting_ = false;
    reportedButton_ = MouseButton::None;
    pressedHyperlink_ = 0;
    setHoveredHyperlink(0);
}

} // namespace oneui
