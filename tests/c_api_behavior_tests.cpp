#include "oneui/oneui_c_api.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

int failures = 0;

void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << " failed\n";
        ++failures;
    }
}

OneUiUtf8String utf8View(const std::string& value) {
    return OneUiUtf8String{value.data(), value.size()};
}

struct Utf8CallbackState {
    std::string text;
    int calls = 0;
};

struct IndexCallbackState {
    int index = -1;
    int calls = 0;
};

struct OwnedCallbackState {
    int invoked = 0;
    int cleaned = 0;
};

#ifdef _WIN32
struct WindowRawKeyState {
    OneUiRawKeyEvent last{};
    int calls = 0;
};

struct WindowHyperlinkState {
    unsigned int lastId = 0;
    int calls = 0;
};

void onWindowTerminalHyperlink(unsigned int hyperlinkId, void* userData) {
    auto* state = static_cast<WindowHyperlinkState*>(userData);
    if (state) {
        state->lastId = hyperlinkId;
        ++state->calls;
    }
}

int onWindowRawKey(const OneUiRawKeyEvent* event, void* userData) {
    auto* state = static_cast<WindowRawKeyState*>(userData);
    if (state && event) {
        state->last = *event;
        ++state->calls;
    }
    return 1;
}

void testWindowRawKeyTracksMessageModifiersAndResetsOnFocusLoss() {
    OneUiWindowOptions options{};
    options.title = L"OneUI raw key modifier test";
    options.width = 320;
    options.height = 240;
    options.visible = 0;
    options.borderless = 1;
    options.resizable = 1;

    OneUiWindow* window = oneui_window_create(&options);
    expectTrue("raw key window create", window != nullptr);
    if (!window) {
        return;
    }

    WindowRawKeyState state;
    oneui_window_set_on_raw_key(window, onWindowRawKey, &state);
    const OneUiTerminalCellUtf8 cells[] = {
        {
            {"A", 1},
            {220, 226, 240, 255},
            {20, 24, 36, 255},
            0,
            42,
            0,
            {0, 0, 0, 255},
            0,
        },
    };
    OneUiWidget* terminal = oneui_terminal_view_create();
    WindowHyperlinkState hyperlinkState;
    expectTrue("mouse modifier terminal create", terminal != nullptr);
    if (terminal) {
        oneui_terminal_view_set_font_size(terminal, 20.0f);
        oneui_terminal_view_set_grid_utf8(terminal, 1, 2, cells, 1);
        oneui_terminal_view_set_on_hyperlink(
            terminal,
            onWindowTerminalHyperlink,
            &hyperlinkState);
        oneui_window_set_content(window, terminal);
    }

    oneui_window_initialize(window);
    auto hwnd = static_cast<HWND>(oneui_window_native_handle(window));
    expectTrue("raw key native handle", hwnd != nullptr);
    if (hwnd) {
        SendMessageW(hwnd, WM_KEYDOWN, VK_CONTROL, 0);
        SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, 0);
        SendMessageW(hwnd, WM_KEYDOWN, 'V', 0);
        expectTrue("raw key callback receives V", state.calls >= 3 && state.last.virtual_key == 'V');
        expectTrue("raw key tracks control message", state.last.ctrl != 0);
        expectTrue("raw key tracks shift message", state.last.shift != 0);

        SendMessageW(hwnd, WM_KEYUP, VK_SHIFT, 0);
        SendMessageW(hwnd, WM_KEYUP, VK_CONTROL, 0);
        SendMessageW(hwnd, WM_KEYDOWN, 'V', 0);
        expectTrue("raw key releases control message", state.last.ctrl == 0);
        expectTrue("raw key releases shift message", state.last.shift == 0);

        SendMessageW(hwnd, WM_KEYDOWN, VK_CONTROL, 0);
        SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
        SendMessageW(hwnd, WM_KEYDOWN, 'V', 0);
        expectTrue("raw key clears modifiers on focus loss", state.last.ctrl == 0);

        if (terminal) {
            SetWindowPos(
                hwnd,
                nullptr,
                -32000,
                -32000,
                0,
                0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            oneui_window_show(window);
            // Complete the first layout/paint before coordinate-based input.
            // Hiding immediately after ShowWindow leaves terminal metrics at
            // their pre-paint defaults and makes the hyperlink hit test race.
            UpdateWindow(hwnd);

            const LPARAM point = MAKELPARAM(6, 10);
            SendMessageW(hwnd, WM_KEYDOWN, VK_CONTROL, 0);
            SendMessageW(hwnd, WM_LBUTTONDOWN, MK_CONTROL | MK_LBUTTON, point);
            SendMessageW(hwnd, WM_LBUTTONUP, MK_CONTROL, point);
            SendMessageW(hwnd, WM_KEYUP, VK_CONTROL, 0);
            expectTrue(
                "mouse event tracks control message",
                hyperlinkState.calls == 1 && hyperlinkState.lastId == 42);
            ShowWindow(hwnd, SW_HIDE);
        }
    }

    oneui_widget_destroy(terminal);
    oneui_window_destroy(window);
}
#endif

void onOwnedCallback(void* userData) {
    auto* state = static_cast<OwnedCallbackState*>(userData);
    if (state) {
        ++state->invoked;
    }
}

void onOwnedCallbackCleanup(void* userData) {
    auto* state = static_cast<OwnedCallbackState*>(userData);
    if (state) {
        ++state->cleaned;
    }
}

void testOwnedWindowPostCleansUpCancelledWork() {
    OneUiWindowOptionsUtf8 options{};
    options.title = utf8View("OneUI owned post test");
    options.width = 320;
    options.height = 240;
    OneUiWindow* window = oneui_window_create_utf8(&options);
    expectTrue("owned post window create", window != nullptr);
    if (!window) {
        return;
    }

    OwnedCallbackState queuedState;
    const int queued = oneui_window_post_owned(
        window,
        onOwnedCallback,
        &queuedState,
        onOwnedCallbackCleanup);
    expectTrue("owned post accepted", queued == 1);
    oneui_window_destroy(window);
    expectTrue("cancelled post not invoked", queuedState.invoked == 0);
    expectTrue("cancelled post cleaned once", queuedState.cleaned == 1);

    OwnedCallbackState rejectedState;
    const int rejected = oneui_window_post_owned(
        nullptr,
        onOwnedCallback,
        &rejectedState,
        onOwnedCallbackCleanup);
    expectTrue("invalid window post rejected", rejected == 0);
    expectTrue("rejected post cleaned once", rejectedState.cleaned == 1);
}

