#include "oneui/controls/terminal_view.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

class RecordingCanvas final : public oneui::Canvas {
public:
    struct FillCall {
        oneui::Rect rect;
        oneui::Color color;
    };

    struct TextCall {
        std::wstring text;
        oneui::Rect rect;
        oneui::Color color;
        int weight = 400;
        oneui::TextFontFamily family = oneui::TextFontFamily::Default;
    };

    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect rect, oneui::Color color, float = 0.0f) override {
        ++fillCount;
        fills.push_back(FillCall{rect, color});
    }
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
    void drawTextStyledWithFont(
        const std::wstring& text,
        oneui::Rect rect,
        oneui::Color color,
        float,
        oneui::TextAlign,
        oneui::TextFontFamily family,
        int weight = 400) override {
        texts.push_back(TextCall{text, rect, color, weight, family});
    }
    float measureTextWidth(const std::wstring&, float size, int = 400) const override { return size * 0.60f; }
    float measureTextWidthWithFont(
        const std::wstring&,
        float size,
        oneui::TextFontFamily family,
        int = 400) const override {
        lastMeasuredFamily = family;
        return size * 0.60f;
    }

    int fillCount = 0;
    int lineCount = 0;
    std::vector<FillCall> fills;
    mutable oneui::TextFontFamily lastMeasuredFamily = oneui::TextFontFamily::Default;
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

bool sameColor(oneui::Color left, oneui::Color right) {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
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

void testGridUpdatesOnlyRequestedCells() {
    oneui::TerminalView terminal;
    terminal.setGrid(1, 4, {
        oneui::TerminalCell{L"A"},
        oneui::TerminalCell{L"B"},
        oneui::TerminalCell{L"C"},
        oneui::TerminalCell{L"D"},
    });

    terminal.updateCells(1, {
        oneui::TerminalCell{L"X"},
        oneui::TerminalCell{L"Y"},
    });

    expectEqual("terminal partial update keeps prefix", terminal.cellAt(0, 0)->text == L"A" ? 1 : 0, 1);
    expectEqual("terminal partial update first cell", terminal.cellAt(0, 1)->text == L"X" ? 1 : 0, 1);
    expectEqual("terminal partial update second cell", terminal.cellAt(0, 2)->text == L"Y" ? 1 : 0, 1);
    expectEqual("terminal partial update keeps suffix", terminal.cellAt(0, 3)->text == L"D" ? 1 : 0, 1);

    terminal.updateCells(3, {
        oneui::TerminalCell{L"Z"},
        oneui::TerminalCell{L"ignored"},
    });
    expectEqual("terminal partial update clamps to grid", terminal.cellAt(0, 3)->text == L"Z" ? 1 : 0, 1);
    expectEqual("terminal partial update keeps cell count", static_cast<int>(terminal.cellCount()), 4);
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
    oneui::TerminalViewport viewport{};
    int viewportChanges = 0;
    terminal.setOnViewportChanged([&](oneui::TerminalViewport value) {
        viewport = value;
        ++viewportChanges;
    });

    RecordingCanvas canvas;
    terminal.paint(canvas);

    expectEqual("terminal paints one glyph per non-continuation cell", static_cast<int>(canvas.texts.size()), 2);
    expectEqual("terminal bold weight", canvas.texts.front().weight, 700);
    expectEqual(
        "terminal uses monospace font",
        canvas.texts.front().family == oneui::TextFontFamily::Monospace ? 1 : 0,
        1);
    expectEqual(
        "terminal measures monospace font",
        canvas.lastMeasuredFamily == oneui::TextFontFamily::Monospace ? 1 : 0,
        1);
    expectNear("terminal wide glyph width", canvas.texts.back().rect.width, 24.0f);
    expectEqual("terminal underline painted", canvas.lineCount, 1);
    expectEqual("terminal paints background and cursor", canvas.fillCount >= 2 ? 1 : 0, 1);
    expectEqual("terminal reports viewport once", viewportChanges, 1);
    expectEqual("terminal viewport columns", viewport.columns, 25);
    expectEqual("terminal viewport rows", viewport.rows, 6);

    terminal.paint(canvas);
    expectEqual("terminal suppresses duplicate viewport", viewportChanges, 1);
}

void testPaintBatchesCompatibleAsciiCellsIntoRuns() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{0.0f, 0.0f, 300.0f, 80.0f});
    terminal.setFontSize(20.0f);
    const oneui::Color first{220, 226, 240, 255};
    const oneui::Color second{80, 180, 255, 255};
    const oneui::Color background{20, 24, 36, 255};
    terminal.setGrid(1, 6, {
        oneui::TerminalCell{L"A", first, background},
        oneui::TerminalCell{L"B", first, background},
        oneui::TerminalCell{L"C", first, background},
        oneui::TerminalCell{L"D", second, background},
        oneui::TerminalCell{L"E", second, background},
        oneui::TerminalCell{L"F", second, background},
    });
    terminal.setCursor(oneui::TerminalCursor{0, 0, false});

    RecordingCanvas canvas;
    terminal.paint(canvas);

    expectEqual("terminal batches ASCII cells by paint style", static_cast<int>(canvas.texts.size()), 2);
    expectEqual("terminal first ASCII run keeps text", canvas.texts.front().text == L"ABC" ? 1 : 0, 1);
    expectEqual("terminal second ASCII run keeps text", canvas.texts.back().text == L"DEF" ? 1 : 0, 1);
    expectNear("terminal ASCII run spans whole cell range", canvas.texts.front().rect.width, 36.0f);
}

