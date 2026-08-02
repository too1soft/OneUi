#include "oneui/controls/terminal_view.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace oneui {
namespace {

constexpr std::size_t kMaximumCells = 2'000'000;
constexpr unsigned int kVirtualKeyInsert = 0x2D;

bool hasStyle(const TerminalCell& cell, TerminalCellStyle style) {
    return (cell.style & static_cast<std::uint32_t>(style)) != 0;
}

Color dim(Color color) {
    color.r = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.r) * 0.62f));
    color.g = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.g) * 0.62f));
    color.b = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.b) * 0.62f));
    return color;
}

bool sameColor(Color left, Color right) {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

bool isCopyShortcut(const KeyEvent& event) {
    return (event.control && event.shift && event.key == Key::C) ||
           (event.control && !event.shift && event.virtualKey == kVirtualKeyInsert);
}

bool isPasteShortcut(const KeyEvent& event) {
    return (event.control && event.shift && event.key == Key::V) ||
           (!event.control && event.shift && event.virtualKey == kVirtualKeyInsert);
}

} // namespace

TerminalView::TerminalView() {
    setPreferredSize(Size{640.0f, 400.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"Terminal");
    setAccessibleDescription(L"Interactive terminal output");
}

void TerminalView::setGrid(std::uint16_t rows, std::uint16_t columns, std::vector<TerminalCell> cells) {
    const bool dimensionsChanged = rows != rows_ || columns != columns_;
    const std::size_t requested = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
    if (requested > kMaximumCells) {
        rows = 0;
        columns = 0;
        cells.clear();
    } else {
        cells.resize(requested);
    }

    rows_ = rows;
    columns_ = columns;
    cells_ = std::move(cells);
    if (dimensionsChanged) {
        clearSelection();
    }
    if (cursor_.row >= rows_ || cursor_.column >= columns_) {
        cursor_.visible = false;
    }
    invalidate();
}

void TerminalView::updateCells(std::size_t firstCell, std::vector<TerminalCell> cells) {
    if (cells.empty() || firstCell >= cells_.size()) {
        return;
    }

    const std::size_t count = std::min(cells.size(), cells_.size() - firstCell);
    std::move(cells.begin(), cells.begin() + count, cells_.begin() + firstCell);
    invalidate();
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
    cursor_ = cursor;
    if (cursor_.row >= rows_ || cursor_.column >= columns_) {
        cursor_.visible = false;
    }
    invalidate();
}

TerminalCursor TerminalView::cursorState() const {
    return cursor_;
}

void TerminalView::setFontSize(float size) {
    const float clamped = std::max(6.0f, std::min(size, 72.0f));
    if (fontSize_ == clamped) {
        return;
    }
    fontSize_ = clamped;
    invalidate();
}

float TerminalView::fontSize() const {
    return fontSize_;
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
    invalidate();
}

void TerminalView::setClipboard(std::shared_ptr<Clipboard> clipboard) {
    clipboard_ = std::move(clipboard);
}

void TerminalView::selectAll() {
    if (rows_ == 0 || columns_ == 0) {
        return;
    }
    selectionAnchor_ = TextPosition{0, 0};
    selectionCaret_ = TextPosition{
        static_cast<std::uint16_t>(rows_ - 1),
        columns_,
    };
    hasSelection_ = true;
    selecting_ = false;
    invalidate();
}

void TerminalView::clearSelection() {
    selectionAnchor_ = {};
    selectionCaret_ = {};
    hasSelection_ = false;
    selecting_ = false;
    invalidate();
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
    clipboard_->setText(selectedText());
    return true;
}

bool TerminalView::pasteFromClipboard() {
    if (!clipboard_) {
        return false;
    }
    const std::wstring text = clipboard_->text();
    if (text.empty()) {
        return false;
    }
    if (onPaste_) {
        onPaste_(text);
    } else if (onTextInput_) {
        onTextInput_(text);
    } else {
        return false;
    }
    return true;
}

void TerminalView::setOnTextInput(TextInputCallback callback) {
    onTextInput_ = std::move(callback);
}

void TerminalView::setOnPaste(PasteCallback callback) {
    onPaste_ = std::move(callback);
}

void TerminalView::setOnRawKey(RawKeyCallback callback) {
    onRawKey_ = std::move(callback);
}

void TerminalView::setOnScroll(ScrollCallback callback) {
    onScroll_ = std::move(callback);
    wheelRowRemainder_ = 0.0f;
}

void TerminalView::setOnPointer(PointerCallback callback) {
    onPointer_ = std::move(callback);
    reportedButton_ = MouseButton::None;
    pointerWheelRemainder_ = 0.0f;
}

void TerminalView::setOnViewportChanged(ViewportChangedCallback callback) {
    onViewportChanged_ = std::move(callback);
    viewport_ = {};
    invalidate();
}

void TerminalView::paint(Canvas& canvas) {
    const Rect bounds = frame();
    canvas.fillRect(bounds, background_);
    if (rows_ == 0 || columns_ == 0) {
        return;
    }

    const GridMetrics metrics = gridMetrics(canvas);
    lastMetrics_ = metrics;
    reportViewport(bounds, metrics);
    canvas.save();
    canvas.clipRect(bounds);

    struct PaintStyle {
        Color foreground;
        Color background;
        int weight = 400;
        bool underline = false;
        bool cursor = false;
    };

    const auto styleFor = [&](const TerminalCell& cell, std::uint16_t row, std::uint16_t column) {
        PaintStyle style{cell.foreground, cell.background};
        if (hasStyle(cell, TerminalCellInverse)) {
            std::swap(style.foreground, style.background);
        }
        if (hasStyle(cell, TerminalCellDim)) {
            style.foreground = dim(style.foreground);
        }
        if (cellSelected(row, column)) {
            style.background = selectionBackground_;
        }
        style.cursor = cursor_.visible && cursor_.row == row && cursor_.column == column;
        if (style.cursor) {
            style.background = cursorColor_;
            style.foreground = background_;
        }
        style.weight = hasStyle(cell, TerminalCellBold) ? 700 : 400;
        style.underline = hasStyle(cell, TerminalCellUnderline);
        return style;
    };

    const auto samePaintStyle = [&](const PaintStyle& left, const PaintStyle& right) {
        return sameColor(left.foreground, right.foreground) &&
               sameColor(left.background, right.background) &&
               left.weight == right.weight &&
               left.underline == right.underline &&
               left.cursor == right.cursor;
    };

    const auto paintTextRun = [&](std::uint16_t row,
                                  std::uint16_t column,
                                  std::uint16_t span,
                                  const std::wstring& text,
                                  const PaintStyle& style) {
        const Rect runRect{
            bounds.x + metrics.cellWidth * static_cast<float>(column),
            bounds.y + metrics.cellHeight * static_cast<float>(row),
            metrics.cellWidth * static_cast<float>(span),
            metrics.cellHeight,
        };
        if (!sameColor(style.background, background_) || style.cursor) {
            canvas.fillRect(runRect, style.background);
        }
        if (!text.empty()) {
            canvas.drawTextStyledWithFont(
                text,
                runRect,
                style.foreground,
                fontSize_,
                TextAlign::Left,
                TextFontFamily::Monospace,
                style.weight);
        }
        if (style.underline) {
            const float underlineY = runRect.y + std::max(1.0f, runRect.height - 2.0f);
            canvas.drawLine(
                Point{runRect.x + 1.0f, underlineY},
                Point{runRect.x + std::max(1.0f, runRect.width - 1.0f), underlineY},
                style.foreground,
                1.0f);
        }
    };

    for (std::uint16_t row = 0; row < rows_; ++row) {
        for (std::uint16_t column = 0; column < columns_;) {
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

            std::wstring run = cell->text;
            std::uint16_t end = static_cast<std::uint16_t>(column + 1);
            while (end < columns_) {
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
                run += next->text;
                ++end;
            }
            paintTextRun(row, column, static_cast<std::uint16_t>(end - column), run, style);
            column = end;
        }
    }
    canvas.restore();
}

bool TerminalView::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position) || rows_ == 0 || columns_ == 0) {
        return false;
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
    if (event.button != MouseButton::Left) {
        return false;
    }
    selectionAnchor_ = positionFromPoint(event.position);
    selectionCaret_ = selectionAnchor_;
    hasSelection_ = true;
    selecting_ = true;
    invalidate();
    return true;
}

