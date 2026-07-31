#include "oneui/controls/terminal_view.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

class RecordingCanvas final : public oneui::Canvas {
public:
    struct TextCall {
        std::wstring text;
        oneui::Rect rect;
        oneui::Color color;
        int weight = 400;
    };

    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override { ++fillCount; }
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override { ++lineCount; }
    void drawText(const std::wstring& text, oneui::Rect rect, oneui::Color color, float, oneui::TextAlign = oneui::TextAlign::Center) override {
        texts.push_back(TextCall{text, rect, color});
    }
    void drawTextStyled(const std::wstring& text, oneui::Rect rect, oneui::Color color, float, oneui::TextAlign = oneui::TextAlign::Center, int weight = 400) override {
        texts.push_back(TextCall{text, rect, color, weight});
    }
    float measureTextWidth(const std::wstring&, float size, int = 400) const override { return size * 0.60f; }

    int fillCount = 0;
    int lineCount = 0;
    std::vector<TextCall> texts;
};

void expectEqual(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectNear(const char* name, float actual, float expected) {
    if (std::fabs(actual - expected) > 0.001f) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void testGridCopiesCellsAndCursor() {
    oneui::TerminalView terminal;
    terminal.setGrid(2, 3, {
        oneui::TerminalCell{L"A", {255, 0, 0, 255}, {0, 0, 0, 255}, oneui::TerminalCellBold},
        oneui::TerminalCell{L"中", {0, 255, 0, 255}, {0, 0, 0, 255}, oneui::TerminalCellWide},
        oneui::TerminalCell{L"", {0, 255, 0, 255}, {0, 0, 0, 255}, oneui::TerminalCellWideContinuation},
    });
    terminal.setCursor(oneui::TerminalCursor{1, 2, true});

    expectNear("terminal columns", terminal.gridSize().width, 3.0f);
    expectNear("terminal rows", terminal.gridSize().height, 2.0f);
    expectEqual("terminal fills omitted cells", static_cast<int>(terminal.cellCount()), 6);
    expectEqual("terminal first text", terminal.cellAt(0, 0)->text == L"A" ? 1 : 0, 1);
    expectEqual("terminal omitted cell is blank", terminal.cellAt(1, 2)->text.empty() ? 1 : 0, 1);
    expectEqual("terminal cursor visible", terminal.cursorState().visible ? 1 : 0, 1);
}

void testPaintHonorsWideCellsStylesAndCursor() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{5.0f, 8.0f, 300.0f, 160.0f});
    terminal.setFontSize(20.0f);
    terminal.setGrid(1, 3, {
        oneui::TerminalCell{L"A", {250, 250, 250, 255}, {20, 24, 36, 255}, oneui::TerminalCellBold | oneui::TerminalCellUnderline},
        oneui::TerminalCell{L"中", {240, 200, 80, 255}, {20, 24, 36, 255}, oneui::TerminalCellWide},
        oneui::TerminalCell{L"", {240, 200, 80, 255}, {20, 24, 36, 255}, oneui::TerminalCellWideContinuation},
    });
    terminal.setCursor(oneui::TerminalCursor{0, 1, true});

    RecordingCanvas canvas;
    terminal.paint(canvas);

    expectEqual("terminal paints one glyph per non-continuation cell", static_cast<int>(canvas.texts.size()), 2);
    expectEqual("terminal bold weight", canvas.texts.front().weight, 700);
    expectNear("terminal wide glyph width", canvas.texts.back().rect.width, 24.0f);
    expectEqual("terminal underline painted", canvas.lineCount, 1);
    expectEqual("terminal paints background and cursor", canvas.fillCount >= 2 ? 1 : 0, 1);
}

void testTextAndRawKeyCallbacksStaySeparate() {
    oneui::TerminalView terminal;
    std::wstring typed;
    std::vector<oneui::KeyEvent> keys;
    terminal.setOnTextInput([&](const std::wstring& text) { typed += text; });
    terminal.setOnRawKey([&](const oneui::KeyEvent& event) { keys.push_back(event); });

    oneui::KeyEvent down;
    down.virtualKey = 0x0D;
    down.pressed = true;
    terminal.onKeyDown(down);
    terminal.onKeyUp(down);
    terminal.onTextInput(L'你');

    expectEqual("terminal text callback", typed == L"你" ? 1 : 0, 1);
    expectEqual("terminal raw key callbacks", static_cast<int>(keys.size()), 2);
    expectEqual("terminal key up marked released", keys.back().pressed ? 1 : 0, 0);
}

} // namespace

int main() {
    testGridCopiesCellsAndCursor();
    testPaintHonorsWideCellsStylesAndCursor();
    testTextAndRawKeyCallbacksStaySeparate();

    if (failures != 0) {
        std::cerr << failures << " terminal view behavior test(s) failed.\n";
        return 1;
    }
    return 0;
}