void testLineHeightAndCursorStylesAreNativeMetrics() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 120.0f});
    terminal.setFontSize(20.0f);
    terminal.setLineHeight(1.5f);
    terminal.setGrid(3, 4, {
        oneui::TerminalCell{L"A"},
    });
    terminal.setCursor(oneui::TerminalCursor{1, 1, true});

    oneui::TerminalViewport viewport{};
    terminal.setOnViewportChanged([&](oneui::TerminalViewport value) { viewport = value; });
    RecordingCanvas blockCanvas;
    terminal.paint(blockCanvas);
    expectNear("terminal configurable line height updates caret", terminal.textInputCaretRect().height, 30.0f);
    expectEqual("terminal configurable line height updates viewport", viewport.rows, 4);
    expectEqual("terminal block cursor fills its cell", blockCanvas.fillCount >= 2 ? 1 : 0, 1);

    terminal.setCursorStyle(oneui::TerminalCursorStyle::Bar);
    RecordingCanvas barCanvas;
    terminal.paint(barCanvas);
    expectEqual("terminal bar cursor uses a native stroke", barCanvas.lineCount, 1);

    terminal.setCursorStyle(oneui::TerminalCursorStyle::Underline);
    RecordingCanvas underlineCanvas;
    terminal.paint(underlineCanvas);
    expectEqual("terminal underline cursor uses a native stroke", underlineCanvas.lineCount, 1);

    int animationSchedules = 0;
    terminal.onFocusChanged(true);
    terminal.setAnimationScheduler([&] { ++animationSchedules; });
    expectEqual("terminal focused blinking cursor schedules frames", animationSchedules, 1);
    expectEqual("terminal blinking cursor keeps animation alive", terminal.tickAnimations(0.0) ? 1 : 0, 1);
}

