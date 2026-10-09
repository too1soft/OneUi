#include <memory>
#include <oneui/controls/terminal_view.h>

std::shared_ptr<oneui::TerminalView> example_terminal() {
    auto terminal = std::make_shared<oneui::TerminalView>();
    std::vector<oneui::TerminalCell> cells(2 * 80);
    cells[0].text = L"A";
    terminal->setGrid(2, 80, std::move(cells));
    terminal->setCursor({0, 1, true});
    terminal->setCursorBlinking(false);
    terminal->setOnTextInput([](const std::wstring &text) {
        // Feed your terminal model. TerminalView is not a shell/SSH client.
        (void)text;
    });
    return terminal;
}
