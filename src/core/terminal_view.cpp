#include "oneui/controls/terminal_view.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <limits>
#include <utility>

namespace oneui {
namespace {

constexpr std::size_t kMaximumCells = 2'000'000;
constexpr unsigned int kVirtualKeyInsert = 0x2D;
constexpr unsigned int kVirtualKeyShift = 0x10;
constexpr unsigned int kVirtualKeyControl = 0x11;
constexpr unsigned int kVirtualKeyAlt = 0x12;
constexpr unsigned int kVirtualKeyLeftWin = 0x5B;
constexpr unsigned int kVirtualKeyRightWin = 0x5C;
constexpr double kCursorBlinkPeriodMs = 1060.0;
constexpr double kCursorBlinkOnMs = 530.0;
constexpr double kSlowTextBlinkPeriodMs = 1000.0;
constexpr double kSlowTextBlinkOnMs = 500.0;
constexpr double kRapidTextBlinkPeriodMs = 500.0;
constexpr double kRapidTextBlinkOnMs = 250.0;
constexpr double kMultiClickIntervalMs = 500.0;
constexpr std::size_t kMaximumSparseInvalidations = 32;

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

bool sameTerminalCell(const TerminalCell& left, const TerminalCell& right) {
    return left.text == right.text && sameColor(left.foreground, right.foreground) &&
           sameColor(left.background, right.background) && left.style == right.style &&
           left.hyperlinkId == right.hyperlinkId &&
           left.underlineStyle == right.underlineStyle &&
           sameColor(left.underlineColor, right.underlineColor) &&
           left.underlineColorSet == right.underlineColorSet;
}

bool isCopyShortcut(const KeyEvent& event) {
    return (event.control && event.shift && event.key == Key::C) ||
           (event.control && !event.shift && event.virtualKey == kVirtualKeyInsert);
}

bool isPasteShortcut(const KeyEvent& event) {
    return (event.control && event.shift && event.key == Key::V) ||
           (!event.control && event.shift && event.virtualKey == kVirtualKeyInsert);
}

bool isModifierKey(const KeyEvent& event) {
    return event.virtualKey == kVirtualKeyShift || event.virtualKey == kVirtualKeyControl ||
           event.virtualKey == kVirtualKeyAlt || event.virtualKey == kVirtualKeyLeftWin ||
           event.virtualKey == kVirtualKeyRightWin;
}

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

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

} // namespace

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

void TerminalView::setRightButtonAction(TerminalAuxiliaryButtonAction action) {
    rightButtonAction_ = action;
}

void TerminalView::setMiddleButtonAction(TerminalAuxiliaryButtonAction action) {
    middleButtonAction_ = action;
}

void TerminalView::setOnHyperlink(HyperlinkCallback callback) {
    onHyperlink_ = std::move(callback);
    pressedHyperlink_ = 0;
}

void TerminalView::setOnViewportChanged(ViewportChangedCallback callback) {
    onViewportChanged_ = std::move(callback);
    viewport_ = {};
    invalidate();
}

void TerminalView::setOnFocusChanged(FocusChangedCallback callback) {
    onFocusChanged_ = std::move(callback);
}

void TerminalView::setAnimationScheduler(std::function<void()> scheduler) {
    Widget::setAnimationScheduler(std::move(scheduler));
    if ((focused() && cursorBlinking_ && cursor_.visible) || hasBlinkingText()) {
        requestAnimationFrame();
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
    lastMetrics_ = metrics;
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
        static_cast<int>(std::floor((paintBounds.x - bounds.x) / metrics.cellWidth)),
        0,
        static_cast<int>(columns_ - 1)));
    const auto lastColumn = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::ceil(
            (paintBounds.x + paintBounds.width - bounds.x) / metrics.cellWidth)),
        static_cast<int>(firstColumn + 1),
        static_cast<int>(columns_)));

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
        if (style.cursor && cursorStyle_ == TerminalCursorStyle::Block) {
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
            bounds.x + metrics.cellWidth * static_cast<float>(column),
            bounds.y + metrics.cellHeight * static_cast<float>(row),
            metrics.cellWidth * static_cast<float>(span),
            metrics.cellHeight,
        };
        if (!sameColor(style.background, background_) ||
            (style.cursor && cursorStyle_ == TerminalCursorStyle::Block)) {
            canvas.fillRect(runRect, style.background);
        }
        if (style.textVisible && !text.empty()) {
            canvas.drawTextStyledWithNamedFont(
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
        if (style.cursor && cursorStyle_ == TerminalCursorStyle::Bar) {
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
            while (end < lastColumn) {
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
}

bool TerminalView::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position) || rows_ == 0 || columns_ == 0) {
        return false;
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
    if (!isModifierKey(event) && hasSelection()) {
        clearSelection();
    }
    if (onRawKey_) {
        onRawKey_(event);
    }
    restartCursorBlink();
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
    if (hasSelection()) {
        clearSelection();
    }
    if (onTextInput_) {
        onTextInput_(std::wstring(1, character));
    }
    restartCursorBlink();
    return true;
}

bool TerminalView::onTextInputText(const std::wstring& text) {
    if (!interactive() || text.empty()) {
        return false;
    }
    if (hasSelection()) {
        clearSelection();
    }
    if (onTextInput_) {
        onTextInput_(text);
    }
    restartCursorBlink();
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

bool TerminalView::onFocusChanged(bool focused) {
    if (!Widget::onFocusChanged(focused)) {
        return false;
    }
    if (focused) {
        restartCursorBlink();
    } else if (!cursorBlinkVisible_) {
        cursorBlinkVisible_ = true;
        invalidateCell(TextPosition{cursor_.row, cursor_.column});
    }
    if (onFocusChanged_) {
        onFocusChanged_(focused);
    }
    return true;
}

bool TerminalView::isFocusable() const {
    return interactive();
}

CursorKind TerminalView::cursor(Point point) const {
    if (!contains(point)) {
        return CursorKind::Default;
    }
    if (hyperlinkAt(point) != 0) {
        return CursorKind::Pointer;
    }
    return !mouseReporting_ ? CursorKind::Text : CursorKind::Default;
}

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
    const float measured = canvas.measureTextWidthWithNamedFont(
        L"M", fontSize_, fontFamily_, TextFontFamily::Monospace, 400);
    return GridMetrics{
        std::max(1.0f, measured),
        std::max(fontSize_ * lineHeight_, fontSize_ + 2.0f),
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
        bounds.x + width * static_cast<float>(position.column),
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
    const float width = std::max(1.0f, lastMetrics_.cellWidth);
    const float height = std::max(1.0f, lastMetrics_.cellHeight);

    if (firstRow == lastRow) {
        invalidateRect(Rect{
            bounds.x + width * static_cast<float>(firstColumn),
            bounds.y + height * static_cast<float>(firstRow),
            width * static_cast<float>(lastColumn - firstColumn + 1),
            height,
        });
        return;
    }

    invalidateRect(Rect{
        bounds.x + width * static_cast<float>(firstColumn),
        bounds.y + height * static_cast<float>(firstRow),
        width * static_cast<float>(columns_ - firstColumn),
        height,
    });
    if (lastRow > firstRow + 1) {
        invalidateRect(Rect{
            bounds.x,
            bounds.y + height * static_cast<float>(firstRow + 1),
            width * static_cast<float>(columns_),
            height * static_cast<float>(lastRow - firstRow - 1),
        });
    }
    invalidateRect(Rect{
        bounds.x,
        bounds.y + height * static_cast<float>(lastRow),
        width * static_cast<float>(lastColumn + 1),
        height,
    });
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
    return selecting_ || reportedButton_ != MouseButton::None || pressedHyperlink_ != 0;
}

void TerminalView::resetInteractionState() {
    selecting_ = false;
    reportedButton_ = MouseButton::None;
    pressedHyperlink_ = 0;
    setHoveredHyperlink(0);
}

} // namespace oneui
