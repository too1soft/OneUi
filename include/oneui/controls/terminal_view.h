#pragma once

#include "oneui/color.h"
#include "oneui/export.h"
#include "oneui/widget.h"

#include <cstdint>
#include <functional>
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

/// The number of whole terminal cells that fit in the current viewport.
struct TerminalViewport {
    std::uint16_t rows = 1;
    std::uint16_t columns = 1;
};

// TerminalView is an ANSI-terminal renderer, not a log viewer. The terminal
// emulator owns escape-sequence parsing and supplies a structured cell grid;
// this control owns pixel rendering, focus, and native input delivery.
class ONEUI_API TerminalView final : public Widget {
public:
    using TextInputCallback = std::function<void(const std::wstring&)>;
    using RawKeyCallback = std::function<void(const KeyEvent&)>;
    using ViewportChangedCallback = std::function<void(TerminalViewport)>;

    TerminalView();

    void setGrid(std::uint16_t rows, std::uint16_t columns, std::vector<TerminalCell> cells);
    Size gridSize() const;
    std::size_t cellCount() const;
    const TerminalCell* cellAt(std::uint16_t row, std::uint16_t column) const;

    void setCursor(TerminalCursor cursor);
    TerminalCursor cursorState() const;
    void setFontSize(float size);
    float fontSize() const;
    void setPalette(Color background, Color foreground, Color cursor);

    void setOnTextInput(TextInputCallback callback);
    void setOnRawKey(RawKeyCallback callback);
    void setOnViewportChanged(ViewportChangedCallback callback);

    void paint(Canvas& canvas) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onKeyUp(const KeyEvent& event) override;
    bool onTextInput(wchar_t character) override;
    bool isFocusable() const override;
    CursorKind cursor(Point point) const override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    struct GridMetrics {
        float cellWidth = 1.0f;
        float cellHeight = 1.0f;
    };

    GridMetrics gridMetrics(const Canvas& canvas) const;
    void reportViewport(Rect bounds, GridMetrics metrics);
    std::size_t cellIndex(std::uint16_t row, std::uint16_t column) const;

    std::uint16_t rows_ = 0;
    std::uint16_t columns_ = 0;
    std::vector<TerminalCell> cells_;
    TerminalCursor cursor_;
    float fontSize_ = 14.0f;
    Color background_{20, 24, 36, 255};
    Color foreground_{220, 226, 240, 255};
    Color cursorColor_{196, 181, 253, 255};
    TextInputCallback onTextInput_;
    RawKeyCallback onRawKey_;
    ViewportChangedCallback onViewportChanged_;
    TerminalViewport viewport_{};
};

} // namespace oneui