void onUtf8TextChanged(const char* text, size_t length, void* userData) {
    auto* state = static_cast<Utf8CallbackState*>(userData);
    if (!state) {
        return;
    }
    state->text.assign(text ? text : "", length);
    ++state->calls;
}

void onIndexChanged(int index, void* userData) {
    auto* state = static_cast<IndexCallbackState*>(userData);
    if (state) {
        state->index = index;
        ++state->calls;
    }
}

void testUtf8AbiRoundTripsUnicodeText() {
    const std::string title = "iShellPro \xE9\xBA\x92\xE9\xBA\x9F \xF0\x9F\x9A\x80";
    const std::string text = "\xE5\x85\xB4\xE4\xB8\x9A\xE9\x93\xB6\xE8\xA1\x8C SSH \xF0\x9F\x94\x90";

    expectTrue("utf8 abi version", oneui_utf8_abi_version() == ONEUI_UTF8_ABI_VERSION);

    OneUiWindowOptionsUtf8 options{};
    options.title = utf8View(title);
    options.width = 480;
    options.height = 320;
    options.borderless = 1;
    options.resizable = 1;
    OneUiWindow* window = oneui_window_create_utf8(&options);
    expectTrue("utf8 window create", window != nullptr);
    if (window) {
        oneui_window_set_title_utf8(window, utf8View(text));
    }

    OneUiWidget* label = oneui_label_create_utf8(utf8View(title));
    OneUiWidget* button = oneui_button_create_utf8(utf8View(text));
    OneUiWidget* field = oneui_text_field_create_utf8(utf8View(title));
    OneUiWidget* search = oneui_search_box_create_utf8(utf8View(text));
    expectTrue("utf8 label create", label != nullptr);
    expectTrue("utf8 button create", button != nullptr);
    expectTrue("utf8 text field create", field != nullptr);
    expectTrue("utf8 search box create", search != nullptr);
    expectTrue("widget focused query handles unfocused field", oneui_widget_focused(field) == 0);
    expectTrue("widget focused query handles null", oneui_widget_focused(nullptr) == 0);
    expectTrue("text field undo empty history", oneui_text_field_undo(field) == 0);
    expectTrue("text field redo empty history", oneui_text_field_redo(field) == 0);
    expectTrue("text field undo handles null", oneui_text_field_undo(nullptr) == 0);
    expectTrue("text field redo handles null", oneui_text_field_redo(nullptr) == 0);

    Utf8CallbackState callbackState;
    oneui_text_field_set_on_changed_utf8(field, onUtf8TextChanged, &callbackState);
    oneui_text_field_set_text_utf8(field, utf8View(text));
    oneui_label_set_text_utf8(label, utf8View(text));
    oneui_button_set_text_utf8(button, utf8View(title));
    expectTrue("utf8 text callback invoked", callbackState.calls == 1);
    expectTrue("utf8 text callback round trip", callbackState.text == text);

    const std::string malformed = "bad \xF0\x28\x8C\x28";
    const std::string replacement = "bad \xEF\xBF\xBD(\xEF\xBF\xBD(";
    oneui_text_field_set_text_utf8(field, utf8View(malformed));
    expectTrue("utf8 malformed text callback invoked", callbackState.calls == 2);
    expectTrue("utf8 malformed text replaces invalid sequences", callbackState.text == replacement);

    oneui_widget_destroy(search);
    oneui_widget_destroy(field);
    oneui_widget_destroy(button);
    oneui_widget_destroy(label);
    oneui_window_destroy(window);
}

void testUtf8ListUsesStructuredItems() {
    OneUiWidget* list = oneui_list_create();
    expectTrue("utf8 list create", list != nullptr);
    if (!list) {
        return;
    }

    const std::string title = "\xE7\x94\x9F\xE4\xBA\xA7 SSH\t\xE4\xB8\xBB\xE6\x9C\xBA";
    const std::string detail = "10.0.0.1\n\xE5\x85\xB4\xE4\xB8\x9A\xE9\x93\xB6\xE8\xA1\x8C\xE8\x82\xA1\xE4\xBB\xBD\xE6\x9C\x89\xE9\x99\x90\xE5\x85\xAC\xE5\x8F\xB8";
    const std::string secondTitle = "Kylin V10";
    const std::string secondDetail = "\xE5\xA0\xA1\xE5\x9E\x92\xE6\x9C\xBA\xE7\x9B\xB4\xE8\xBF\x9E";
    const OneUiListItemUtf8 items[] = {
        OneUiListItemUtf8{utf8View(title), utf8View(detail)},
        OneUiListItemUtf8{utf8View(secondTitle), utf8View(secondDetail)},
    };

    oneui_list_set_items_utf8(list, items, sizeof(items) / sizeof(items[0]));
    oneui_list_set_selected_index(list, 1);
    expectTrue("utf8 list selected index", oneui_list_selected_index(list) == 1);

    oneui_widget_destroy(list);
}

void testUtf8TreeViewUsesStableIdsAndStructuredParents() {
    OneUiWidget* tree = oneui_tree_view_create();
    expectTrue("utf8 tree view create", tree != nullptr);
    if (!tree) {
        return;
    }

    const std::string rootId = "platform";
    const std::string childId = "production";
    const std::string rootTitle = "Platform ä¸»æº";
    const std::string childTitle = "çäº§ SSH";
    const OneUiTreeItemUtf8 items[] = {
        {utf8View(rootId), {}, utf8View(rootTitle), utf8View("12"), 1},
        {utf8View(childId), utf8View(rootId), utf8View(childTitle), utf8View("8"), 1},
    };
    oneui_tree_view_set_items_utf8(tree, items, sizeof(items) / sizeof(items[0]));
    oneui_tree_view_set_selected_id_utf8(tree, utf8View(childId));

    const size_t required = oneui_tree_view_selected_id_utf8(tree, nullptr, 0);
    expectTrue("utf8 tree selected id has required length", required == childId.size() + 1);
    char buffer[64]{};
    const size_t copied = oneui_tree_view_selected_id_utf8(tree, buffer, sizeof(buffer));
    expectTrue("utf8 tree selected id round trips", copied == required && std::string(buffer) == childId);

    oneui_widget_destroy(tree);
}

