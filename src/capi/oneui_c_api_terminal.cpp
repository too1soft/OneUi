#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/terminal_view.h"
#include "oneui/platform/window.h"
#include "oneui/controls/log_view.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace oneui::capi;

namespace {

oneui::TerminalUnderlineStyle toNativeTerminalUnderlineStyle(unsigned int style) {
    switch (style) {
    case 1:
        return oneui::TerminalUnderlineStyle::Single;
    case 2:
        return oneui::TerminalUnderlineStyle::Double;
    case 3:
        return oneui::TerminalUnderlineStyle::Curly;
    case 4:
        return oneui::TerminalUnderlineStyle::Dotted;
    case 5:
        return oneui::TerminalUnderlineStyle::Dashed;
    default:
        return oneui::TerminalUnderlineStyle::None;
    }
}

OneUiTerminalPointerAction toCTerminalPointerAction(oneui::TerminalPointerAction action) {
    switch (action) {
    case oneui::TerminalPointerAction::Press:
        return OneUiTerminalPointerActionPress;
    case oneui::TerminalPointerAction::Release:
        return OneUiTerminalPointerActionRelease;
    case oneui::TerminalPointerAction::Wheel:
        return OneUiTerminalPointerActionWheel;
    case oneui::TerminalPointerAction::Move:
    default:
        return OneUiTerminalPointerActionMove;
    }
}

oneui::TerminalAuxiliaryButtonAction terminalAuxiliaryButtonAction(
    OneUiTerminalAuxiliaryButtonAction action) {
    switch (action) {
    case OneUiTerminalAuxiliaryButtonCopy:
        return oneui::TerminalAuxiliaryButtonAction::Copy;
    case OneUiTerminalAuxiliaryButtonPaste:
        return oneui::TerminalAuxiliaryButtonAction::Paste;
    case OneUiTerminalAuxiliaryButtonCallback:
        return oneui::TerminalAuxiliaryButtonAction::Callback;
    case OneUiTerminalAuxiliaryButtonIgnore:
    default:
        return oneui::TerminalAuxiliaryButtonAction::Ignore;
    }
}

} // namespace

OneUiWidget* oneui_log_view_create(void) {
    auto view = std::make_shared<oneui::LogView>();
    // 日志查看器默认接系统剪贴板，Ctrl+C 复制选中内容开箱即用。
    view->setClipboard(std::make_shared<oneui::SystemClipboard>());
    return wrap(view);
}

void oneui_log_view_append_line(OneUiWidget* view, const wchar_t* text, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeView = asWidget<oneui::LogView>(view);
    if (!nativeView) {
        return;
    }
    nativeView->appendLine(text ? text : L"", oneui::Color{r, g, b, a});
}

void oneui_log_view_clear(OneUiWidget* view) {
    auto* nativeView = asWidget<oneui::LogView>(view);
    if (!nativeView) {
        return;
    }
    nativeView->clearLines();
}

float oneui_log_view_content_height(OneUiWidget* view) {
    auto* nativeView = asWidget<oneui::LogView>(view);
    if (!nativeView) {
        return 0.0f;
    }
    return nativeView->contentHeight();
}

void oneui_log_view_set_font_size(OneUiWidget* view, float size) {
    if (auto* nativeView = asWidget<oneui::LogView>(view)) {
        nativeView->setFontSize(size);
    }
}

void oneui_log_view_set_line_height(OneUiWidget* view, float height) {
    if (auto* nativeView = asWidget<oneui::LogView>(view)) {
        nativeView->setLineHeight(height);
    }
}

OneUiWidget* oneui_terminal_view_create(void) {
    auto terminal = std::make_shared<oneui::TerminalView>();
    terminal->setClipboard(std::make_shared<oneui::SystemClipboard>());
    auto* wrapper = wrap(std::move(terminal));
    if (wrapper) {
        wrapper->tag = "terminal-view";
        applyCurrentStyleSheet(wrapper);
    }
    return wrapper;
}

void oneui_terminal_view_set_font_size(OneUiWidget* view, float size) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setFontSize(size);
    }
}

void oneui_terminal_view_set_font_family_utf8(
    OneUiWidget* view,
    OneUiUtf8String family) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setFontFamily(utf8OrEmpty(family));
    }
}

void oneui_terminal_view_set_line_height(OneUiWidget* view, float multiplier) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setLineHeight(multiplier);
    }
}

void oneui_terminal_view_set_letter_spacing(OneUiWidget* view, float pixels) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setLetterSpacing(pixels);
    }
}

void oneui_terminal_view_set_line_numbers_visible(OneUiWidget* view, int visible) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setLineNumbersVisible(visible != 0);
    }
}

void oneui_terminal_view_set_scrollback(OneUiWidget* view, unsigned long long history_rows, unsigned long long offset) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) nativeView->setScrollback(history_rows, offset);
}

void oneui_terminal_view_set_first_visible_line_number(
    OneUiWidget* view,
    unsigned long long line_number) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setFirstVisibleLineNumber(line_number);
    }
}