void testTerminalUpdatesInvalidateOnlyDirtyCells() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{10.0f, 20.0f, 120.0f, 90.0f});
    terminal.setFontSize(20.0f);
    terminal.setGrid(3, 4, {});
    terminal.setCursor(oneui::TerminalCursor{0, 0, true});
    RecordingCanvas canvas;
    terminal.paint(canvas);

    int fullInvalidations = 0;
    std::vector<oneui::Rect> dirtyRects;
    terminal.setInvalidator([&] { ++fullInvalidations; });
    terminal.setRectInvalidator([&](oneui::Rect rect) { dirtyRects.push_back(rect); });

    terminal.updateCells(5, {
        oneui::TerminalCell{L"X"},
        oneui::TerminalCell{L"Y"},
    });
    expectEqual("terminal cell update avoids full invalidation", fullInvalidations, 0);
    expectEqual("terminal cell update emits one dirty rectangle", static_cast<int>(dirtyRects.size()), 1);
    if (!dirtyRects.empty()) {
        expectNear("terminal dirty rectangle starts at changed column", dirtyRects.front().x, 22.0f);
        expectNear("terminal dirty rectangle spans changed cells", dirtyRects.front().width, 24.0f);
        expectNear("terminal dirty rectangle starts at changed row", dirtyRects.front().y, 46.0f);
    }

    dirtyRects.clear();
    terminal.setCursor(oneui::TerminalCursor{2, 3, true});
    expectEqual("terminal cursor movement invalidates old and new cells", static_cast<int>(dirtyRects.size()), 2);
    expectEqual("terminal cursor movement avoids full invalidation", fullInvalidations, 0);
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

void testCommittedUnicodeAndImeCaretStayCellAligned() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{5.0f, 8.0f, 300.0f, 160.0f});
    terminal.setFontSize(20.0f);
    terminal.setGrid(2, 3, {});
    terminal.setCursor(oneui::TerminalCursor{1, 2, true});

    std::wstring typed;
    terminal.setOnTextInput([&](const std::wstring& text) { typed = text; });
    const std::wstring nonBmp{static_cast<wchar_t>(0xD83D), static_cast<wchar_t>(0xDE80)};
    expectEqual("terminal accepts committed Unicode text", terminal.onTextInputText(nonBmp) ? 1 : 0, 1);
    expectEqual("terminal preserves surrogate pair in one callback", typed == nonBmp ? 1 : 0, 1);

    RecordingCanvas canvas;
    terminal.paint(canvas);
    const oneui::Rect caret = terminal.textInputCaretRect();
    expectNear("terminal IME caret x follows cursor column", caret.x, 29.0f);
    expectNear("terminal IME caret y follows cursor row", caret.y, 34.0f);
    expectNear("terminal IME caret width matches cell", caret.width, 12.0f);
    expectNear("terminal IME caret height matches cell", caret.height, 26.0f);
}

void testStyleBoxControlsTerminalVisualTokens() {
    oneui::TerminalView terminal;
    oneui::StyleBox style;
    style.background.color = oneui::Color{11, 17, 29, 255};
    style.foreground = oneui::Color{209, 216, 235, 255};
    style.caretColor = oneui::Color{132, 145, 255, 255};
    style.selectionColor = oneui::Color{63, 69, 112, 255};
    style.fontSize = 17.0f;
    terminal.setStyleBox(style);
    terminal.setFrame(oneui::Rect{0.0f, 0.0f, 80.0f, 30.0f});
    terminal.setGrid(1, 1, {oneui::TerminalCell{L"A"}});
    terminal.setCursor(oneui::TerminalCursor{0, 0, false});
    terminal.setSelection(0, 0, 0, 1);

    RecordingCanvas canvas;
    terminal.paint(canvas);
    expectNear("terminal style box font size", terminal.fontSize(), 17.0f);
    expectEqual(
        "terminal style box paints background",
        !canvas.fills.empty() && sameColor(canvas.fills.front().color, *style.background.color) ? 1 : 0,
        1);
    expectEqual(
        "terminal style box paints selection",
        canvas.fills.size() >= 2 && sameColor(canvas.fills[1].color, *style.selectionColor) ? 1 : 0,
        1);
}