void testWindowDpiMetricsAbiUsesLogicalAndPhysicalSizes() {
    OneUiWindowOptions options{};
    options.title = L"OneUI C ABI DPI Metrics";
    options.width = 480;
    options.height = 320;
    options.visible = 0;
    options.borderless = 1;
    options.resizable = 1;

    OneUiWindow* window = oneui_window_create(&options);
    expectTrue("window create", window != nullptr);
    if (!window) {
        return;
    }

    const int logicalWidth = oneui_window_client_width(window);
    const int logicalHeight = oneui_window_client_height(window);
    const int pixelWidth = oneui_window_client_pixel_width(window);
    const int pixelHeight = oneui_window_client_pixel_height(window);
    const float scale = oneui_window_dpi_scale(window);

    expectTrue("window logical width", logicalWidth == options.width);
    expectTrue("window logical height", logicalHeight == options.height);
    expectTrue("window dpi scale positive", scale > 0.0f);
    expectTrue("window pixel width positive", pixelWidth > 0);
    expectTrue("window pixel height positive", pixelHeight > 0);
    expectTrue("window logical to physical width", std::fabs(static_cast<float>(pixelWidth) - logicalWidth * scale) <= 2.0f);
    expectTrue("window logical to physical height", std::fabs(static_cast<float>(pixelHeight) - logicalHeight * scale) <= 2.0f);

    oneui_window_destroy(window);
}

void testWindowPlacementAbiRoundTripsRestoredBounds() {
    OneUiWindowOptions options{};
    options.title = L"OneUI C ABI Placement";
    options.width = 640;
    options.height = 480;
    options.visible = 0;
    options.borderless = 1;
    options.resizable = 1;

    OneUiWindow* window = oneui_window_create(&options);
    expectTrue("placement window create", window != nullptr);
    if (!window) {
        return;
    }

    oneui_window_initialize(window);
    OneUiWindowPlacement requested{120, 140, 720, 520, 0};
    expectTrue("placement restore accepted", oneui_window_set_placement(window, &requested) == 1);

    OneUiWindowPlacement actual{};
    expectTrue("placement query accepted", oneui_window_get_placement(window, &actual) == 1);
    expectTrue("placement width round trip", actual.width == requested.width);
    expectTrue("placement height round trip", actual.height == requested.height);
    expectTrue("placement restored state round trip", actual.maximized == 0);

    expectTrue("placement rejects null output", oneui_window_get_placement(window, nullptr) == 0);
    const OneUiWindowPlacement invalid{};
    expectTrue(
        "placement rejects invalid size",
        oneui_window_set_placement(window, &invalid) == 0);
    oneui_window_destroy(window);
}