void oneui_terminal_view_set_cursor_style(OneUiWidget* view, OneUiTerminalCursorStyle style) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        const auto nativeStyle = style == OneUiTerminalCursorStyleBar
                                     ? oneui::TerminalCursorStyle::Bar
                                     : (style == OneUiTerminalCursorStyleUnderline
                                            ? oneui::TerminalCursorStyle::Underline
                                            : oneui::TerminalCursorStyle::Block);
        nativeView->setCursorStyle(nativeStyle);
    }
}

void oneui_terminal_view_set_cursor_blinking(OneUiWidget* view, int enabled) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setCursorBlinking(enabled != 0);
    }
}

void oneui_terminal_view_flash_bell(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->flashBell();
    }
}

void oneui_terminal_view_set_copy_on_select(OneUiWidget* view, int enabled) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setCopyOnSelect(enabled != 0);
    }
}

void oneui_terminal_view_set_right_button_action(
    OneUiWidget* view,
    OneUiTerminalAuxiliaryButtonAction action) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setRightButtonAction(terminalAuxiliaryButtonAction(action));
    }
}

void oneui_terminal_view_set_middle_button_action(
    OneUiWidget* view,
    OneUiTerminalAuxiliaryButtonAction action) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setMiddleButtonAction(terminalAuxiliaryButtonAction(action));
    }
}

void oneui_terminal_view_set_palette(
    OneUiWidget* view,
    OneUiColor background,
    OneUiColor foreground,
    OneUiColor cursor) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setPalette(toNativeColor(background), toNativeColor(foreground), toNativeColor(cursor));
    }
}

void oneui_terminal_view_set_grid_utf8(
    OneUiWidget* view,
    unsigned short rows,
    unsigned short columns,
    const OneUiTerminalCellUtf8* cells,
    size_t cell_count) {
    auto* nativeView = asWidget<oneui::TerminalView>(view);
    if (!nativeView) {
        return;
    }

    constexpr std::size_t kMaximumCells = 2'000'000;
    const std::size_t requested = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
    if (requested > kMaximumCells) {
        nativeView->setGrid(0, 0, {});
        return;
    }

    const std::size_t copied = cells ? std::min(cell_count, requested) : 0;
    std::vector<oneui::TerminalCell> nativeCells;
    nativeCells.reserve(copied);
    for (std::size_t index = 0; index < copied; ++index) {
        const OneUiTerminalCellUtf8& cell = cells[index];
        nativeCells.push_back(oneui::TerminalCell{
            utf8OrEmpty(cell.text),
            toNativeColor(cell.foreground),
            toNativeColor(cell.background),
            cell.style,
            cell.hyperlink_id,
            toNativeTerminalUnderlineStyle(cell.underline_style),
            toNativeColor(cell.underline_color),
            cell.underline_color_set != 0,
        });
    }
    nativeView->setGrid(rows, columns, std::move(nativeCells));
}

void oneui_terminal_view_update_cells_utf8(
    OneUiWidget* view,
    size_t first_cell,
    const OneUiTerminalCellUtf8* cells,
    size_t cell_count) {
    auto* nativeView = asWidget<oneui::TerminalView>(view);
    if (!nativeView || !cells || cell_count == 0 || first_cell >= nativeView->cellCount()) {
        return;
    }

    const std::size_t copied = std::min(cell_count, nativeView->cellCount() - first_cell);
    std::vector<oneui::TerminalCell> nativeCells;
    nativeCells.reserve(copied);
    for (std::size_t index = 0; index < copied; ++index) {
        const OneUiTerminalCellUtf8& cell = cells[index];
        nativeCells.push_back(oneui::TerminalCell{
            utf8OrEmpty(cell.text),
            toNativeColor(cell.foreground),
            toNativeColor(cell.background),
            cell.style,
            cell.hyperlink_id,
            toNativeTerminalUnderlineStyle(cell.underline_style),
            toNativeColor(cell.underline_color),
            cell.underline_color_set != 0,
        });
    }
    nativeView->updateCells(first_cell, std::move(nativeCells));
}

void oneui_terminal_view_set_cursor(
    OneUiWidget* view,
    unsigned short row,
    unsigned short column,
    int visible) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setCursor(oneui::TerminalCursor{row, column, visible != 0});
    }
}

int oneui_terminal_view_text_input_caret_rect(
    OneUiWidget* view,
    OneUiRect* out_rect) {
    const auto* nativeView = asWidget<oneui::TerminalView>(view);
    if (!nativeView || !out_rect) {
        return 0;
    }
    const oneui::Rect rect = nativeView->textInputCaretRect();
    *out_rect = OneUiRect{rect.x, rect.y, rect.width, rect.height};
    return 1;
}

void oneui_terminal_view_select_all(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->selectAll();
    }
}

int oneui_terminal_view_copy_selection(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        return nativeView->copySelectionToClipboard() ? 1 : 0;
    }
    return 0;
}

int oneui_terminal_view_paste_clipboard(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        return nativeView->pasteFromClipboard() ? 1 : 0;
    }
    return 0;
}

int oneui_terminal_view_paste_utf8(OneUiWidget* view, OneUiUtf8String text) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        return nativeView->pasteText(utf8OrEmpty(text)) ? 1 : 0;
    }
    return 0;
}