void testSelectionCopyPasteAndTerminalShortcuts() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 80.0f});
    terminal.setFontSize(20.0f);
    terminal.setGrid(2, 5, {
        oneui::TerminalCell{L"A"},
        oneui::TerminalCell{
            L"中",
            {220, 226, 240, 255},
            {20, 24, 36, 255},
            oneui::TerminalCellWide,
        },
        oneui::TerminalCell{
            L"",
            {220, 226, 240, 255},
            {20, 24, 36, 255},
            oneui::TerminalCellWideContinuation,
        },
        oneui::TerminalCell{},
        oneui::TerminalCell{},
        oneui::TerminalCell{L"B"},
        oneui::TerminalCell{L"C"},
        oneui::TerminalCell{},
        oneui::TerminalCell{},
        oneui::TerminalCell{},
    });
    auto clipboard = std::make_shared<oneui::MemoryClipboard>();
    terminal.setClipboard(clipboard);
    std::wstring pasted;
    int rawKeyCount = 0;
    terminal.setOnPaste([&](const std::wstring& text) { pasted = text; });
    terminal.setOnRawKey([&](const oneui::KeyEvent&) { ++rawKeyCount; });

    RecordingCanvas canvas;
    terminal.paint(canvas);
    terminal.onMouseDown(oneui::MouseEvent{{0.0f, 8.0f}, oneui::MouseButton::Left});
    terminal.onMouseMove(oneui::MouseEvent{{36.0f, 8.0f}, oneui::MouseButton::Left});
    terminal.onMouseUp(oneui::MouseEvent{{36.0f, 8.0f}, oneui::MouseButton::Left});
    expectEqual("terminal drag creates selection", terminal.hasSelection() ? 1 : 0, 1);
    expectEqual("terminal selection skips wide continuation", terminal.selectedText() == L"A中" ? 1 : 0, 1);

    oneui::KeyEvent copy;
    copy.key = oneui::Key::C;
    copy.control = true;
    copy.shift = true;
    terminal.onKeyDown(copy);
    expectEqual("terminal copy shortcut writes clipboard", clipboard->text() == L"A中" ? 1 : 0, 1);
    expectEqual("terminal copy shortcut is not forwarded", rawKeyCount, 0);

    oneui::KeyEvent interrupt;
    interrupt.key = oneui::Key::C;
    interrupt.control = true;
    terminal.onKeyDown(interrupt);
    expectEqual("terminal ctrl-c remains a raw terminal key", rawKeyCount, 1);

    clipboard->setText(L"echo 你好");
    oneui::KeyEvent paste;
    paste.key = oneui::Key::V;
    paste.control = true;
    paste.shift = true;
    terminal.onKeyDown(paste);
    expectEqual("terminal paste uses dedicated callback", pasted == L"echo 你好" ? 1 : 0, 1);
    expectEqual("terminal paste shortcut is not forwarded", rawKeyCount, 1);
    expectEqual("terminal paste clears the previous selection", terminal.hasSelection() ? 1 : 0, 0);

    terminal.selectAll();
    expectEqual("terminal select all trims blank cell padding", terminal.selectedText() == L"A中\r\nBC" ? 1 : 0, 1);
    terminal.setGrid(3, 5, {});
    expectEqual("terminal resize clears stale selection", terminal.hasSelection() ? 1 : 0, 0);
}

void testInputClearsSelectionWithoutBreakingModifierShortcuts() {
    oneui::TerminalView terminal;
    terminal.setGrid(1, 4, {
        oneui::TerminalCell{L"t"}, oneui::TerminalCell{L"e"},
        oneui::TerminalCell{L"s"}, oneui::TerminalCell{L"t"},
    });
    auto clipboard = std::make_shared<oneui::MemoryClipboard>();
    terminal.setClipboard(clipboard);
    terminal.selectAll();

    oneui::KeyEvent control;
    control.virtualKey = 0x11;
    control.control = true;
    terminal.onKeyDown(control);
    expectEqual("terminal modifier keeps selection available for copy", terminal.hasSelection() ? 1 : 0, 1);

    oneui::KeyEvent copy;
    copy.key = oneui::Key::C;
    copy.virtualKey = 0x43;
    copy.control = true;
    copy.shift = true;
    terminal.onKeyDown(copy);
    expectEqual("terminal modifier sequence still copies selection", clipboard->text() == L"test" ? 1 : 0, 1);

    expectEqual("terminal committed input is handled", terminal.onTextInputText(L"x") ? 1 : 0, 1);
    expectEqual("terminal committed input clears selection", terminal.hasSelection() ? 1 : 0, 0);
}