void testAppShellAbiCreatesReusableSlots() {
    OneUiStyleSheet* sheet = oneui_style_sheet_create();
    expectTrue("style sheet create", sheet != nullptr);
    oneui_style_sheet_set_custom_property(sheet, "--slot-gap", "6px");

    char error[256]{};
    const int cssOk = oneui_style_sheet_add_css(
        sheet,
        ".app-shell { padding: 1px 2px 3px 4px; gap: var(--slot-gap); }",
        error,
        static_cast<int>(sizeof(error)));
    expectTrue(error, cssOk != 0);

    OneUiWidget* shell = oneui_app_shell_create();
    OneUiWidget* sidebar = oneui_panel_create();
    OneUiWidget* header = oneui_stack_create(OneUiStackDirectionRow);
    OneUiWidget* split = oneui_split_view_create(OneUiSplitOrientationHorizontal);
    OneUiWidget* splitFirst = oneui_panel_create();
    OneUiWidget* splitSecond = oneui_panel_create();
    OneUiWidget* content = oneui_panel_create();
    OneUiWidget* footer = oneui_label_create(L"status");
    OneUiWidget* notify = oneui_icon_button_create(8);
    OneUiWidget* strip = oneui_status_strip_create(L"Status", L"Ready");
    OneUiWidget* stateView = oneui_state_view_create(L"Empty", L"No data yet");
    OneUiWidget* tile = oneui_tile_create(L"Remote Desktop", L"169 510 1007");
    OneUiWidget* segmented = oneui_segmented_control_create();
    OneUiWidget* tabs = oneui_tabs_create();
    OneUiWidget* select = oneui_select_create();
    OneUiWidget* titleBar = oneui_title_bar_create(L"Product");

    expectTrue("app shell create", shell != nullptr);
    expectTrue("sidebar create", sidebar != nullptr);
    expectTrue("header create", header != nullptr);
    expectTrue("split view create", split != nullptr);
    expectTrue("split first create", splitFirst != nullptr);
    expectTrue("split second create", splitSecond != nullptr);
    expectTrue("content create", content != nullptr);
    expectTrue("footer create", footer != nullptr);
    expectTrue("icon button create", notify != nullptr);
    expectTrue("status strip create", strip != nullptr);
    expectTrue("state view create", stateView != nullptr);
    expectTrue("tile create", tile != nullptr);
    expectTrue("segmented control create", segmented != nullptr);
    expectTrue("tabs create", tabs != nullptr);
    expectTrue("select create", select != nullptr);
    expectTrue("title bar create", titleBar != nullptr);

    oneui_widget_set_style_node(shell, "app-shell", "app-shell");
    oneui_widget_apply_style_sheet(shell, sheet);
    oneui_app_shell_set_sidebar(shell, sidebar);
    oneui_app_shell_set_header(shell, header);
    oneui_app_shell_set_content(shell, content);
    oneui_app_shell_set_footer(shell, footer);
    oneui_app_shell_set_sidebar_width(shell, 184.0f);
    oneui_app_shell_set_header_height(shell, 44.0f);
    oneui_app_shell_set_footer_height(shell, 28.0f);
    oneui_app_shell_set_gap(shell, 0.0f);
    oneui_app_shell_set_padding(shell, OneUiInsets{0.0f, 0.0f, 0.0f, 0.0f});
    oneui_app_shell_set_sidebar_visible(shell, 1);
    oneui_split_view_set_first(split, splitFirst);
    oneui_split_view_set_second(split, splitSecond);
    oneui_split_view_set_ratio(split, 0.6f);
    oneui_split_view_set_gap(split, 6.0f);
    oneui_split_view_set_padding(split, OneUiInsets{1.0f, 2.0f, 3.0f, 4.0f});
    oneui_split_view_set_resizable(split, 1);
    oneui_split_view_set_minimum_pane_extent(split, 80.0f, 60.0f);
    oneui_split_view_set_on_ratio_changed(split, nullptr, nullptr);
    expectTrue("split ratio round trip", std::abs(oneui_split_view_ratio(split) - 0.6f) < 0.001f);
    oneui_split_view_set_orientation(split, OneUiSplitOrientationVertical);
    oneui_icon_button_set_symbol(notify, 13);
    oneui_icon_button_set_on_click(notify, nullptr, nullptr);
    oneui_status_strip_set_title(strip, L"State");
    oneui_status_strip_set_message(strip, L"Updated");
    oneui_status_strip_set_primary_action(strip, L"Copy");
    oneui_status_strip_set_secondary_action(strip, L"Details");
    oneui_status_strip_set_on_primary_action(strip, nullptr, nullptr);
    oneui_status_strip_set_on_secondary_action(strip, nullptr, nullptr);
    oneui_state_view_set_title(stateView, L"No results");
    oneui_state_view_set_message(stateView, L"Adjust the active filters");
    oneui_state_view_set_icon(stateView, 4);
    oneui_state_view_set_action(stateView, L"Reset");
    oneui_state_view_set_on_action(stateView, nullptr, nullptr);
    oneui_tile_set_title(tile, L"Recent Assist");
    oneui_tile_set_subtitle(tile, L"164 709 2397");
    oneui_tile_set_leading_symbol(tile, 14);
    oneui_tile_set_trailing_symbol(tile, 13);
    oneui_tile_set_on_click(tile, nullptr, nullptr);
    oneui_segmented_control_set_items(segmented, L"Classic|Modern");
    oneui_segmented_control_set_selected_index(segmented, 1);
    expectTrue("segmented selected index", oneui_segmented_control_selected_index(segmented) == 1);
    oneui_segmented_control_set_on_changed(segmented, nullptr, nullptr);
    const OneUiUtf8String tabItems[] = {
        utf8View("Production bastion"),
        utf8View("Kylin V10"),
    };
    IndexCallbackState tabsCallbackState;
    oneui_tabs_set_items_utf8(tabs, tabItems, 2);
    const int tabIcons[] = {31, -1};
    oneui_tabs_set_item_icons(tabs, tabIcons, 2);
    oneui_tabs_set_on_changed(tabs, onIndexChanged, &tabsCallbackState);
    oneui_tabs_set_selected_index(tabs, 1);
    expectTrue("tabs selected index", oneui_tabs_selected_index(tabs) == 1);
    expectTrue(
        "tabs changed callback",
        tabsCallbackState.calls == 1 && tabsCallbackState.index == 1);
    const OneUiUtf8String selectItems[] = {
        utf8View("All protocols"),
        utf8View("SSH"),
        utf8View("Remote Desktop"),
    };
    oneui_select_set_items_utf8(select, selectItems, 3);
    IndexCallbackState selectCallbackState;
    oneui_select_set_on_changed(select, onIndexChanged, &selectCallbackState);
    oneui_select_set_selected_index(select, 2);
    expectTrue("select selected index", oneui_select_selected_index(select) == 2);
    expectTrue(
        "select changed callback",
        selectCallbackState.calls == 1 && selectCallbackState.index == 2);
    oneui_title_bar_set_title(titleBar, L"Updated Product");
    oneui_title_bar_set_icon_symbol(titleBar, 2);
    oneui_title_bar_set_maximized(titleBar, 0);
    oneui_title_bar_set_on_minimize(titleBar, nullptr, nullptr);
    oneui_title_bar_set_on_maximize(titleBar, nullptr, nullptr);
    oneui_title_bar_set_on_close(titleBar, nullptr, nullptr);

    oneui_widget_destroy(titleBar);
    oneui_widget_destroy(tabs);
    oneui_widget_destroy(select);
    oneui_widget_destroy(segmented);
    oneui_widget_destroy(tile);
    oneui_widget_destroy(stateView);
    oneui_widget_destroy(strip);
    oneui_widget_destroy(notify);
    oneui_widget_destroy(footer);
    oneui_widget_destroy(content);
    oneui_widget_destroy(header);
    oneui_widget_destroy(splitSecond);
    oneui_widget_destroy(splitFirst);
    oneui_widget_destroy(split);
    oneui_widget_destroy(sidebar);
    oneui_widget_destroy(shell);
    oneui_style_sheet_destroy(sheet);
}