void oneui_terminal_view_set_selection(
    OneUiWidget* view,
    unsigned short start_row,
    unsigned short start_column,
    unsigned short end_row,
    unsigned short end_column) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setSelection(start_row, start_column, end_row, end_column);
    }
}

void oneui_terminal_view_clear_selection(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->clearSelection();
    }
}

int oneui_terminal_view_has_selection(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        return nativeView->hasSelection() ? 1 : 0;
    }
    return 0;
}

std::size_t oneui_terminal_view_get_selected_text_utf8(
    OneUiWidget* view,
    char* buffer,
    std::size_t buffer_len) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        const std::string value = utf8FromWide(nativeView->selectedText());
        copyUtf8Field(value, buffer, buffer_len);
        return value.size() + 1;
    }
    if (buffer && buffer_len > 0) {
        buffer[0] = '\0';
    }
    return 0;
}

void oneui_terminal_view_set_on_text_input_utf8(
    OneUiWidget* view,
    OneUiUtf8TextCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnTextInput(nullptr);
            return;
        }
        nativeView->setOnTextInput([callback, user_data](const std::wstring& text) {
            const std::string utf8 = utf8FromWide(text);
            callback(utf8.data(), utf8.size(), user_data);
        });
    }
}

void oneui_terminal_view_set_on_paste_utf8(
    OneUiWidget* view,
    OneUiUtf8TextCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnPaste(nullptr);
            return;
        }
        nativeView->setOnPaste([callback, user_data](const std::wstring& text) {
            const std::string utf8 = utf8FromWide(text);
            callback(utf8.data(), utf8.size(), user_data);
        });
    }
}

void oneui_terminal_view_set_on_raw_key(
    OneUiWidget* view,
    OneUiRawKeyCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnRawKey(nullptr);
            return;
        }
        nativeView->setOnRawKey([callback, user_data](const oneui::KeyEvent& event) {
            OneUiRawKeyEvent cEvent{};
            cEvent.virtual_key = event.virtualKey;
            cEvent.scan_code = event.scanCode;
            cEvent.pressed = event.pressed ? 1 : 0;
            cEvent.repeat = event.repeat ? 1 : 0;
            cEvent.extended = event.extended ? 1 : 0;
            cEvent.alt = event.alt ? 1 : 0;
            cEvent.ctrl = event.control ? 1 : 0;
            cEvent.shift = event.shift ? 1 : 0;
            cEvent.win = event.win ? 1 : 0;
            callback(&cEvent, user_data);
        });
    }
}

void oneui_terminal_view_set_scroll_rows_per_wheel(OneUiWidget* view, float rows) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setScrollRowsPerWheel(rows);
    }
}

void oneui_terminal_view_set_on_scroll(
    OneUiWidget* view,
    OneUiIntCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnScroll(nullptr);
            return;
        }
        nativeView->setOnScroll([callback, user_data](int rows) {
            callback(rows, user_data);
        });
    }
}

void oneui_terminal_view_set_mouse_reporting(OneUiWidget* view, int enabled) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setMouseReporting(enabled != 0);
    }
}

void oneui_terminal_view_set_on_pointer(
    OneUiWidget* view,
    OneUiTerminalPointerCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnPointer(nullptr);
            return;
        }
        nativeView->setOnPointer([callback, user_data](const oneui::TerminalPointerEvent& event) {
            const OneUiTerminalPointerEvent cEvent{
                toCTerminalPointerAction(event.action),
                toCButton(event.button),
                event.position.x,
                event.position.y,
                event.row,
                event.column,
                event.wheelDelta,
                event.shift ? 1 : 0,
                event.control ? 1 : 0,
                event.alt ? 1 : 0,
            };
            callback(&cEvent, user_data);
        });
    }
}

void oneui_terminal_view_set_on_hyperlink(
    OneUiWidget* view,
    OneUiTerminalHyperlinkCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnHyperlink(nullptr);
            return;
        }
        nativeView->setOnHyperlink([callback, user_data](std::uint32_t hyperlinkId) {
            callback(hyperlinkId, user_data);
        });
    }
}

void oneui_terminal_view_set_on_cursor_changed(OneUiWidget* view, OneUiVoidCallback callback, void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) nativeView->setOnCursorChanged(nullptr);
        else nativeView->setOnCursorChanged([callback, user_data] { callback(user_data); });
    }
}

void oneui_terminal_view_set_on_viewport_changed(
    OneUiWidget* view,
    OneUiTerminalViewportCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnViewportChanged(nullptr);
            return;
        }
        nativeView->setOnViewportChanged([callback, user_data](oneui::TerminalViewport viewport) {
            callback(viewport.rows, viewport.columns, user_data);
        });
    }
}

void oneui_terminal_view_set_on_focus_changed(
    OneUiWidget* view,
    OneUiBoolCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnFocusChanged(nullptr);
            return;
        }
        nativeView->setOnFocusChanged([callback, user_data](bool focused) {
            callback(focused ? 1 : 0, user_data);
        });
    }
}
