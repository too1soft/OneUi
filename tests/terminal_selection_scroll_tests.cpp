#include "oneui/controls/terminal_view.h"
#include <algorithm>
#include <iostream>

int main() {
    int failures = 0;
    const auto check = [&](bool value, const char* name) {
        if (!value) { std::cerr << name << '\n'; ++failures; }
    };
    oneui::TerminalView view;
    view.setFrame({0, 0, 6, 3});
    std::uint64_t offset = 0;
    const auto publish = [&] {
        view.setScrollback(5, offset);
        std::vector<oneui::TerminalCell> cells;
        for (int row = 0; row < 3; ++row) {
            const wchar_t c = L'A' + 5 - static_cast<int>(offset) + row;
            for (int col = 0; col < 3; ++col) cells.push_back(oneui::TerminalCell{std::wstring(1, c)});
        }
        view.setGrid(3, 3, std::move(cells));
    };
    publish();
    int scrolls = 0;
    view.setOnScroll([&](int rows) {
        ++scrolls;
        offset = std::clamp<std::int64_t>(static_cast<std::int64_t>(offset) + rows, 0, 5);
        publish();
    });
    auto clipboard = std::make_shared<oneui::MemoryClipboard>();
    view.setClipboard(clipboard);
    view.setOnRawKey([&](const oneui::KeyEvent& event) {
        if (event.control && event.virtualKey == 0x43 && view.hasSelection()) view.copySelectionToClipboard();
        if (event.control && event.virtualKey == 0x41) view.selectAll();
    });
    view.setSelection(0, 0, 0, 3);
    oneui::KeyEvent copy;
    copy.control = true; copy.virtualKey = 0x43; copy.key = oneui::Key::C;
    view.onKeyDown(copy);
    check(clipboard->text() == L"FFF", "host smart Ctrl+C sees the selection");
    check(view.hasSelection(), "copy keeps the selection available for another copy");
    oneui::KeyEvent selectAll;
    selectAll.control = true; selectAll.virtualKey = 0x41;
    view.onKeyDown(selectAll);
    check(view.selectedText() == L"FFF\r\nGGG\r\nHHH", "host Select All keeps the selection it creates");

    // Move beyond the top edge, then leave the mouse still while frames tick.
    view.onMouseDown({{0, 1}, oneui::MouseButton::Left});
    view.onMouseMove({{3, -1}, oneui::MouseButton::Left});
    view.tickAnimations(100);
    view.tickAnimations(101);
    check(offset == 1, "edge drag scrolls without another pointer event");
    view.tickAnimations(151);
    view.tickAnimations(152);
    check(offset == 2, "held pointer continues scrolling");
    view.onMouseUp({{0, -1}, oneui::MouseButton::Left});
    check(view.selectedText() == L"DDD\r\nEEE\r\nFFF\r\n", "upward selection retains offscreen rows and half-open endpoint");
    check(view.copySelectionToClipboard(), "cross-screen selection copies");
    const auto copied = clipboard->text();
    view.tickAnimations(220);
    check(scrolls == 2 && copied == view.selectedText(), "release stops scrolling and preserves selection");

    view.onMouseDown({{0, 0}, oneui::MouseButton::Left});
    view.onMouseMove({{3, 4}, oneui::MouseButton::Left});
    view.tickAnimations(300);
    view.tickAnimations(301);
    view.tickAnimations(351);
    view.tickAnimations(352);
    view.onMouseUp({{3, 4}, oneui::MouseButton::Left});
    check(offset == 0, "downward drag reaches live output");
    check(view.selectedText() == L"DDD\r\nEEE\r\nFFF\r\nGGG\r\nHHH", "downward selection retains every intervening row");
    view.clearSelection();
    view.selectAll();
    check(view.selectedText() == L"FFF\r\nGGG\r\nHHH", "new selection releases the previous history");
    view.onMouseDown({{0, 1}, oneui::MouseButton::Left});
    view.onMouseMove({{3, 0}, oneui::MouseButton::Left});
    view.onMouseWheel({{2, 1}, 1});
    view.onMouseUp({{0, 0}, oneui::MouseButton::Left});
    check(offset == 2, "wheel scroll during a drag is bounded to an overlapping frame");
    check(view.selectedText() == L"DDD\r\nEEE\r\nFFF\r\n", "wheel selection keeps its original anchor");
    view.setScrollback(0, 0);
    check(!view.hasSelection(), "clearing history releases cached selection text");
    return failures ? 1 : 0;
}