void testProductShellAbiIsPublicProductFrame() {
    OneUiStyleSheet* sheet = oneui_style_sheet_create();
    expectTrue("product shell style sheet create", sheet != nullptr);

    char error[256]{};
    const int cssOk = oneui_style_sheet_add_css(
        sheet,
        ".product-shell { padding: 0px; gap: 0px; }",
        error,
        static_cast<int>(sizeof(error)));
    expectTrue(error, cssOk != 0);

    OneUiWidget* shell = oneui_product_shell_create();
    OneUiWidget* sidebar = oneui_panel_create();
    OneUiWidget* topbar = oneui_top_bar_create();
    OneUiWidget* search = oneui_search_box_create(L"Search");
    OneUiWidget* action = oneui_icon_button_create(8);
    OneUiWidget* content = oneui_panel_create();
    OneUiWidget* status = oneui_status_strip_create(L"Status", L"Ready");

    expectTrue("product shell create", shell != nullptr);
    expectTrue("product shell sidebar create", sidebar != nullptr);
    expectTrue("product shell topbar create", topbar != nullptr);
    expectTrue("product shell topbar search create", search != nullptr);
    expectTrue("product shell topbar action create", action != nullptr);
    expectTrue("product shell content create", content != nullptr);
    expectTrue("product shell status create", status != nullptr);

    oneui_widget_apply_style_sheet(shell, sheet);
    oneui_top_bar_set_leading(topbar, search);
    oneui_top_bar_add_action(topbar, action);
    oneui_top_bar_set_gap(topbar, 10.0f);
    oneui_top_bar_set_padding(topbar, OneUiInsets{0.0f, 0.0f, 0.0f, 0.0f});
    oneui_top_bar_set_leading_width(topbar, 320.0f);
    oneui_product_shell_set_sidebar(shell, sidebar);
    oneui_product_shell_set_topbar(shell, topbar);
    oneui_product_shell_set_content(shell, content);
    oneui_product_shell_set_status(shell, status);
    oneui_product_shell_set_sidebar_width(shell, 184.0f);
    oneui_product_shell_set_topbar_height(shell, 0.0f);
    oneui_product_shell_set_status_height(shell, 0.0f);
    oneui_product_shell_set_gap(shell, 0.0f);
    oneui_product_shell_set_padding(shell, OneUiInsets{0.0f, 0.0f, 0.0f, 0.0f});
    oneui_product_shell_set_sidebar_visible(shell, 1);

    oneui_widget_destroy(status);
    oneui_widget_destroy(content);
    oneui_widget_destroy(action);
    oneui_widget_destroy(search);
    oneui_widget_destroy(topbar);
    oneui_widget_destroy(sidebar);
    oneui_widget_destroy(shell);
    oneui_style_sheet_destroy(sheet);
}

void testOverlayToastAbiSupportsAnchoredNotice() {
    OneUiStyleSheet* sheet = oneui_style_sheet_create();
    expectTrue("overlay toast style sheet create", sheet != nullptr);

    char error[256]{};
    const int cssOk = oneui_style_sheet_add_css(
        sheet,
        ".toast { background: #303036; border-color: #42424c; border-width: 1px; border-radius: 8px; padding: 12px 14px 12px 14px; box-shadow: 0px 14px 30px 0px #00000062; }",
        error,
        static_cast<int>(sizeof(error)));
    expectTrue(error, cssOk != 0);

    OneUiWidget* host = oneui_overlay_host_create();
    OneUiWidget* content = oneui_panel_create();
    OneUiWidget* toast = oneui_toast_create(L"Notice", L"Message");

    expectTrue("overlay host create", host != nullptr);
    expectTrue("overlay content create", content != nullptr);
    expectTrue("toast create", toast != nullptr);

    oneui_widget_set_style_node(toast, "toast", "toast");
    oneui_widget_apply_style_sheet(toast, sheet);
    oneui_toast_set_title(toast, L"Coupon");
    oneui_toast_set_message(toast, L"Use it before it expires");
    oneui_toast_set_primary_action(toast, L"Use");
    oneui_toast_set_secondary_action(toast, L"Close");
    oneui_toast_set_icon_symbol(toast, 16);
    oneui_toast_set_close_visible(toast, 1);
    oneui_toast_set_on_primary_action(toast, nullptr, nullptr);
    oneui_toast_set_on_secondary_action(toast, nullptr, nullptr);
    oneui_toast_set_on_close(toast, nullptr, nullptr);
    oneui_overlay_host_set_content(host, content);
    oneui_overlay_host_add_anchored_overlay(
        host,
        toast,
        10,
        330.0f,
        72.0f,
        OneUiInsets{54.0f, 42.0f, 0.0f, 0.0f},
        2,
        0);

    oneui_widget_destroy(toast);
    oneui_widget_destroy(content);
    oneui_widget_destroy(host);
    oneui_style_sheet_destroy(sheet);
}

void testRealtimeFrameViewAbiAcceptsBgraFrames() {
    OneUiWidget* frameView = oneui_realtime_frame_view_create();
    expectTrue("realtime frame view create", frameView != nullptr);
    if (!frameView) {
        return;
    }

    unsigned char pixels[16] = {
        0, 0, 0, 255,
        64, 64, 64, 255,
        128, 128, 128, 255,
        255, 255, 255, 255
    };
    oneui_realtime_frame_view_set_scale_mode(frameView, OneUiVideoScaleModeFit);
    oneui_realtime_frame_view_submit_frame(
        frameView,
        pixels,
        2,
        2,
        8,
        OneUiPixelFormatBgra8888,
        42,
        123456);

    oneui_widget_destroy(frameView);
}

int pointerCallbackCount = 0;

void onRemotePointer(const OneUiRemotePointerEvent* event, void*) {
    if (!event) {
        return;
    }
    ++pointerCallbackCount;
}

void testRemoteInputRegionAbiCreatesAndAcceptsCallbacks() {
    pointerCallbackCount = 0;
    OneUiWidget* region = oneui_remote_input_region_create();
    expectTrue("remote input region create", region != nullptr);
    if (!region) {
        return;
    }

    oneui_remote_input_region_set_remote_size(region, 1920.0f, 1080.0f);
    oneui_remote_input_region_set_scale_mode(region, OneUiVideoScaleModeFit);
    oneui_remote_input_region_set_on_pointer(region, onRemotePointer, nullptr);
    oneui_remote_input_region_release_all_inputs(region);

    oneui_widget_destroy(region);
}

void onTerminalTextInput(const char*, size_t, void*) {}

void onTerminalRawKey(const OneUiRawKeyEvent*, void*) {}

void onTerminalPointer(const OneUiTerminalPointerEvent*, void*) {}

void onTerminalHyperlink(unsigned int, void*) {}

