#include "oneui/controls/terminal_view.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace oneui {
namespace {

constexpr std::size_t kMaximumCells = 2'000'000;

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

} // namespace

TerminalView::TerminalView() {
    setPreferredSize(Size{640.0f, 400.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"Terminal");
    setAccessibleDescription(L"Interactive terminal output");
}

void TerminalView::setGrid(std::uint16_t rows, std::uint16_t columns, std::vector<TerminalCell> cells) {
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
    if (cursor_.row >= rows_ || cursor_.column >= columns_) {
        cursor_.visible = false;
    }
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

void TerminalView::setOnTextInput(TextInputCallback callback) {
    onTextInput_ = std::move(callback);
}

void TerminalView::setOnRawKey(RawKeyCallback callback) {
    onRawKey_ = std::move(callback);
}

void TerminalView::paint(Canvas& canvas) {
    const Rect bounds = frame();
    canvas.fillRect(bounds, background_);
    if (rows_ == 0 || columns_ == 0) {
        return;
    }

    const GridMetrics metrics = gridMetrics(canvas);
    canvas.save();
    canvas.clipRect(bounds);

    for (std::uint16_t row = 0; row < rows_; ++row) {
        for (std::uint16_t column = 0; column < columns_; ++column) {
            const TerminalCell* cell = cellAt(row, column);
            if (!cell || hasStyle(*cell, TerminalCellWideContinuation)) {
                continue;
            }

            const bool isCursor = cursor_.visible && cursor_.row == row && cursor_.column == column;
            const bool wide = hasStyle(*cell, TerminalCellWide);
            const float width = metrics.cellWidth * (wide ? 2.0f : 1.0f);
            const Rect cellRect{
                bounds.x + metrics.cellWidth * static_cast<float>(column),
                bounds.y + metrics.cellHeight * static_cast<float>(row),
                width,
                metrics.cellHeight,
            };

            Color foreground = cell->foreground;
            Color background = cell->background;
            if (hasStyle(*cell, TerminalCellInverse)) {
                std::swap(foreground, background);
            }
            if (hasStyle(*cell, TerminalCellDim)) {
                foreground = dim(foreground);
            }
            if (isCursor) {
                background = cursorColor_;
                foreground = background_;
            }
            if (!sameColor(background, background_)) {
                canvas.fillRect(cellRect, background);
            } else if (isCursor) {
                canvas.fillRect(cellRect, background);
            }
            if (!cell->text.empty()) {
                const int weight = hasStyle(*cell, TerminalCellBold) ? 700 : 400;
                canvas.drawTextStyled(cell->text, cellRect, foreground, fontSize_, TextAlign::Left, weight);
            }
            if (hasStyle(*cell, TerminalCellUnderline)) {
                const float underlineY = cellRect.y + std::max(1.0f, cellRect.height - 2.0f);
                canvas.drawLine(
                    Point{cellRect.x + 1.0f, underlineY},
                    Point{cellRect.x + std::max(1.0f, cellRect.width - 1.0f), underlineY},
                    foreground,
                    1.0f);
            }
        }
    }
    canvas.restore();
}

bool TerminalView::onMouseDown(const MouseEvent& event) {
    return interactive() && contains(event.position);
}

bool TerminalView::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
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

bool TerminalView::isFocusable() const {
    return interactive();
}

CursorKind TerminalView::cursor(Point point) const {
    return contains(point) ? CursorKind::Text : CursorKind::Default;
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
    const float measured = canvas.measureTextWidth(L"M", fontSize_, 400);
    return GridMetrics{
        std::max(1.0f, measured),
        std::max(fontSize_ * 1.30f, fontSize_ + 2.0f),
    };
}

std::size_t TerminalView::cellIndex(std::uint16_t row, std::uint16_t column) const {
    if (row >= rows_ || column >= columns_) {
        return std::numeric_limits<std::size_t>::max();
    }
    return static_cast<std::size_t>(row) * columns_ + column;
}

} // namespace oneui
