#include "oneui/oneui_c_api.h"

#include <cmath>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <string>

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

void onUtf8TextChanged(const char* text, size_t length, void* userData) {
    auto* state = static_cast<Utf8CallbackState*>(userData);
    if (!state) {
        return;
    }
    state->text.assign(text ? text : "", length);
    ++state->calls;
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
    OneUiWidget* content = oneui_panel_create();
    OneUiWidget* footer = oneui_label_create(L"status");
    OneUiWidget* notify = oneui_icon_button_create(8);
    OneUiWidget* strip = oneui_status_strip_create(L"Status", L"Ready");
    OneUiWidget* tile = oneui_tile_create(L"Remote Desktop", L"169 510 1007");
    OneUiWidget* segmented = oneui_segmented_control_create();
    OneUiWidget* titleBar = oneui_title_bar_create(L"Product");

    expectTrue("app shell create", shell != nullptr);
    expectTrue("sidebar create", sidebar != nullptr);
    expectTrue("header create", header != nullptr);
    expectTrue("content create", content != nullptr);
    expectTrue("footer create", footer != nullptr);
    expectTrue("icon button create", notify != nullptr);
    expectTrue("status strip create", strip != nullptr);
    expectTrue("tile create", tile != nullptr);
    expectTrue("segmented control create", segmented != nullptr);
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
    oneui_icon_button_set_symbol(notify, 13);
    oneui_icon_button_set_on_click(notify, nullptr, nullptr);
    oneui_status_strip_set_title(strip, L"State");
    oneui_status_strip_set_message(strip, L"Updated");
    oneui_status_strip_set_primary_action(strip, L"Copy");
    oneui_status_strip_set_secondary_action(strip, L"Details");
    oneui_status_strip_set_on_primary_action(strip, nullptr, nullptr);
    oneui_status_strip_set_on_secondary_action(strip, nullptr, nullptr);
    oneui_tile_set_title(tile, L"Recent Assist");
    oneui_tile_set_subtitle(tile, L"164 709 2397");
    oneui_tile_set_leading_symbol(tile, 14);
    oneui_tile_set_trailing_symbol(tile, 13);
    oneui_tile_set_on_click(tile, nullptr, nullptr);
    oneui_segmented_control_set_items(segmented, L"Classic|Modern");
    oneui_segmented_control_set_selected_index(segmented, 1);
    expectTrue("segmented selected index", oneui_segmented_control_selected_index(segmented) == 1);
    oneui_segmented_control_set_on_changed(segmented, nullptr, nullptr);
    oneui_title_bar_set_title(titleBar, L"Updated Product");
    oneui_title_bar_set_icon_symbol(titleBar, 2);
    oneui_title_bar_set_maximized(titleBar, 0);
    oneui_title_bar_set_on_minimize(titleBar, nullptr, nullptr);
    oneui_title_bar_set_on_maximize(titleBar, nullptr, nullptr);
    oneui_title_bar_set_on_close(titleBar, nullptr, nullptr);

    oneui_widget_destroy(titleBar);
    oneui_widget_destroy(segmented);
    oneui_widget_destroy(tile);
    oneui_widget_destroy(strip);
    oneui_widget_destroy(notify);
    oneui_widget_destroy(footer);
    oneui_widget_destroy(content);
    oneui_widget_destroy(header);
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

void testClipboardAbiRoundTripIfAvailable() {
    const int setOk = oneui_clipboard_set_text(L"OneUI C ABI clipboard smoke");
    if (!setOk) {
        std::cerr << "Clipboard C ABI round trip skipped: system clipboard unavailable.\n";
        return;
    }

    wchar_t buffer[128]{};
    const int required = oneui_clipboard_get_text(
        buffer,
        static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
    expectTrue("clipboard get required length", required > 0);
    expectTrue(
        "clipboard round trip text",
        std::wcscmp(buffer, L"OneUI C ABI clipboard smoke") == 0);
}

} // namespace

int main() {
    expectTrue("version exported", std::strcmp(oneui_version(), "0.1.0") == 0);
    testUtf8AbiRoundTripsUnicodeText();
    testWindowDpiMetricsAbiUsesLogicalAndPhysicalSizes();
    testAppShellAbiCreatesReusableSlots();
    testProductShellAbiIsPublicProductFrame();
    testOverlayToastAbiSupportsAnchoredNotice();
    testRealtimeFrameViewAbiAcceptsBgraFrames();
    testRemoteInputRegionAbiCreatesAndAcceptsCallbacks();
    testClipboardAbiRoundTripIfAvailable();

    if (failures != 0) {
        std::cerr << failures << " C ABI behavior test(s) failed.\n";
        return 1;
    }
    return 0;
}