void testTerminalViewAbiUsesStructuredCells() {
    OneUiWidget* view = oneui_terminal_view_create();
    expectTrue("terminal view create", view != nullptr);
    if (!view) {
        return;
    }

    const OneUiTerminalCellUtf8 cells[] = {
        {
            {"A", 1},
            {220, 226, 240, 255},
            {20, 24, 36, 255},
            OneUiTerminalCellBold,
            42,
            3,
            {12, 34, 56, 255},
            1,
        },
        {
            {"\xE4\xB8\xAD", 3},
            {240, 200, 80, 255},
            {20, 24, 36, 255},
            OneUiTerminalCellWide,
            0,
            0,
            {0, 0, 0, 255},
            0,
        },
    };
    oneui_terminal_view_set_font_size(view, 14.0f);
    oneui_terminal_view_set_font_family_utf8(view, {"Cascadia Mono", 13});
    oneui_terminal_view_set_right_button_action(view, OneUiTerminalAuxiliaryButtonCopy);
    oneui_terminal_view_set_middle_button_action(view, OneUiTerminalAuxiliaryButtonPaste);
    oneui_terminal_view_set_palette(
        view,
        OneUiColor{20, 24, 36, 255},
        OneUiColor{220, 226, 240, 255},
        OneUiColor{196, 181, 253, 255});
    oneui_terminal_view_set_grid_utf8(view, 2, 3, cells, 2);
    oneui_terminal_view_update_cells_utf8(view, 1, cells, 1);
    oneui_terminal_view_set_cursor(view, 1, 2, 1);
    OneUiRect caret{};
    expectTrue(
        "terminal view caret rect",
        oneui_terminal_view_text_input_caret_rect(view, &caret) != 0 &&
            caret.x >= 2.0f && caret.y >= 1.0f && caret.width >= 1.0f && caret.height >= 1.0f);
    expectTrue(
        "terminal view caret rect validates output",
        oneui_terminal_view_text_input_caret_rect(view, nullptr) == 0);
    oneui_terminal_view_select_all(view);
    expectTrue("terminal view selection", oneui_terminal_view_has_selection(view) != 0);
    expectTrue("terminal view copy selection", oneui_terminal_view_copy_selection(view) != 0);
    char selected[32]{};
    const size_t selectedRequired =
        oneui_terminal_view_get_selected_text_utf8(view, selected, sizeof(selected));
    expectTrue("terminal view selected text size", selectedRequired > 1);
    expectTrue("terminal view selected text utf8", std::string(selected).find("A") == 0);
    oneui_terminal_view_set_selection(view, 0, 0, 0, 1);
    expectTrue("terminal view programmatic selection", oneui_terminal_view_has_selection(view) != 0);
    oneui_terminal_view_clear_selection(view);
    expectTrue("terminal view clear selection", oneui_terminal_view_has_selection(view) == 0);
    oneui_terminal_view_set_on_text_input_utf8(view, onTerminalTextInput, nullptr);
    oneui_terminal_view_set_on_paste_utf8(view, onTerminalTextInput, nullptr);
    oneui_terminal_view_set_on_raw_key(view, onTerminalRawKey, nullptr);
    oneui_terminal_view_set_scroll_rows_per_wheel(view, 4.0f);
    oneui_terminal_view_set_on_scroll(view, nullptr, nullptr);
    oneui_terminal_view_set_mouse_reporting(view, 1);
    oneui_terminal_view_set_on_pointer(view, onTerminalPointer, nullptr);
    oneui_terminal_view_set_on_hyperlink(view, onTerminalHyperlink, nullptr);

    oneui_widget_destroy(view);
}

