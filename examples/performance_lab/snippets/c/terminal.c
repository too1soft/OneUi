#include <oneui/oneui_c_api.h>

/* UTF-8 callback/user_data stay valid until cleared or widget destruction.
   The caller releases the returned widget with oneui_widget_destroy. */
OneUiWidget *example_c_terminal(OneUiUtf8TextCallback input, void *user_data) {
    OneUiWidget *terminal = oneui_terminal_view_create();
    if (!terminal)
        return 0;
    OneUiTerminalCellUtf8 cells[4] = {0};
    cells[0].text = (OneUiUtf8String){"A", 1};
    for (int i = 0; i < 4; ++i) {
        cells[i].foreground = (OneUiColor){229, 236, 232, 255};
        cells[i].background = (OneUiColor){28, 34, 40, 255};
    }
    oneui_terminal_view_set_grid_utf8(terminal, 2, 2, cells, 4);
    oneui_terminal_view_set_cursor(terminal, 0, 1, 1);
    oneui_terminal_view_set_cursor_blinking(terminal, 0);
    oneui_terminal_view_set_on_text_input_utf8(terminal, input, user_data);
    /* TerminalView draws cells and emits input. It does not start a shell. */
    return terminal;
}
