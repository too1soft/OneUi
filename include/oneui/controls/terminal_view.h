#pragma once

#include "oneui/clipboard.h"
#include "oneui/color.h"
#include "oneui/export.h"
#include "oneui/widget.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oneui {

// Style flags are deliberately stable because the C ABI mirrors these values.
enum TerminalCellStyle : std::uint32_t {
    TerminalCellBold = 1u << 0,
    TerminalCellDim = 1u << 1,
    TerminalCellItalic = 1u << 2,
    TerminalCellUnderline = 1u << 3,
    TerminalCellInverse = 1u << 4,
    TerminalCellWide = 1u << 5,
    TerminalCellWideContinuation = 1u << 6,
};

struct TerminalCell {
    std::wstring text;
    Color foreground{220, 226, 240, 255};
    Color background{20, 24, 36, 255};
    std::uint32_t style = 0;
};

struct TerminalCursor {
    std::uint16_t row = 0;
    std::uint16_t column = 0;
    bool visible = true;
};

enum class TerminalCursorStyle {
    Block,
    Bar,
    Underline,
};

/// The number of whole terminal cells that fit in the current viewport.
struct TerminalViewport {
    std::uint16_t rows = 1;
    std::uint16_t columns = 1;
};

enum class TerminalPointerAction {
    Press,
    Release,
    Move,
    Wheel,
};

struct TerminalPointerEvent {
    TerminalPointerAction action = TerminalPointerAction::Move;
    MouseButton button = MouseButton::None;
    std::uint16_t row = 0;
    std::uint16_t column = 0;
    int wheelDelta = 0;
    bool shift = false;
    bool control = false;
    bool alt = false;
};

// TerminalView is an ANSI-terminal renderer, not a log viewer. The terminal
// emulator owns escape-sequence parsing and supplies a structured cell grid;
// this control owns pixel rendering, focus, and native input delivery.
class ONEUI_API TerminalView final : public Widget {
public:
    using TextInputCallback = std::function<void(const std::wstring&)>;
    using PasteCallback = std::function<void(const std::wstring&)>;
    using RawKeyCallback = std::function<void(const KeyEvent&)>;
    using ScrollCallback = std::function<void(int)>;
    using PointerCallback = std::function<void(const TerminalPointerEvent&)>;
    using ViewportChangedCallback = std::function<void(TerminalViewport)>;

    TerminalView();

    void setGrid(std::uint16_t rows, std::uint16_t columns, std::vector<TerminalCell> cells);
    void updateCells(std::size_t firstCell, std::vector<TerminalCell> cells);
    Size gridSize() const;
    std::size_t cellCount() const;
    const TerminalCell* cellAt(std::uint16_t row, std::uint16_t column) const;

    void setCursor(TerminalCursor cursor);
    TerminalCursor cursorState() const;
    void setCursorStyle(TerminalCursorStyle style);
    TerminalCursorStyle cursorStyle() const;
    void setCursorBlinking(bool enabled);
    bool cursorBlinking() const;
    void setFontSize(float size);
    float fontSize() const;
    void setLineHeight(float multiplier);
    float lineHeight() const;
    void setPalette(Color background, Color foreground, Color cursor);
    void setSelectionBackground(Color color);
    void setCopyOnSelect(bool enabled);
    bool copyOnSelect() const;
    void setScrollRowsPerWheel(float rows);
    void setMouseReporting(bool enabled);

    void setClipboard(std::shared_ptr<Clipboard> clipboard);
    void selectAll();
    void setSelection(
        std::uint16_t startRow,
        std::uint16_t startColumn,
        std::uint16_t endRow,
        std::uint16_t endColumn);
    void clearSelection();
    bool hasSelection() const;
    std::wstring selectedText() const;
    bool copySelectionToClipboard();
    bool pasteFromClipboard();

    void setOnTextInput(TextInputCallback callback);
    void setOnPaste(PasteCallback callback);
    void setOnRawKey(RawKeyCallback callback);
    void setOnScroll(ScrollCallback callback);
    void setOnPointer(PointerCallback callback);
    void setOnViewportChanged(ViewportChangedCallback callback);
    void setAnimationScheduler(std::function<void()> scheduler) override;