bool TerminalView::onMouseMove(const MouseEvent& event) {
    if (selecting_) {
        const TextPosition next = positionFromPoint(event.position);
        if (next == selectionCaret_) {
            return false;
        }
        selectionCaret_ = next;
        invalidate();
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
    return false;
}

bool TerminalView::onMouseUp(const MouseEvent& event) {
    if (selecting_) {
        selecting_ = false;
        if (selectionAnchor_ == selectionCaret_) {
            hasSelection_ = false;
            invalidate();
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

bool TerminalView::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (isCopyShortcut(event)) {
        copySelectionToClipboard();
        return true;
    }
    if (isPasteShortcut(event)) {
        pasteFromClipboard();
        return true;
    }
    if (onRawKey_) {
        onRawKey_(event);
    }
    return true;
}

bool TerminalView::onKeyUp(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (isCopyShortcut(event) || isPasteShortcut(event)) {
        return true;
    }
    KeyEvent released = event;
    released.pressed = false;
    released.repeat = false;
    if (onRawKey_) {
        onRawKey_(released);
    }
    return true;
}

bool TerminalView::onTextInput(wchar_t character) {
    if (!interactive() || character == L'\0') {
        return false;
    }
    if (onTextInput_) {
        onTextInput_(std::wstring(1, character));
    }
    return true;
}

bool TerminalView::onTextInputText(const std::wstring& text) {
    if (!interactive() || text.empty()) {
        return false;
    }
    if (onTextInput_) {
        onTextInput_(text);
    }
    return true;
}

Rect TerminalView::textInputCaretRect() const {
    const Rect bounds = frame();
    const float cellWidth = std::max(1.0f, lastMetrics_.cellWidth);
    const float cellHeight = std::max(1.0f, lastMetrics_.cellHeight);
    const float x = bounds.x + cellWidth * static_cast<float>(cursor_.column);
    const float y = bounds.y + cellHeight * static_cast<float>(cursor_.row);
    return Rect{x, y, cellWidth, cellHeight};
}

bool TerminalView::isFocusable() const {
    return interactive();
}

CursorKind TerminalView::cursor(Point point) const {
    return contains(point) && !mouseReporting_ ? CursorKind::Text : CursorKind::Default;
}

AccessibilityInfo TerminalView::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::Custom;
    }
    if (info.name.empty()) {
        info.name = L"Terminal";
    }
    info.state.readOnly = !onTextInput_ && !onRawKey_;
    return info;
}

TerminalView::GridMetrics TerminalView::gridMetrics(const Canvas& canvas) const {
    const float measured = canvas.measureTextWidthWithFont(
        L"M", fontSize_, TextFontFamily::Monospace, 400);
    return GridMetrics{
        std::max(1.0f, measured),
        std::max(fontSize_ * 1.30f, fontSize_ + 2.0f),
    };
}

void TerminalView::reportViewport(Rect bounds, GridMetrics metrics) {
    const auto columns = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor(bounds.width / std::max(1.0f, metrics.cellWidth))),
        1,
        static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    const auto rows = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor(bounds.height / std::max(1.0f, metrics.cellHeight))),
        1,
        static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    const TerminalViewport viewport{rows, columns};
    if (viewport.rows == viewport_.rows && viewport.columns == viewport_.columns) {
        return;
    }
    viewport_ = viewport;
    if (onViewportChanged_) {
        onViewportChanged_(viewport);
    }
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

TerminalView::TextPosition TerminalView::positionFromPoint(Point point) const {
    if (rows_ == 0 || columns_ == 0) {
        return {};
    }
    const Rect bounds = frame();
    const float localX = std::max(0.0f, point.x - bounds.x);
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
        static_cast<int>(std::floor((point.x - bounds.x) / cellWidth)),
        0,
        static_cast<int>(columns_ - 1)));
    return TextPosition{row, column};
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
        position.row,
        position.column,
        wheelDelta,
        shift,
        control,
        alt,
    });
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

bool TerminalView::hasInteractionState() const {
    return selecting_ || reportedButton_ != MouseButton::None;
}

void TerminalView::resetInteractionState() {
    selecting_ = false;
    reportedButton_ = MouseButton::None;
}

} // namespace oneui