void testDoubleClickWordTripleClickLineAndCopyOnSelect() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 80.0f});
    terminal.setFontSize(20.0f);
    const std::wstring text = L"echo foo-bar";
    std::vector<oneui::TerminalCell> cells;
    for (wchar_t character : text) {
        cells.push_back(oneui::TerminalCell{std::wstring(1, character)});
    }
    const auto columns = static_cast<std::uint16_t>(cells.size());
    terminal.setGrid(1, columns, std::move(cells));
    auto clipboard = std::make_shared<oneui::MemoryClipboard>();
    terminal.setClipboard(clipboard);
    terminal.setCopyOnSelect(true);
    RecordingCanvas canvas;
    terminal.paint(canvas);

    const oneui::MouseEvent wordClick{{74.0f, 8.0f}, oneui::MouseButton::Left};
    terminal.onMouseDown(wordClick);
    terminal.onMouseUp(wordClick);
    terminal.onMouseDown(wordClick);
    terminal.onMouseUp(wordClick);
    expectEqual("terminal double click selects one word", terminal.selectedText() == L"foo" ? 1 : 0, 1);
    expectEqual("terminal copy-on-select copies completed word", clipboard->text() == L"foo" ? 1 : 0, 1);

    terminal.onMouseDown(wordClick);
    terminal.onMouseUp(wordClick);
    expectEqual("terminal triple click selects the visible line", terminal.selectedText() == text ? 1 : 0, 1);
    expectEqual("terminal copy-on-select copies completed line", clipboard->text() == text ? 1 : 0, 1);
}

void testProgrammaticSelectionUsesHalfOpenCellRanges() {
    oneui::TerminalView terminal;
    terminal.setGrid(2, 6, {
        oneui::TerminalCell{L"a"}, oneui::TerminalCell{L"l"}, oneui::TerminalCell{L"p"},
        oneui::TerminalCell{L"h"}, oneui::TerminalCell{L"a"}, oneui::TerminalCell{},
        oneui::TerminalCell{L"b"}, oneui::TerminalCell{L"e"}, oneui::TerminalCell{L"t"},
        oneui::TerminalCell{L"a"}, oneui::TerminalCell{}, oneui::TerminalCell{},
    });

    terminal.setSelection(0, 1, 0, 4);
    expectEqual("terminal programmatic selection is half open", terminal.selectedText() == L"lph" ? 1 : 0, 1);
    terminal.setSelection(1, 0, 1, 4);
    expectEqual("terminal programmatic selection changes rows", terminal.selectedText() == L"beta" ? 1 : 0, 1);
    terminal.setSelection(9, 9, 9, 9);
    expectEqual("terminal programmatic selection clamps safely", terminal.hasSelection() ? 1 : 0, 0);
}

void testWheelReportsWholeScrollbackRows() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{10.0f, 10.0f, 240.0f, 80.0f});
    terminal.setScrollRowsPerWheel(4.0f);
    int scrolledRows = 0;
    terminal.setOnScroll([&](int rows) { scrolledRows += rows; });

    expectEqual(
        "terminal wheel outside bounds is ignored",
        terminal.onMouseWheel(oneui::MouseWheelEvent{{0.0f, 0.0f}, 1.0f}) ? 1 : 0,
        0);
    expectEqual(
        "terminal wheel inside bounds is handled",
        terminal.onMouseWheel(oneui::MouseWheelEvent{{20.0f, 20.0f}, 1.0f}) ? 1 : 0,
        1);
    expectEqual("terminal wheel reports configured rows", scrolledRows, 4);
    terminal.onMouseWheel(oneui::MouseWheelEvent{{20.0f, 20.0f}, -0.5f});
    expectEqual("terminal wheel preserves direction", scrolledRows, 2);
}