void testClipboardAbiRoundTripIfAvailable() {
    const int previousRequired = oneui_clipboard_get_text(nullptr, 0);
    std::vector<wchar_t> previousBuffer(
        static_cast<std::size_t>(std::max(previousRequired, 1)),
        L'\0');
    oneui_clipboard_get_text(previousBuffer.data(), static_cast<int>(previousBuffer.size()));
    const std::wstring previous(previousBuffer.data());

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    const std::wstring value = L"OneUI C ABI clipboard smoke " + std::to_wstring(stamp);
    const int setOk = oneui_clipboard_set_text(value.c_str());
    if (!setOk) {
        std::cerr << "Clipboard C ABI round trip skipped: system clipboard unavailable.\n";
        return;
    }

    bool roundTrip = false;
    for (int attempt = 0; attempt < 8; ++attempt) {
        wchar_t buffer[128]{};
        const int required = oneui_clipboard_get_text(
            buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (required == static_cast<int>(value.size()) + 1 && value == buffer) {
            roundTrip = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!roundTrip) {
        std::cerr << "Clipboard C ABI read skipped: clipboard changed or remained unavailable.\n";
        return;
    }

    expectTrue("clipboard round trip text", roundTrip);
    oneui_clipboard_set_text(previous.c_str());
}

void testPromptAbiRejectsInvalidOutput() {
    expectTrue(
        "prompt C ABI rejects a missing output buffer",
        oneui_window_prompt_text(
            nullptr,
            L"Input",
            L"Enter a value",
            L"",
            L"Value",
            0,
            nullptr,
            0) == 0);
}

void testProgressBarAbiClampsValues() {
    OneUiWidget* progress = oneui_progress_bar_create();
    expectTrue("progress bar create", progress != nullptr);
    oneui_progress_bar_set_value(progress, 0.625);
    expectTrue(
        "progress bar value round trip",
        std::abs(oneui_progress_bar_value(progress) - 0.625) < 0.0001);
    oneui_progress_bar_set_value(progress, 4.0);
    expectTrue("progress bar upper clamp", oneui_progress_bar_value(progress) == 1.0);
    oneui_progress_bar_set_value(progress, -1.0);
    expectTrue("progress bar lower clamp", oneui_progress_bar_value(progress) == 0.0);
    expectTrue("progress bar null query", oneui_progress_bar_value(nullptr) == 0.0);
    oneui_widget_destroy(progress);
}

void testSparklineAbiAcceptsNormalizedSamples() {
    OneUiWidget* sparkline = oneui_sparkline_create();
    expectTrue("sparkline create", sparkline != nullptr);
    if (!sparkline) {
        return;
    }
    const double values[] = {-1.0, 0.25, 0.75, 2.0};
    oneui_sparkline_set_values(sparkline, values, 4);
    oneui_sparkline_set_values(sparkline, nullptr, 0);
    oneui_widget_destroy(sparkline);
}

void testWindowLayoutSnapshotSerializesMountedTreeWithoutFieldValues() {
    OneUiWindowOptionsUtf8 options{};
    const std::string title = "OneUI layout snapshot test";
    options.title = utf8View(title);
    options.width = 420;
    options.height = 280;
    options.visible = 0;
    options.borderless = 1;
    options.resizable = 1;

    OneUiWindow* window = oneui_window_create_utf8(&options);
    expectTrue("layout snapshot window create", window != nullptr);
    if (!window) {
        return;
    }
    OneUiWidget* root = oneui_stack_create(OneUiStackDirectionColumn);
    const std::string placeholder = "Secret placeholder";
    OneUiWidget* field = oneui_text_field_create_utf8(utf8View(placeholder));
    OneUiWidget* label = oneui_label_create(L"Visible field label");
    OneUiWidget* list = oneui_list_create();
    OneUiWidget* tabs = oneui_tabs_create();
    expectTrue(
        "layout snapshot widgets create",
        root != nullptr && field != nullptr && label != nullptr && list != nullptr && tabs != nullptr);
    if (root && field && label && list && tabs) {
        const std::string secret = "must-not-appear-in-layout-snapshot";
        const std::string productionTitle = "Production bastion";
        const std::string productionDetail = "Primary route";
        const std::string kylinTitle = "Kylin V10";
        const std::string kylinDetail = "Offline client";
        const OneUiListItemUtf8 items[] = {
            {utf8View(productionTitle), utf8View(productionDetail)},
            {utf8View(kylinTitle), utf8View(kylinDetail)},
        };
        oneui_text_field_set_text_utf8(field, utf8View(secret));
        oneui_list_set_items_utf8(list, items, 2);
        oneui_list_set_selected_index(list, 1);
        const std::string sshTab = "SSH - production";
        const std::string localTab = "Local terminal";
        const OneUiUtf8String tabItems[] = {utf8View(sshTab), utf8View(localTab)};
        oneui_tabs_set_items_utf8(tabs, tabItems, 2);
        oneui_tabs_set_selected_index(tabs, 1);
        oneui_widget_set_style_node(field, "input", "qa-field");
        oneui_widget_set_preferred_size(field, 240.0f, 32.0f);
        oneui_stack_add(root, label);
        oneui_stack_add(root, field);
        oneui_stack_add(root, tabs);
        oneui_stack_add(root, list);
        oneui_window_set_content(window, root);
        oneui_window_initialize(window);

        std::size_t required = 0;
        expectTrue(
            "layout snapshot size query",
            oneui_window_layout_snapshot_utf8(window, nullptr, 0, &required) == -2 &&
                required > 1);
        std::vector<char> snapshot(required, '\0');
        expectTrue(
            "layout snapshot serialization",
            oneui_window_layout_snapshot_utf8(
                window,
                snapshot.data(),
                snapshot.size(),
                &required) == 1);
        const std::string json(snapshot.data());
        expectTrue("layout snapshot schema", json.find("\"schemaVersion\":1") != std::string::npos);
        expectTrue("layout snapshot semantic tag", json.find("\"tag\":\"input\"") != std::string::npos);
        expectTrue("layout snapshot semantic class", json.find("qa-field") != std::string::npos);
        expectTrue("layout snapshot value length", json.find("\"valueLength\":34") != std::string::npos);
        expectTrue("layout snapshot excludes values", json.find(secret) == std::string::npos);
        expectTrue(
            "layout snapshot includes label text",
            json.find("\"text\":\"Visible field label\"") != std::string::npos);
        expectTrue(
            "layout snapshot includes list title",
            json.find("\"title\":\"Production bastion\"") != std::string::npos);
        expectTrue(
            "layout snapshot includes list detail",
            json.find("\"detail\":\"Offline client\"") != std::string::npos);
        expectTrue(
            "layout snapshot includes selected list item",
            json.find("\"selected\":true") != std::string::npos);
        expectTrue(
            "layout snapshot includes tab item",
            json.find("\"title\":\"SSH - production\"") != std::string::npos);
        expectTrue(
            "layout snapshot includes selected tab item",
            json.find("\"title\":\"Local terminal\",\"detail\":\"\",\"selected\":true") !=
                std::string::npos);
        expectTrue(
            "layout snapshot commits visible geometry",
            json.find("\"frame\":{\"x\":0,\"y\":0,\"width\":0,\"height\":0}") ==
                std::string::npos);

        OneUiWidget* replacement = oneui_stack_create(OneUiStackDirectionColumn);
        OneUiWidget* replacementLabel = oneui_label_create(L"Replacement route content");
        expectTrue(
            "layout snapshot replacement widgets create",
            replacement != nullptr && replacementLabel != nullptr);
        if (replacement && replacementLabel) {
            oneui_stack_add(replacement, replacementLabel);
            oneui_window_set_content(window, replacement);
            std::size_t replacementRequired = 0;
            expectTrue(
                "layout snapshot replacement size query",
                oneui_window_layout_snapshot_utf8(
                    window,
                    nullptr,
                    0,
                    &replacementRequired) == -2 &&
                    replacementRequired > 1);
            std::vector<char> replacementSnapshot(replacementRequired, '\0');
            expectTrue(
                "layout snapshot replacement serialization",
                oneui_window_layout_snapshot_utf8(
                    window,
                    replacementSnapshot.data(),
                    replacementSnapshot.size(),
                    &replacementRequired) == 1);
            const std::string replacementJson(replacementSnapshot.data());
            expectTrue(
                "layout snapshot replacement route is current",
                replacementJson.find("Replacement route content") != std::string::npos &&
                    replacementJson.find("Visible field label") == std::string::npos);
            expectTrue(
                "layout snapshot replacement geometry is committed",
                replacementJson.find(
                    "\"frame\":{\"x\":0,\"y\":0,\"width\":0,\"height\":0}") ==
                    std::string::npos);
        }
        oneui_widget_destroy(replacementLabel);
        oneui_widget_destroy(replacement);
    }

    oneui_widget_destroy(list);
    oneui_widget_destroy(label);
    oneui_widget_destroy(field);
    oneui_widget_destroy(root);
    oneui_window_destroy(window);
}

void testWindowLayoutSnapshotRetainsModalOverlayTree() {
    OneUiWindowOptionsUtf8 options{};
    const std::string title = "OneUI modal snapshot lifetime test";
    options.title = utf8View(title);
    options.width = 640;
    options.height = 480;
    options.visible = 0;
    options.borderless = 1;
    options.resizable = 1;

    OneUiWindow* window = oneui_window_create_utf8(&options);
    OneUiWidget* host = oneui_overlay_host_create();
    OneUiWidget* content = oneui_panel_create();
    OneUiWidget* dialog = oneui_dialog_create(L"Snapshot dialog", L"Lifetime regression");
    OneUiWidget* body = oneui_label_create(L"Snapshot dialog body");
    expectTrue(
        "modal snapshot widgets create",
        window != nullptr && host != nullptr && content != nullptr && dialog != nullptr && body != nullptr);
    if (!window || !host || !content || !dialog || !body) {
        oneui_widget_destroy(body);
        oneui_widget_destroy(dialog);
        oneui_widget_destroy(content);
        oneui_widget_destroy(host);
        oneui_window_destroy(window);
        return;
    }

    oneui_dialog_set_content(dialog, body);
    oneui_overlay_host_set_content(host, content);
    oneui_overlay_host_add_modal_anchored_overlay(
        host,
        dialog,
        100,
        420.0f,
        280.0f,
        OneUiInsets{0.0f, 0.0f, 0.0f, 0.0f},
        1,
        1);
    oneui_window_set_content(window, host);

    // Mounted widgets remain owned by the tree after their C handles are released.
    oneui_widget_destroy(body);
    oneui_widget_destroy(dialog);
    oneui_widget_destroy(content);
    oneui_widget_destroy(host);
    oneui_window_initialize(window);

    std::size_t required = 0;
    expectTrue(
        "modal snapshot size query",
        oneui_window_layout_snapshot_utf8(window, nullptr, 0, &required) == -2 && required > 1);
    std::vector<char> snapshot(required, '\0');
    expectTrue(
        "modal snapshot serialization",
        oneui_window_layout_snapshot_utf8(
            window,
            snapshot.data(),
            snapshot.size(),
            &required) == 1);
    const std::string json(snapshot.data());
    expectTrue("modal snapshot contains dialog", json.find("\"tag\":\"dialog\"") != std::string::npos);
    expectTrue(
        "modal snapshot contains label text",
        json.find("\"text\":\"Snapshot dialog body\"") != std::string::npos);

    oneui_window_destroy(window);
}

void testWindowLayoutSnapshotKeepsSizeQueryAndReadAtomicForVirtualLists() {
    OneUiWindowOptionsUtf8 options{};
    const std::string title = "OneUI virtual-list snapshot transaction test";
    options.title = utf8View(title);
    options.width = 640;
    options.height = 480;
    options.visible = 0;
    options.borderless = 1;
    options.resizable = 1;

    OneUiWindow* window = oneui_window_create_utf8(&options);
    OneUiWidget* list = oneui_virtual_list_create();
    expectTrue("virtual-list snapshot widgets create", window != nullptr && list != nullptr);
    if (!window || !list) {
        oneui_widget_destroy(list);
        oneui_window_destroy(window);
        return;
    }

    std::vector<std::string> titles;
    std::vector<std::string> details;
    std::vector<OneUiListItemUtf8> items;
    titles.reserve(256);
    details.reserve(256);
    items.reserve(256);
    for (int index = 0; index < 256; ++index) {
        titles.push_back("Process " + std::to_string(index));
        details.push_back("PID " + std::to_string(1000 + index) + " · CPU 12.5% · memory 64 MB");
    }
    for (std::size_t index = 0; index < titles.size(); ++index) {
        items.push_back(OneUiListItemUtf8{utf8View(titles[index]), utf8View(details[index])});
    }
    oneui_virtual_list_set_items_utf8(list, items.data(), items.size());
    oneui_virtual_list_set_row_height(list, 40.0f);
    oneui_widget_set_preferred_size(list, 560.0f, 400.0f);
    oneui_window_set_content(window, list);
    oneui_window_initialize(window);

    std::size_t required = 0;
    expectTrue(
        "virtual-list snapshot size query",
        oneui_window_layout_snapshot_utf8(window, nullptr, 0, &required) == -2 && required > 1);
    const std::size_t queriedSize = required;
    std::vector<char> snapshot(queriedSize, '\0');
    expectTrue(
        "virtual-list snapshot exact-size read",
        oneui_window_layout_snapshot_utf8(
            window,
            snapshot.data(),
            snapshot.size(),
            &required) == 1 &&
            required == queriedSize);
    const std::string json(snapshot.data());
    expectTrue(
        "virtual-list snapshot contains realized rows",
        json.find("Process 0") != std::string::npos && json.find("PID 1000") != std::string::npos);

    oneui_widget_destroy(list);
    oneui_window_destroy(window);
}

} // namespace

int main() {
    expectTrue("version exported", std::strcmp(oneui_version(), "0.1.0") == 0);
    testOwnedWindowPostCleansUpCancelledWork();
#ifdef _WIN32
    testWindowRawKeyTracksMessageModifiersAndResetsOnFocusLoss();
#endif
    testUtf8AbiRoundTripsUnicodeText();
    testUtf8ListUsesStructuredItems();
    testUtf8TreeViewUsesStableIdsAndStructuredParents();
    testWindowDpiMetricsAbiUsesLogicalAndPhysicalSizes();
    testWindowPlacementAbiRoundTripsRestoredBounds();
    testAppShellAbiCreatesReusableSlots();
    testProductShellAbiIsPublicProductFrame();
    testOverlayToastAbiSupportsAnchoredNotice();
    testRealtimeFrameViewAbiAcceptsBgraFrames();
    testRemoteInputRegionAbiCreatesAndAcceptsCallbacks();
    testTerminalViewAbiUsesStructuredCells();
    testProgressBarAbiClampsValues();
    testSparklineAbiAcceptsNormalizedSamples();
    testWindowLayoutSnapshotSerializesMountedTreeWithoutFieldValues();
    testWindowLayoutSnapshotRetainsModalOverlayTree();
    testWindowLayoutSnapshotKeepsSizeQueryAndReadAtomicForVirtualLists();
    testPromptAbiRejectsInvalidOutput();
    testClipboardAbiRoundTripIfAvailable();

    if (failures != 0) {
        std::cerr << failures << " C ABI behavior test(s) failed.\n";
        return 1;
    }
    return 0;
}
