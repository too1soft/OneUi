#include "oneui/controls/data_grid.h"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

class RecordingCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override { ++fills; }
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring& text, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {
        texts.push_back(text);
    }
    float measureTextWidth(const std::wstring& text, float size, int = 400) const override {
        return static_cast<float>(text.size()) * size * 0.6f;
    }
    int fills = 0;
    std::vector<std::wstring> texts;
};

void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << " failed\n";
        ++failures;
    }
}

template <typename T>
void expectEqual(const char* name, T actual, T expected) {
    if (!(actual == expected)) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

std::vector<oneui::DataGridColumn> columns(int count) {
    std::vector<oneui::DataGridColumn> result;
    for (int i = 0; i < count; ++i) result.push_back({L"c" + std::to_wstring(i), L"col" + std::to_wstring(i), L"int", 100.0f});
    return result;
}

std::vector<oneui::DataGridRow> rows(std::int64_t start, std::int64_t count, int width) {
    std::vector<oneui::DataGridRow> result;
    for (auto r = start; r < start + count; ++r) {
        oneui::DataGridRow row;
        for (int c = 0; c < width; ++c) row.cells.push_back({L"r" + std::to_wstring(r) + L"c" + std::to_wstring(c), c == 1 ? "null" : ""});
        result.push_back(std::move(row));
    }
    return result;
}

struct Fixture {
    oneui::DataGrid grid;
    std::vector<std::pair<std::int64_t, std::int64_t>> requests;
    std::vector<oneui::DataGridSelection> selections;
    Fixture(std::int64_t rowCount, int width) {
        grid.setFrame(oneui::Rect{0.0f, 0.0f, 800.0f, 330.0f});
        grid.setColumns(columns(width));
        grid.setOnViewport([this](std::int64_t first, std::int64_t count) { requests.push_back({first, count}); });
        grid.setOnSelectionChanged([this](const oneui::DataGridSelection& s) { selections.push_back(s); });
        grid.setRowCount(rowCount);
    }
    void serveLastRequest(int width) {
        auto [first, count] = requests.back();
        grid.setWindow(first, rows(first, count, width));
    }
    void click(float x, float y, bool shift = false, int clicks = 1) {
        oneui::MouseEvent event;
        event.position = {x, y};
        event.shift = shift;
        event.clickCount = clicks;
        grid.onMouseDown(event);
        grid.onMouseUp(event);
    }
    void key(oneui::Key key, bool shift = false, bool control = false) {
        oneui::KeyEvent event;
        event.key = key;
        event.shift = shift;
        event.control = control;
        grid.onKeyDown(event);
    }
};

void testViewportRequestsCoverTheScreenAndFollowScrolling() {
    Fixture f(1'000'000, 30);
    expectTrue("an initial window is requested", !f.requests.empty());
    expectEqual("first request starts at the top", f.requests.back().first, std::int64_t{0});
    expectTrue("first request covers several screens", f.requests.back().second >= 20);
    f.serveLastRequest(30);
    const auto before = f.requests.size();
    oneui::MouseWheelEvent wheel;
    wheel.position = {400.0f, 200.0f};
    wheel.deltaY = -1.0f;
    f.grid.onMouseWheel(wheel);
    expectEqual("small scrolls stay inside the window", f.requests.size(), before);
    f.grid.scrollToCell(900'000, 0);
    expectTrue("far jumps request a new window", f.requests.size() > before);
    expectTrue("the new window holds the target", f.requests.back().first <= 900'000 &&
               f.requests.back().first + f.requests.back().second > 900'000);
    expectTrue("ten million pixels later the offset stays exact", f.grid.firstVisibleRow() <= 900'000);
}

void testPaintTouchesOnlyVisibleCells() {
    Fixture f(1'000'000, 30);
    f.serveLastRequest(30);
    RecordingCanvas canvas;
    f.grid.paint(canvas);
    std::size_t cells = 0;
    for (const auto& text : canvas.texts) if (text.rfind(L"r", 0) == 0) ++cells;
    expectTrue("visible cells were painted", cells > 0);
    expectTrue("off-screen cells were not painted", cells < 15 * 10);
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 200; ++frame) {
        oneui::MouseWheelEvent wheel;
        wheel.position = {400.0f, 200.0f};
        wheel.deltaY = -1.0f;
        f.grid.onMouseWheel(wheel);
        if (f.grid.windowStart() + static_cast<std::int64_t>(f.grid.windowSize()) < f.grid.firstVisibleRow() + f.grid.visibleRowCount())
            f.serveLastRequest(30);
        RecordingCanvas frameCanvas;
        f.grid.paint(frameCanvas);
    }
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 200.0;
    std::cout << "data grid frame (30 columns, 1M rows, recording canvas): " << ms << " ms\n";
    expectTrue("frames stay cheap", ms < 16.0);
}

void testKeyboardSelection() {
    Fixture f(100, 5);
    f.serveLastRequest(5);
    f.key(oneui::Key::Down);
    expectEqual("first key selects the first cell", f.grid.selection().focusRow, std::int64_t{0});
    f.key(oneui::Key::Down);
    f.key(oneui::Key::Right, true);
    f.key(oneui::Key::Right, true);
    const auto selection = f.grid.selection();
    expectEqual("anchor row kept", selection.anchorRow, std::int64_t{1});
    expectEqual("focus extended", selection.focusColumn, 2);
    expectTrue("rectangle contains inner cell", selection.contains(1, 1));
    f.key(oneui::Key::End, false, true);
    expectEqual("ctrl+end goes to the last row", f.grid.selection().focusRow, std::int64_t{99});
    expectEqual("ctrl+end goes to the last column", f.grid.selection().focusColumn, 4);
    oneui::KeyEvent all;
    all.key = oneui::Key::A;
    all.control = true;
    f.grid.onKeyDown(all);
    expectTrue("ctrl+a selects all", f.grid.selection().kind == oneui::DataGridSelectKind::All);
    expectTrue("selection changes are reported", f.selections.size() >= 5);
}

void testMouseSelectionSortAndResize() {
    Fixture f(100, 5);
    f.serveLastRequest(5);
    int sorted = -1;
    std::pair<int, float> resized{-1, 0.0f};
    f.grid.setOnSort([&](int column) { sorted = column; });
    f.grid.setOnResize([&](int column, float width) { resized = {column, width}; });
    const float gutter = 40.5f; // three-digit row numbers
    f.click(gutter + 150.0f, 30.0f + 28.0f * 2 + 5.0f);
    expectEqual("click selects row", f.grid.selection().focusRow, std::int64_t{2});
    expectEqual("click selects column", f.grid.selection().focusColumn, 1);
    f.click(gutter + 350.0f, 30.0f + 28.0f * 4 + 5.0f, true);
    expectEqual("shift-click keeps the anchor", f.grid.selection().anchorRow, std::int64_t{2});
    expectEqual("shift-click extends", f.grid.selection().focusColumn, 3);
    f.click(gutter + 50.0f, 10.0f);
    expectTrue("header click selects a column", f.grid.selection().kind == oneui::DataGridSelectKind::Columns);
    f.click(gutter + 95.0f - 8.0f, 10.0f);
    expectEqual("sort button sorts", sorted, 0);
    f.click(10.0f, 30.0f + 28.0f * 3 + 5.0f);
    expectTrue("gutter click selects a row", f.grid.selection().kind == oneui::DataGridSelectKind::Rows);
    oneui::MouseEvent press;
    press.position = {gutter + 100.0f, 10.0f};
    f.grid.onMouseDown(press);
    press.position = {gutter + 160.0f, 10.0f};
    f.grid.onMouseMove(press);
    f.grid.onMouseUp(press);
    expectEqual("resize reports the column", resized.first, 0);
    expectTrue("resize reports the width", resized.second > 150.0f && resized.second < 170.0f);
}

void testInPlaceEditing() {
    Fixture f(10, 3);
    f.serveLastRequest(3);
    std::vector<std::tuple<std::int64_t, int, std::wstring>> edits;
    f.grid.setOnEdit([&](std::int64_t row, int column, const std::wstring& text) { edits.push_back({row, column, text}); });
    f.key(oneui::Key::Down);
    f.key(oneui::Key::F2);
    expectTrue("read-only grids do not edit", !f.grid.editing());
    f.grid.setEditable(true);
    f.key(oneui::Key::F2);
    expectTrue("F2 starts editing", f.grid.editing());
    f.key(oneui::Key::Escape);
    expectTrue("Esc cancels", !f.grid.editing() && edits.empty());
    f.grid.onTextInput(L'x');
    expectTrue("typing starts editing", f.grid.editing());
    f.grid.onTextInput(L'y');
    f.key(oneui::Key::Enter);
    expectEqual("Enter commits once", edits.size(), std::size_t{1});
    expectTrue("typed text is committed", std::get<2>(edits[0]) == L"xy");
    expectEqual("Enter moves down", f.grid.selection().focusRow, std::int64_t{1});
    f.key(oneui::Key::Right);
    f.key(oneui::Key::F2);
    f.key(oneui::Key::Tab);
    expectEqual("Tab commits", edits.size(), std::size_t{2});
    expectTrue("NULL cells start empty", std::get<2>(edits[1]).empty());
    expectEqual("Tab moves right", f.grid.selection().focusColumn, 2);
}

void testEditingKeysAndImeEntry() {
    Fixture f(10, 3);
    f.serveLastRequest(3);
    f.grid.setEditable(true);
    std::vector<std::tuple<std::int64_t, int, std::wstring>> edits;
    f.grid.setOnEdit([&](std::int64_t row, int column, const std::wstring& text) { edits.push_back({row, column, text}); });
    f.key(oneui::Key::Down);
    f.key(oneui::Key::Down);
    f.key(oneui::Key::Down);
    expectEqual("starting row", f.grid.selection().focusRow, std::int64_t{2});
    f.grid.onTextInput(L'\u4e2d');
    expectTrue("a CJK character starts editing", f.grid.editing());
    oneui::KeyEvent shiftEnter;
    shiftEnter.key = oneui::Key::Enter;
    shiftEnter.shift = true;
    f.grid.onKeyDown(shiftEnter);
    expectTrue("Shift+Enter commits", !f.grid.editing() && edits.size() == 1 && std::get<2>(edits[0]) == L"\u4e2d");
    expectEqual("Shift+Enter moves up", f.grid.selection().focusRow, std::int64_t{1});
    expectTrue("editable grids accept IME before editing", f.grid.textInputState().editable);
    f.grid.setTextComposition(L"zhong", 5);
    expectTrue("IME composition opens the editor", f.grid.editing());
    expectTrue("the composition reaches the editor", f.grid.textInputState().editable);
    f.grid.setTextComposition({}, 0);
    f.grid.onTextCommitted(L"\u4e2d\u6587");
    oneui::KeyEvent altEnter;
    altEnter.key = oneui::Key::Enter;
    altEnter.alt = true;
    f.grid.onKeyDown(altEnter);
    expectTrue("Alt+Enter keeps editing", f.grid.editing());
    f.grid.onTextInput(L'x');
    f.key(oneui::Key::Enter);
    expectEqual("IME edit commits once", edits.size(), std::size_t{2});
    expectTrue("committed IME text and a line break", std::get<2>(edits[1]) == L"\u4e2d\u6587\nx");
    f.grid.setEditable(false);
    expectTrue("read-only grids do not take IME", !f.grid.textInputState().editable);
    f.grid.onTextCommitted(L"\u4e2d");
    expectTrue("read-only grids ignore committed text", !f.grid.editing());
}

void testCopyContextAndActivate() {
    Fixture f(10, 3);
    f.serveLastRequest(3);
    int copies = 0;
    std::vector<std::pair<std::int64_t, int>> activations, menus;
    oneui::Point menuAt{};
    f.grid.setOnCopy([&] { ++copies; });
    f.grid.setOnActivate([&](std::int64_t row, int column) { activations.push_back({row, column}); });
    f.grid.setOnContextMenu([&](std::int64_t row, int column, oneui::Point at) { menus.push_back({row, column}); menuAt = at; });
    f.key(oneui::Key::Down);
    f.key(oneui::Key::C, false, true);
    oneui::KeyEvent insert;
    insert.key = oneui::Key::Other;
    insert.virtualKey = 0x2d;
    insert.control = true;
    f.grid.onKeyDown(insert);
    expectEqual("Ctrl+C and Ctrl+Insert only raise copy", copies, 2);
    oneui::KeyEvent apps;
    apps.key = oneui::Key::Other;
    apps.virtualKey = 0x5d;
    f.grid.onKeyDown(apps);
    oneui::KeyEvent shiftF10;
    shiftF10.key = oneui::Key::Other;
    shiftF10.virtualKey = 0x79;
    shiftF10.shift = true;
    f.grid.onKeyDown(shiftF10);
    expectEqual("menu keys raise context", menus.size(), std::size_t{2});
    expectTrue("keyboard context sits under the focused cell", menuAt.y >= 30.0f + 28.0f);
    const float gutter = 30.0f;
    f.click(gutter + 150.0f, 30.0f + 28.0f * 3 + 5.0f, false, 2);
    f.key(oneui::Key::Enter);
    expectEqual("double-click and Enter activate read-only cells", activations.size(), std::size_t{2});
    oneui::MouseEvent right;
    right.position = {gutter + 250.0f, 30.0f + 28.0f * 5 + 5.0f};
    right.button = oneui::MouseButton::Right;
    f.grid.onMouseDown(right);
    expectTrue("right click selects and reports the cell", menus.size() == 3 && menus.back().first == 5 && menus.back().second == 2);
    expectTrue("right click reports the pointer position", menuAt.x == right.position.x && menuAt.y == right.position.y);
}

} // namespace

int main() {
    testViewportRequestsCoverTheScreenAndFollowScrolling();
    testPaintTouchesOnlyVisibleCells();
    testKeyboardSelection();
    testMouseSelectionSortAndResize();
    testInPlaceEditing();
    testEditingKeysAndImeEntry();
    testCopyContextAndActivate();
    if (failures) std::cerr << failures << " data grid failure(s)\n";
    return failures ? 1 : 0;
}