void testMouseReportingPreservesApplicationInputAndShiftSelection() {
    oneui::TerminalView terminal;
    terminal.setFrame(oneui::Rect{10.0f, 20.0f, 240.0f, 100.0f});
    terminal.setFontSize(20.0f);
    terminal.setGrid(3, 5, {});
    RecordingCanvas canvas;
    terminal.paint(canvas);

    std::vector<oneui::TerminalPointerEvent> pointers;
    int scrolledRows = 0;
    terminal.setOnPointer([&](const oneui::TerminalPointerEvent& event) {
        pointers.push_back(event);
    });
    terminal.setOnScroll([&](int rows) { scrolledRows += rows; });
    terminal.setMouseReporting(true);

    oneui::MouseEvent press{{35.0f, 50.0f}, oneui::MouseButton::Left};
    press.control = true;
    terminal.onMouseDown(press);
    terminal.onMouseMove(oneui::MouseEvent{{47.0f, 50.0f}, oneui::MouseButton::Left});
    oneui::MouseEvent release{{47.0f, 50.0f}, oneui::MouseButton::Left};
    release.shift = true;
    terminal.onMouseUp(release);

    expectEqual("terminal reports pointer press move and release", static_cast<int>(pointers.size()), 3);
    expectEqual("terminal pointer row is cell based", pointers.front().row, 1);
    expectEqual("terminal pointer column is cell based", pointers.front().column, 2);
    expectEqual("terminal pointer keeps modifiers", pointers.front().control ? 1 : 0, 1);
    expectEqual(
        "terminal release survives modifier changes",
        pointers.back().action == oneui::TerminalPointerAction::Release ? 1 : 0,
        1);

    terminal.onMouseWheel(oneui::MouseWheelEvent{{35.0f, 50.0f}, 1.0f});
    expectEqual("terminal reports application wheel", static_cast<int>(pointers.size()), 4);
    expectEqual("terminal application wheel does not scroll history", scrolledRows, 0);

    oneui::MouseEvent selectionStart{{10.0f, 20.0f}, oneui::MouseButton::Left};
    selectionStart.shift = true;
    terminal.onMouseDown(selectionStart);
    terminal.onMouseMove(oneui::MouseEvent{{34.0f, 20.0f}, oneui::MouseButton::Left});
    terminal.onMouseUp(oneui::MouseEvent{{34.0f, 20.0f}, oneui::MouseButton::Left});
    expectEqual("terminal shift drag keeps local selection", terminal.hasSelection() ? 1 : 0, 1);

    oneui::MouseWheelEvent historyWheel{{35.0f, 50.0f}, 1.0f};
    historyWheel.shift = true;
    terminal.onMouseWheel(historyWheel);
    expectEqual("terminal shift wheel scrolls local history", scrolledRows, 3);
}

} // namespace

int main() {
    testGridCopiesCellsAndCursor();
    testGridUpdatesOnlyRequestedCells();
    testPaintHonorsWideCellsStylesAndCursor();
    testPaintBatchesCompatibleAsciiCellsIntoRuns();
    testLineHeightAndCursorStylesAreNativeMetrics();
    testTerminalUpdatesInvalidateOnlyDirtyCells();
    testTextAndRawKeyCallbacksStaySeparate();
    testCommittedUnicodeAndImeCaretStayCellAligned();
    testStyleBoxControlsTerminalVisualTokens();
    testSelectionCopyPasteAndTerminalShortcuts();
    testInputClearsSelectionWithoutBreakingModifierShortcuts();
    testDoubleClickWordTripleClickLineAndCopyOnSelect();
    testProgrammaticSelectionUsesHalfOpenCellRanges();
    testWheelReportsWholeScrollbackRows();
    testMouseReportingPreservesApplicationInputAndShiftSelection();

    if (failures != 0) {
        std::cerr << failures << " terminal view behavior test(s) failed.\n";
        return 1;
    }
    return 0;
}