    void paint(Canvas& canvas) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onKeyUp(const KeyEvent& event) override;
    bool onTextInput(wchar_t character) override;
    bool onTextInputText(const std::wstring& text) override;
    Rect textInputCaretRect() const override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    CursorKind cursor(Point point) const override;
    bool tickAnimations(double nowMs) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    struct GridMetrics {
        float cellWidth = 1.0f;
        float cellHeight = 1.0f;
    };

    struct TextPosition {
        std::uint16_t row = 0;
        std::uint16_t column = 0;

        bool operator==(const TextPosition& other) const {
            return row == other.row && column == other.column;
        }
        bool operator<(const TextPosition& other) const {
            return row != other.row ? row < other.row : column < other.column;
        }
    };

    enum class SelectionMode {
        Character,
        Word,
        Line,
    };

    enum class CellWordClass {
        Whitespace,
        Word,
        Punctuation,
    };

    struct SelectionRange {
        TextPosition start;
        TextPosition end;
    };

    GridMetrics gridMetrics(const Canvas& canvas) const;
    void reportViewport(Rect bounds, GridMetrics metrics);
    std::size_t cellIndex(std::uint16_t row, std::uint16_t column) const;
    TextPosition selectionStart() const;
    TextPosition selectionEnd() const;
    TextPosition positionFromPoint(Point point) const;
    TextPosition cellPositionFromPoint(Point point) const;
    TextPosition normalizedCellPosition(TextPosition position) const;
    CellWordClass wordClassAt(TextPosition position) const;
    SelectionRange wordRangeAt(TextPosition position) const;
    void selectWordAt(TextPosition position);
    void selectLineAt(std::uint16_t row);
    void extendWordSelection(TextPosition position);
    void extendLineSelection(std::uint16_t row);
    void restartCursorBlink();
    bool cursorPaintVisible() const;
    Rect cellRect(TextPosition position) const;
    void invalidateCell(TextPosition position);
    void invalidateCellRange(std::size_t firstCell, std::size_t count);
    void reportPointer(
        TerminalPointerAction action,
        MouseButton button,
        Point point,
        int wheelDelta,
        bool shift,
        bool control,
        bool alt);
    bool cellSelected(std::uint16_t row, std::uint16_t column) const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::uint16_t rows_ = 0;
    std::uint16_t columns_ = 0;
    std::vector<TerminalCell> cells_;
    TerminalCursor cursor_;
    TerminalCursorStyle cursorStyle_ = TerminalCursorStyle::Block;
    bool cursorBlinking_ = true;
    bool cursorBlinkVisible_ = true;
    double cursorBlinkStartMs_ = 0.0;
    float fontSize_ = 14.0f;
    float lineHeight_ = 1.30f;
    Color background_{20, 24, 36, 255};
    Color foreground_{220, 226, 240, 255};
    Color cursorColor_{196, 181, 253, 255};
    Color selectionBackground_{69, 83, 144, 255};
    TextPosition selectionAnchor_{};
    TextPosition selectionCaret_{};
    TextPosition selectionOriginStart_{};
    TextPosition selectionOriginEnd_{};
    TextPosition lastClickPosition_{};
    SelectionMode selectionMode_ = SelectionMode::Character;
    int clickCount_ = 0;
    double lastClickMs_ = 0.0;
    bool selecting_ = false;
    bool hasSelection_ = false;
    bool copyOnSelect_ = false;
    std::shared_ptr<Clipboard> clipboard_;
    TextInputCallback onTextInput_;
    PasteCallback onPaste_;
    RawKeyCallback onRawKey_;
    ScrollCallback onScroll_;
    PointerCallback onPointer_;
    ViewportChangedCallback onViewportChanged_;
    TerminalViewport viewport_{};
    GridMetrics lastMetrics_{};
    bool hasGridMetrics_ = false;
    float scrollRowsPerWheel_ = 3.0f;
    float wheelRowRemainder_ = 0.0f;
    float pointerWheelRemainder_ = 0.0f;
    MouseButton reportedButton_ = MouseButton::None;
    bool mouseReporting_ = false;
};

} // namespace oneui
