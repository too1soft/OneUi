#pragma once

#include "oneui/export.h"

#include <stddef.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OneUiWindow OneUiWindow;
typedef struct OneUiWidget OneUiWidget;
typedef struct OneUiStyleSheet OneUiStyleSheet;
typedef struct OneUiTray OneUiTray;

typedef void (*OneUiVoidCallback)(void* user_data);
typedef void (*OneUiDestroyCallback)(void* user_data);
typedef void (*OneUiFrameCallback)(double now_ms, void* user_data);
typedef void (*OneUiTextCallback)(const wchar_t* text, void* user_data);
typedef void (*OneUiUtf8TextCallback)(const char* text, size_t length, void* user_data);
typedef void (*OneUiTreeExpansionCallback)(const char* id, size_t length, int expanded, void* user_data);
typedef void (*OneUiBoolCallback)(int checked, void* user_data);
typedef void (*OneUiIntCallback)(int value, void* user_data);

/*
 * Cross-platform string ABI. The caller owns data and it is only read during
 * the function call. Invalid UTF-8 is converted to U+FFFD by OneUI.
 */
typedef struct OneUiUtf8String {
    const char* data;
    size_t length;
} OneUiUtf8String;

/*
 * Structured UTF-8 list data. The caller owns both string buffers and the
 * array; OneUI copies every field before oneui_list_set_items_utf8 returns.
 */
typedef struct OneUiListItemUtf8 {
    OneUiUtf8String title;
    OneUiUtf8String detail;
} OneUiListItemUtf8;

/*
 * Structured tree data. `id` is a stable opaque identifier; an empty
 * parent_id denotes a root. OneUI copies every field before the call returns.
 */
typedef struct OneUiTreeItemUtf8 {
    OneUiUtf8String id;
    OneUiUtf8String parent_id;
    OneUiUtf8String title;
    OneUiUtf8String detail;
    int expanded;
} OneUiTreeItemUtf8;

typedef struct OneUiWindowOptionsUtf8 {
    OneUiUtf8String title;
    int width;
    int height;
    int visible;
    int borderless;
    int fullscreen;
    int topmost;
    int resizable;
} OneUiWindowOptionsUtf8;

typedef struct OneUiWindowOptions {
    const wchar_t* title;
    int width;
    int height;
    int visible;
    int borderless;
    int fullscreen;
    int topmost;
    int resizable;
} OneUiWindowOptions;

typedef struct OneUiInsets {
    float top;
    float right;
    float bottom;
    float left;
} OneUiInsets;

typedef struct OneUiColor {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} OneUiColor;

/*
 * Terminal cells are structured so text, colors, and styles never depend on
 * delimiter encodings. OneUI copies every cell before this call returns.
 */
typedef struct OneUiTerminalCellUtf8 {
    OneUiUtf8String text;
    OneUiColor foreground;
    OneUiColor background;
    unsigned int style;
    unsigned int hyperlink_id;
} OneUiTerminalCellUtf8;

typedef enum OneUiTerminalCursorStyle {
    OneUiTerminalCursorStyleBlock = 0,
    OneUiTerminalCursorStyleBar = 1,
    OneUiTerminalCursorStyleUnderline = 2
} OneUiTerminalCursorStyle;

typedef struct OneUiFocusRingStyle {
    OneUiColor color;
    float width;
    float offset;
    float radius;
    int visible;
} OneUiFocusRingStyle;

typedef struct OneUiButtonStateStyle {
    OneUiColor background;
    OneUiColor foreground;
    OneUiColor border;
    float border_width;
    float radius;
    OneUiFocusRingStyle focus_ring;
} OneUiButtonStateStyle;

typedef struct OneUiButtonStyle {
    OneUiButtonStateStyle normal;
    OneUiButtonStateStyle hovered;
    OneUiButtonStateStyle pressed;
    OneUiButtonStateStyle disabled;
    OneUiButtonStateStyle focus_visible;
} OneUiButtonStyle;

typedef struct OneUiInteractiveSurfaceStateStyle {
    OneUiColor background;
    OneUiColor border;
    float border_width;
    float radius;
} OneUiInteractiveSurfaceStateStyle;

typedef struct OneUiInteractiveSurfaceStyle {
    OneUiInteractiveSurfaceStateStyle normal;
    OneUiInteractiveSurfaceStateStyle hovered;
    OneUiInteractiveSurfaceStateStyle pressed;
    OneUiInteractiveSurfaceStateStyle disabled;
} OneUiInteractiveSurfaceStyle;

typedef struct OneUiTextFieldStateStyle {
    OneUiColor background;
    OneUiColor foreground;
    OneUiColor placeholder_foreground;
    OneUiColor border;
    OneUiColor selection_background;
    OneUiColor caret_color;
    float border_width;
    float radius;
    OneUiInsets padding;
    OneUiFocusRingStyle focus_ring;
} OneUiTextFieldStateStyle;

typedef struct OneUiTextFieldStyle {
    OneUiTextFieldStateStyle normal;
    OneUiTextFieldStateStyle hovered;
    OneUiTextFieldStateStyle disabled;
    OneUiTextFieldStateStyle read_only;
    OneUiTextFieldStateStyle focus_visible;
} OneUiTextFieldStyle;

typedef enum OneUiStackDirection {
    OneUiStackDirectionColumn = 0,
    OneUiStackDirectionRow = 1
} OneUiStackDirection;

typedef enum OneUiStackAlign {
    OneUiStackAlignStart = 0,
    OneUiStackAlignCenter = 1,
    OneUiStackAlignEnd = 2,
    OneUiStackAlignStretch = 3
} OneUiStackAlign;

typedef enum OneUiButtonVariant {
    OneUiButtonVariantPrimary = 0,
    OneUiButtonVariantSecondary = 1
} OneUiButtonVariant;

typedef enum OneUiPixelFormat {
    OneUiPixelFormatBgra8888 = 0,
    OneUiPixelFormatRgba8888 = 1,
    OneUiPixelFormatNv12 = 2
} OneUiPixelFormat;

typedef enum OneUiVideoScaleMode {
    OneUiVideoScaleModeActualSize = 0,
    OneUiVideoScaleModeFit = 1,
    OneUiVideoScaleModeFill = 2,
    OneUiVideoScaleModeStretch = 3
} OneUiVideoScaleMode;

typedef enum OneUiPointerButton {
    OneUiPointerButtonNone = 0,
    OneUiPointerButtonLeft = 1,
    OneUiPointerButtonRight = 2,
    OneUiPointerButtonMiddle = 3,
    OneUiPointerButtonX1 = 4,
    OneUiPointerButtonX2 = 5
} OneUiPointerButton;

typedef struct OneUiRemotePointerEvent {
    float window_x;
    float window_y;
    float content_x;
    float content_y;
    float normalized_x;
    float normalized_y;
    float remote_x;
    float remote_y;
    OneUiPointerButton button;
    int pressed;
    int wheel_delta_x;
    int wheel_delta_y;
} OneUiRemotePointerEvent;

typedef struct OneUiRawKeyEvent {
    unsigned int virtual_key;
    unsigned int scan_code;
    int pressed;
    int repeat;
    int extended;
    int alt;
    int ctrl;
    int shift;
    int win;
} OneUiRawKeyEvent;

typedef enum OneUiTerminalPointerAction {
    OneUiTerminalPointerActionPress = 0,
    OneUiTerminalPointerActionRelease = 1,
    OneUiTerminalPointerActionMove = 2,
    OneUiTerminalPointerActionWheel = 3
} OneUiTerminalPointerAction;

typedef struct OneUiTerminalPointerEvent {
    OneUiTerminalPointerAction action;
    OneUiPointerButton button;
    unsigned short row;
    unsigned short column;
    int wheel_delta;
    int shift;
    int control;
    int alt;
} OneUiTerminalPointerEvent;

typedef void (*OneUiRemotePointerCallback)(const OneUiRemotePointerEvent* event, void* user_data);
typedef void (*OneUiRawKeyCallback)(const OneUiRawKeyEvent* event, void* user_data);
typedef void (*OneUiTerminalPointerCallback)(const OneUiTerminalPointerEvent* event, void* user_data);
typedef void (*OneUiTerminalHyperlinkCallback)(unsigned int hyperlink_id, void* user_data);

enum {
    OneUiTerminalCellBold = 1u << 0,
    OneUiTerminalCellDim = 1u << 1,
    OneUiTerminalCellItalic = 1u << 2,
    OneUiTerminalCellUnderline = 1u << 3,
    OneUiTerminalCellInverse = 1u << 4,
    OneUiTerminalCellWide = 1u << 5,
    OneUiTerminalCellWideContinuation = 1u << 6
};

#define ONEUI_UTF8_ABI_VERSION 2u

ONEUI_API const char* oneui_version(void);
ONEUI_API unsigned int oneui_utf8_abi_version(void);
ONEUI_API OneUiWindow* oneui_window_create(const OneUiWindowOptions* options);
ONEUI_API OneUiWindow* oneui_window_create_utf8(const OneUiWindowOptionsUtf8* options);
ONEUI_API void oneui_window_destroy(OneUiWindow* window);
ONEUI_API void oneui_window_initialize(OneUiWindow* window);
ONEUI_API void oneui_window_show(OneUiWindow* window);
ONEUI_API void oneui_window_activate(OneUiWindow* window);
ONEUI_API int oneui_window_run(OneUiWindow* window);
ONEUI_API void oneui_window_close(OneUiWindow* window);
ONEUI_API void oneui_window_request_close(OneUiWindow* window);
ONEUI_API void oneui_window_minimize(OneUiWindow* window);
ONEUI_API void oneui_window_toggle_maximize(OneUiWindow* window);
ONEUI_API void oneui_window_set_borderless(OneUiWindow* window, int borderless);
ONEUI_API void oneui_window_set_title_bar_drag_metrics(OneUiWindow* window, float title_bar_height, float reserved_button_width);
ONEUI_API void oneui_window_set_corner_radius(OneUiWindow* window, float radius);
ONEUI_API void oneui_window_set_close_to_tray(OneUiWindow* window, int close_to_tray);
ONEUI_API void oneui_window_post(OneUiWindow* window, OneUiVoidCallback callback, void* user_data);
/*
 * Queues callback on the window UI thread. Returns 1 when accepted. When the
 * callback cannot run because the window is closed, cleanup receives user_data
 * exactly once. The callback owns user_data after a successful return.
 */
ONEUI_API int oneui_window_post_owned(
    OneUiWindow* window,
    OneUiVoidCallback callback,
    void* user_data,
    OneUiDestroyCallback cleanup);
ONEUI_API void oneui_window_request_animation_frame(OneUiWindow* window, OneUiFrameCallback callback, void* user_data);
ONEUI_API void oneui_window_set_title(OneUiWindow* window, const wchar_t* title);
ONEUI_API void oneui_window_set_title_utf8(OneUiWindow* window, OneUiUtf8String title);
ONEUI_API void* oneui_window_native_handle(OneUiWindow* window);
ONEUI_API int oneui_window_confirm(OneUiWindow* window, const wchar_t* title, const wchar_t* message);
// 打开系统“选择文件夹”对话框；选中写入 out(最多 outLen 个 wchar)并返回 1，取消返回 0。
ONEUI_API int oneui_window_pick_folder(OneUiWindow* window, const wchar_t* title, wchar_t* out, int outLen);
ONEUI_API int oneui_window_client_width(OneUiWindow* window);
ONEUI_API int oneui_window_client_height(OneUiWindow* window);
ONEUI_API int oneui_window_client_pixel_width(OneUiWindow* window);
ONEUI_API int oneui_window_client_pixel_height(OneUiWindow* window);
ONEUI_API float oneui_window_dpi_scale(OneUiWindow* window);
ONEUI_API void oneui_window_set_content(OneUiWindow* window, OneUiWidget* widget);
ONEUI_API int oneui_window_request_focus(OneUiWindow* window, OneUiWidget* widget, int focus_visible);
ONEUI_API void oneui_window_set_style_sheet(OneUiWindow* window, OneUiStyleSheet* style_sheet);

ONEUI_API OneUiTray* oneui_tray_create(OneUiWindow* window, const wchar_t* tooltip);
ONEUI_API void oneui_tray_destroy(OneUiTray* tray);
ONEUI_API int oneui_tray_show(OneUiTray* tray);
ONEUI_API int oneui_tray_hide(OneUiTray* tray);
ONEUI_API void oneui_tray_set_tooltip(OneUiTray* tray, const wchar_t* tooltip);
ONEUI_API int oneui_tray_show_notification(OneUiTray* tray, const wchar_t* title, const wchar_t* message);

ONEUI_API void oneui_widget_destroy(OneUiWidget* widget);
ONEUI_API void oneui_widget_set_preferred_size(OneUiWidget* widget, float width, float height);
ONEUI_API void oneui_widget_set_disabled(OneUiWidget* widget, int disabled);
ONEUI_API void oneui_widget_set_tab_stop(OneUiWidget* widget, int tab_stop);
ONEUI_API void oneui_widget_set_visible(OneUiWidget* widget, int visible);
ONEUI_API void oneui_widget_set_classes(OneUiWidget* widget, const char* classes);
ONEUI_API void oneui_widget_set_style_node(OneUiWidget* widget, const char* tag, const char* classes);
ONEUI_API void oneui_widget_apply_style_sheet(OneUiWidget* widget, OneUiStyleSheet* style_sheet);

ONEUI_API OneUiStyleSheet* oneui_style_sheet_create(void);
ONEUI_API void oneui_style_sheet_destroy(OneUiStyleSheet* style_sheet);
ONEUI_API void oneui_style_sheet_set_custom_property(OneUiStyleSheet* style_sheet, const char* name, const char* value);
ONEUI_API int oneui_style_sheet_add_css(OneUiStyleSheet* style_sheet, const char* css, char* error_buffer, int error_buffer_len);
ONEUI_API int oneui_style_sheet_load_file(OneUiStyleSheet* style_sheet, const wchar_t* path, char* error_buffer, int error_buffer_len);

ONEUI_API int oneui_clipboard_set_text(const wchar_t* text);
ONEUI_API int oneui_clipboard_get_text(wchar_t* buffer, int buffer_len);
/* Returns required buffer bytes including the trailing NUL; returns 0 on error. */
ONEUI_API int oneui_clipboard_set_text_utf8(OneUiUtf8String text);
ONEUI_API size_t oneui_clipboard_get_text_utf8(char* buffer, size_t buffer_len);

ONEUI_API OneUiWidget* oneui_stack_create(OneUiStackDirection direction);
ONEUI_API void oneui_stack_add(OneUiWidget* stack, OneUiWidget* child);
ONEUI_API void oneui_stack_set_gap(OneUiWidget* stack, float gap);
ONEUI_API void oneui_stack_set_padding(OneUiWidget* stack, OneUiInsets insets);
ONEUI_API void oneui_stack_set_align(OneUiWidget* stack, OneUiStackAlign align);

ONEUI_API OneUiWidget* oneui_top_bar_create(void);
ONEUI_API void oneui_top_bar_set_leading(OneUiWidget* top_bar, OneUiWidget* child);
ONEUI_API void oneui_top_bar_add_action(OneUiWidget* top_bar, OneUiWidget* child);
ONEUI_API void oneui_top_bar_clear_actions(OneUiWidget* top_bar);
ONEUI_API void oneui_top_bar_set_gap(OneUiWidget* top_bar, float gap);
ONEUI_API void oneui_top_bar_set_padding(OneUiWidget* top_bar, OneUiInsets insets);
ONEUI_API void oneui_top_bar_set_leading_width(OneUiWidget* top_bar, float width);

ONEUI_API OneUiWidget* oneui_app_shell_create(void);
ONEUI_API void oneui_app_shell_set_sidebar(OneUiWidget* shell, OneUiWidget* child);
ONEUI_API void oneui_app_shell_set_header(OneUiWidget* shell, OneUiWidget* child);
ONEUI_API void oneui_app_shell_set_content(OneUiWidget* shell, OneUiWidget* child);
ONEUI_API void oneui_app_shell_set_footer(OneUiWidget* shell, OneUiWidget* child);
ONEUI_API void oneui_app_shell_set_sidebar_width(OneUiWidget* shell, float width);
ONEUI_API void oneui_app_shell_set_header_height(OneUiWidget* shell, float height);
ONEUI_API void oneui_app_shell_set_footer_height(OneUiWidget* shell, float height);
ONEUI_API void oneui_app_shell_set_gap(OneUiWidget* shell, float gap);
ONEUI_API void oneui_app_shell_set_padding(OneUiWidget* shell, OneUiInsets insets);
ONEUI_API void oneui_app_shell_set_sidebar_visible(OneUiWidget* shell, int visible);

ONEUI_API OneUiWidget* oneui_product_shell_create(void);
ONEUI_API void oneui_product_shell_set_sidebar(OneUiWidget* shell, OneUiWidget* child);
ONEUI_API void oneui_product_shell_set_topbar(OneUiWidget* shell, OneUiWidget* child);
ONEUI_API void oneui_product_shell_set_content(OneUiWidget* shell, OneUiWidget* child);
ONEUI_API void oneui_product_shell_set_status(OneUiWidget* shell, OneUiWidget* child);
ONEUI_API void oneui_product_shell_set_sidebar_width(OneUiWidget* shell, float width);
ONEUI_API void oneui_product_shell_set_topbar_height(OneUiWidget* shell, float height);
ONEUI_API void oneui_product_shell_set_status_height(OneUiWidget* shell, float height);
ONEUI_API void oneui_product_shell_set_gap(OneUiWidget* shell, float gap);
ONEUI_API void oneui_product_shell_set_padding(OneUiWidget* shell, OneUiInsets insets);
ONEUI_API void oneui_product_shell_set_sidebar_visible(OneUiWidget* shell, int visible);

ONEUI_API OneUiWidget* oneui_overlay_host_create(void);
ONEUI_API void oneui_overlay_host_set_content(OneUiWidget* host, OneUiWidget* child);
ONEUI_API void oneui_overlay_host_add_overlay(OneUiWidget* host, OneUiWidget* child, int layer);
ONEUI_API void oneui_overlay_host_add_anchored_overlay(
    OneUiWidget* host,
    OneUiWidget* child,
    int layer,
    float width,
    float height,
    OneUiInsets margin,
    int horizontal_alignment,
    int vertical_alignment);
/* Modal overlays trap focus and block pointer events outside their bounds. */
ONEUI_API void oneui_overlay_host_add_modal_anchored_overlay(
    OneUiWidget* host,
    OneUiWidget* child,
    int layer,
    float width,
    float height,
    OneUiInsets margin,
    int horizontal_alignment,
    int vertical_alignment);
ONEUI_API int oneui_overlay_host_update_anchored_overlay(
    OneUiWidget* host,
    OneUiWidget* child,
    float width,
    float height,
    OneUiInsets margin,
    int horizontal_alignment,
    int vertical_alignment);
ONEUI_API int oneui_overlay_host_remove_overlay(OneUiWidget* host, OneUiWidget* child);

ONEUI_API OneUiWidget* oneui_log_view_create(void);
ONEUI_API void oneui_log_view_append_line(OneUiWidget* view, const wchar_t* text, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
ONEUI_API void oneui_log_view_clear(OneUiWidget* view);
ONEUI_API float oneui_log_view_content_height(OneUiWidget* view);
ONEUI_API void oneui_log_view_set_font_size(OneUiWidget* view, float size);
ONEUI_API void oneui_log_view_set_line_height(OneUiWidget* view, float height);

ONEUI_API OneUiWidget* oneui_scroll_view_create(void);
ONEUI_API void oneui_scroll_view_set_content(OneUiWidget* view, OneUiWidget* child);
ONEUI_API void oneui_scroll_view_set_content_height(OneUiWidget* view, float height);
ONEUI_API void oneui_scroll_view_set_wheel_step(OneUiWidget* view, float step);
ONEUI_API void oneui_scroll_view_set_chrome_visible(OneUiWidget* view, int visible);
ONEUI_API void oneui_scroll_view_set_scrollbar_style(OneUiWidget* view, unsigned char r, unsigned char g, unsigned char b, unsigned char a, float thickness);
ONEUI_API void oneui_scroll_view_scroll_to_bottom(OneUiWidget* view);

ONEUI_API OneUiWidget* oneui_panel_create(void);
ONEUI_API void oneui_panel_set_content(OneUiWidget* panel, OneUiWidget* child);
ONEUI_API void oneui_panel_set_background(OneUiWidget* panel, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
ONEUI_API void oneui_panel_set_border(OneUiWidget* panel, unsigned char r, unsigned char g, unsigned char b, unsigned char a, float width);
ONEUI_API void oneui_panel_set_radius(OneUiWidget* panel, float radius);
ONEUI_API void oneui_panel_set_padding(OneUiWidget* panel, OneUiInsets insets);
ONEUI_API void oneui_panel_set_shadow(
    OneUiWidget* panel,
    unsigned char r,
    unsigned char g,
    unsigned char b,
    unsigned char a,
    float offset_x,
    float offset_y,
    float blur_radius,
    float spread_radius);

ONEUI_API OneUiWidget* oneui_label_create(const wchar_t* text);
ONEUI_API void oneui_label_set_text(OneUiWidget* label, const wchar_t* text);
ONEUI_API OneUiWidget* oneui_label_create_utf8(OneUiUtf8String text);
ONEUI_API void oneui_label_set_text_utf8(OneUiWidget* label, OneUiUtf8String text);
ONEUI_API void oneui_label_set_color(OneUiWidget* label, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
ONEUI_API void oneui_label_set_font_size(OneUiWidget* label, float font_size);
ONEUI_API void oneui_label_set_font_weight(OneUiWidget* label, int font_weight);
/* align: 0 = left, 1 = center, 2 = right */
ONEUI_API void oneui_label_set_align(OneUiWidget* label, int align);

ONEUI_API OneUiWidget* oneui_icon_create(int symbol);
ONEUI_API void oneui_icon_set_symbol(OneUiWidget* icon, int symbol);
ONEUI_API void oneui_icon_set_color(OneUiWidget* icon, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
ONEUI_API void oneui_icon_set_accent(OneUiWidget* icon, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
ONEUI_API void oneui_icon_set_stroke_width(OneUiWidget* icon, float width);

ONEUI_API OneUiWidget* oneui_icon_button_create(int symbol);
ONEUI_API void oneui_icon_button_set_symbol(OneUiWidget* icon_button, int symbol);
ONEUI_API void oneui_icon_button_set_on_click(OneUiWidget* icon_button, OneUiVoidCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_switch_create(const wchar_t* text);
ONEUI_API void oneui_switch_set_text(OneUiWidget* switch_widget, const wchar_t* text);
ONEUI_API void oneui_switch_set_checked(OneUiWidget* switch_widget, int checked);
ONEUI_API int oneui_switch_checked(OneUiWidget* switch_widget);
ONEUI_API void oneui_switch_set_on_changed(OneUiWidget* switch_widget, OneUiBoolCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_title_bar_create(const wchar_t* title);
ONEUI_API void oneui_title_bar_set_title(OneUiWidget* title_bar, const wchar_t* title);
ONEUI_API void oneui_title_bar_set_icon_symbol(OneUiWidget* title_bar, int symbol);
ONEUI_API void oneui_title_bar_set_maximized(OneUiWidget* title_bar, int maximized);
ONEUI_API void oneui_title_bar_set_variant(OneUiWidget* title_bar, const char* variant);
ONEUI_API void oneui_title_bar_set_on_minimize(OneUiWidget* title_bar, OneUiVoidCallback callback, void* user_data);
ONEUI_API void oneui_title_bar_set_on_maximize(OneUiWidget* title_bar, OneUiVoidCallback callback, void* user_data);
ONEUI_API void oneui_title_bar_set_on_close(OneUiWidget* title_bar, OneUiVoidCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_nav_item_create(const wchar_t* text, int symbol, int selected);
ONEUI_API void oneui_nav_item_set_text(OneUiWidget* nav_item, const wchar_t* text);
ONEUI_API void oneui_nav_item_set_symbol(OneUiWidget* nav_item, int symbol);
ONEUI_API void oneui_nav_item_set_selected(OneUiWidget* nav_item, int selected);
ONEUI_API void oneui_nav_item_set_on_click(OneUiWidget* nav_item, OneUiVoidCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_badge_create(const wchar_t* text, int variant);
ONEUI_API void oneui_badge_set_text(OneUiWidget* badge, const wchar_t* text);
ONEUI_API void oneui_badge_set_variant(OneUiWidget* badge, int variant);

ONEUI_API OneUiWidget* oneui_icon_badge_create(int symbol);
ONEUI_API void oneui_icon_badge_set_symbol(OneUiWidget* icon_badge, int symbol);
ONEUI_API void oneui_icon_badge_set_accent(OneUiWidget* icon_badge, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
ONEUI_API void oneui_icon_badge_set_stroke_width(OneUiWidget* icon_badge, float width);

ONEUI_API OneUiWidget* oneui_menu_create(void);
ONEUI_API void oneui_menu_add_header(OneUiWidget* menu, const wchar_t* title, const wchar_t* subtitle);
/* icon_symbol: IconSymbol ordinal, negative = no icon; returns item index */
ONEUI_API int oneui_menu_add_item(OneUiWidget* menu, const wchar_t* text, int icon_symbol, int danger);
ONEUI_API void oneui_menu_add_separator(OneUiWidget* menu);
ONEUI_API void oneui_menu_set_item_disabled(OneUiWidget* menu, int index, int disabled);
ONEUI_API float oneui_menu_preferred_height(OneUiWidget* menu);
ONEUI_API void oneui_menu_set_on_activated(OneUiWidget* menu, OneUiIntCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_dialog_create(const wchar_t* title, const wchar_t* subtitle);
ONEUI_API void oneui_dialog_set_title(OneUiWidget* dialog, const wchar_t* title);
ONEUI_API void oneui_dialog_set_subtitle(OneUiWidget* dialog, const wchar_t* subtitle);
/* symbol: IconSymbol ordinal; negative clears the header icon */
ONEUI_API void oneui_dialog_set_icon(OneUiWidget* dialog, int symbol);
ONEUI_API void oneui_dialog_set_close_visible(OneUiWidget* dialog, int visible);
ONEUI_API void oneui_dialog_set_on_close(OneUiWidget* dialog, OneUiVoidCallback callback, void* user_data);
ONEUI_API void oneui_dialog_set_content(OneUiWidget* dialog, OneUiWidget* child);
ONEUI_API void oneui_dialog_set_actions(OneUiWidget* dialog, OneUiWidget* child);

ONEUI_API OneUiWidget* oneui_segmented_control_create(void);
ONEUI_API void oneui_segmented_control_set_items(OneUiWidget* segmented_control, const wchar_t* items);
ONEUI_API void oneui_segmented_control_set_selected_index(OneUiWidget* segmented_control, int index);
ONEUI_API int oneui_segmented_control_selected_index(OneUiWidget* segmented_control);
ONEUI_API void oneui_segmented_control_set_on_changed(OneUiWidget* segmented_control, OneUiIntCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_list_create(void);
ONEUI_API void oneui_list_set_items(OneUiWidget* list, const wchar_t* items);
ONEUI_API void oneui_list_set_items_utf8(OneUiWidget* list, const OneUiListItemUtf8* items, size_t count);
ONEUI_API void oneui_list_set_selected_index(OneUiWidget* list, int index);
ONEUI_API int oneui_list_selected_index(OneUiWidget* list);
ONEUI_API void oneui_list_set_on_changed(OneUiWidget* list, OneUiIntCallback callback, void* user_data);

/*
 * Fixed-height viewport-virtualized list. Use this for large result sets;
 * OneUI only paints rows intersecting the current viewport.
 */
ONEUI_API OneUiWidget* oneui_virtual_list_create(void);
ONEUI_API void oneui_virtual_list_set_items_utf8(OneUiWidget* list, const OneUiListItemUtf8* items, size_t count);
ONEUI_API void oneui_virtual_list_set_selected_index(OneUiWidget* list, int index);
ONEUI_API int oneui_virtual_list_selected_index(OneUiWidget* list);
ONEUI_API void oneui_virtual_list_set_row_height(OneUiWidget* list, float height);
ONEUI_API void oneui_virtual_list_set_scroll_offset(OneUiWidget* list, float offset);
ONEUI_API float oneui_virtual_list_scroll_offset(OneUiWidget* list);
ONEUI_API float oneui_virtual_list_max_scroll_offset(OneUiWidget* list);
ONEUI_API void oneui_virtual_list_set_on_changed(OneUiWidget* list, OneUiIntCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_tree_view_create(void);
ONEUI_API void oneui_tree_view_set_items_utf8(OneUiWidget* tree_view, const OneUiTreeItemUtf8* items, size_t count);
ONEUI_API void oneui_tree_view_set_selected_id_utf8(OneUiWidget* tree_view, OneUiUtf8String id);
ONEUI_API float oneui_tree_view_content_height(OneUiWidget* tree_view);
/* Returns required bytes including the trailing NUL; returns 0 for an invalid view. */
ONEUI_API size_t oneui_tree_view_selected_id_utf8(OneUiWidget* tree_view, char* buffer, size_t buffer_len);
ONEUI_API void oneui_tree_view_set_on_selection_changed_utf8(
    OneUiWidget* tree_view,
    OneUiUtf8TextCallback callback,
    void* user_data);
ONEUI_API void oneui_tree_view_set_on_expansion_changed_utf8(
    OneUiWidget* tree_view,
    OneUiTreeExpansionCallback callback,
    void* user_data);

ONEUI_API OneUiWidget* oneui_table_create(void);
ONEUI_API void oneui_table_set_columns(OneUiWidget* table, const wchar_t* columns);
ONEUI_API void oneui_table_set_rows(OneUiWidget* table, const wchar_t* rows);

ONEUI_API OneUiWidget* oneui_card_create();
ONEUI_API void oneui_card_set_content(OneUiWidget* card, OneUiWidget* child);

ONEUI_API OneUiWidget* oneui_interactive_surface_create();
ONEUI_API void oneui_interactive_surface_set_content(OneUiWidget* surface, OneUiWidget* child);
ONEUI_API void oneui_interactive_surface_set_padding(OneUiWidget* surface, OneUiInsets padding);
ONEUI_API void oneui_interactive_surface_set_style(OneUiWidget* surface, const OneUiInteractiveSurfaceStyle* style);
ONEUI_API void oneui_interactive_surface_set_on_click(OneUiWidget* surface, OneUiVoidCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_realtime_frame_view_create(void);
ONEUI_API void oneui_realtime_frame_view_set_scale_mode(OneUiWidget* frame_view, OneUiVideoScaleMode scale_mode);
ONEUI_API void oneui_realtime_frame_view_set_background(OneUiWidget* frame_view, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
ONEUI_API void oneui_realtime_frame_view_submit_frame(
    OneUiWidget* frame_view,
    const void* pixels,
    int width,
    int height,
    int stride,
    OneUiPixelFormat pixel_format,
    unsigned long long frame_id,
    unsigned long long timestamp_us);

ONEUI_API OneUiWidget* oneui_remote_input_region_create(void);
ONEUI_API void oneui_remote_input_region_set_remote_size(OneUiWidget* region, float width, float height);
ONEUI_API void oneui_remote_input_region_set_scale_mode(OneUiWidget* region, OneUiVideoScaleMode scale_mode);
ONEUI_API void oneui_remote_input_region_set_on_pointer(
    OneUiWidget* region,
    OneUiRemotePointerCallback callback,
    void* user_data);
ONEUI_API void oneui_remote_input_region_set_on_raw_key(
    OneUiWidget* region,
    OneUiRawKeyCallback callback,
    void* user_data);
ONEUI_API void oneui_remote_input_region_release_all_inputs(OneUiWidget* region);

typedef void (*OneUiTerminalViewportCallback)(unsigned short rows, unsigned short columns, void* user_data);

ONEUI_API OneUiWidget* oneui_terminal_view_create(void);
ONEUI_API void oneui_terminal_view_set_font_size(OneUiWidget* view, float size);
ONEUI_API void oneui_terminal_view_set_line_height(OneUiWidget* view, float multiplier);
ONEUI_API void oneui_terminal_view_set_cursor_style(
    OneUiWidget* view,
    OneUiTerminalCursorStyle style);
ONEUI_API void oneui_terminal_view_set_cursor_blinking(OneUiWidget* view, int enabled);
ONEUI_API void oneui_terminal_view_set_copy_on_select(OneUiWidget* view, int enabled);
ONEUI_API void oneui_terminal_view_set_palette(
    OneUiWidget* view,
    OneUiColor background,
    OneUiColor foreground,
    OneUiColor cursor);
ONEUI_API void oneui_terminal_view_set_grid_utf8(
    OneUiWidget* view,
    unsigned short rows,
    unsigned short columns,
    const OneUiTerminalCellUtf8* cells,
    size_t cell_count);
ONEUI_API void oneui_terminal_view_update_cells_utf8(
    OneUiWidget* view,
    size_t first_cell,
    const OneUiTerminalCellUtf8* cells,
    size_t cell_count);
ONEUI_API void oneui_terminal_view_set_cursor(
    OneUiWidget* view,
    unsigned short row,
    unsigned short column,
    int visible);
ONEUI_API void oneui_terminal_view_select_all(OneUiWidget* view);
ONEUI_API void oneui_terminal_view_set_selection(
    OneUiWidget* view,
    unsigned short start_row,
    unsigned short start_column,
    unsigned short end_row,
    unsigned short end_column);
ONEUI_API void oneui_terminal_view_clear_selection(OneUiWidget* view);
ONEUI_API int oneui_terminal_view_has_selection(OneUiWidget* view);
ONEUI_API size_t oneui_terminal_view_get_selected_text_utf8(
    OneUiWidget* view,
    char* buffer,
    size_t buffer_len);
ONEUI_API void oneui_terminal_view_set_on_text_input_utf8(
    OneUiWidget* view,
    OneUiUtf8TextCallback callback,
    void* user_data);
ONEUI_API void oneui_terminal_view_set_on_paste_utf8(
    OneUiWidget* view,
    OneUiUtf8TextCallback callback,
    void* user_data);
ONEUI_API void oneui_terminal_view_set_on_raw_key(
    OneUiWidget* view,
    OneUiRawKeyCallback callback,
    void* user_data);
ONEUI_API void oneui_terminal_view_set_scroll_rows_per_wheel(OneUiWidget* view, float rows);
ONEUI_API void oneui_terminal_view_set_on_scroll(
    OneUiWidget* view,
    OneUiIntCallback callback,
    void* user_data);
ONEUI_API void oneui_terminal_view_set_mouse_reporting(OneUiWidget* view, int enabled);
ONEUI_API void oneui_terminal_view_set_on_pointer(
    OneUiWidget* view,
    OneUiTerminalPointerCallback callback,
    void* user_data);
ONEUI_API void oneui_terminal_view_set_on_hyperlink(
    OneUiWidget* view,
    OneUiTerminalHyperlinkCallback callback,
    void* user_data);
ONEUI_API void oneui_terminal_view_set_on_viewport_changed(
    OneUiWidget* view,
    OneUiTerminalViewportCallback callback,
    void* user_data);
ONEUI_API void oneui_terminal_view_set_on_focus_changed(
    OneUiWidget* view,
    OneUiBoolCallback callback,
    void* user_data);

ONEUI_API OneUiWidget* oneui_tile_create(const wchar_t* title, const wchar_t* subtitle);
ONEUI_API void oneui_tile_set_title(OneUiWidget* tile, const wchar_t* title);
ONEUI_API void oneui_tile_set_subtitle(OneUiWidget* tile, const wchar_t* subtitle);
ONEUI_API void oneui_tile_set_leading_symbol(OneUiWidget* tile, int symbol);
ONEUI_API void oneui_tile_clear_leading_symbol(OneUiWidget* tile);
ONEUI_API void oneui_tile_set_trailing_symbol(OneUiWidget* tile, int symbol);
ONEUI_API void oneui_tile_clear_trailing_symbol(OneUiWidget* tile);
ONEUI_API void oneui_tile_set_on_click(OneUiWidget* tile, OneUiVoidCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_status_strip_create(const wchar_t* title, const wchar_t* message);
ONEUI_API void oneui_status_strip_set_title(OneUiWidget* status_strip, const wchar_t* title);
ONEUI_API void oneui_status_strip_set_message(OneUiWidget* status_strip, const wchar_t* message);
ONEUI_API void oneui_status_strip_set_primary_action(OneUiWidget* status_strip, const wchar_t* text);
ONEUI_API void oneui_status_strip_set_secondary_action(OneUiWidget* status_strip, const wchar_t* text);
ONEUI_API void oneui_status_strip_set_on_primary_action(OneUiWidget* status_strip, OneUiVoidCallback callback, void* user_data);
ONEUI_API void oneui_status_strip_set_on_secondary_action(OneUiWidget* status_strip, OneUiVoidCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_state_view_create(const wchar_t* title, const wchar_t* message);
ONEUI_API void oneui_state_view_set_title(OneUiWidget* state_view, const wchar_t* title);
ONEUI_API void oneui_state_view_set_message(OneUiWidget* state_view, const wchar_t* message);
ONEUI_API void oneui_state_view_set_icon(OneUiWidget* state_view, int symbol);
ONEUI_API void oneui_state_view_set_action(OneUiWidget* state_view, const wchar_t* text);
ONEUI_API void oneui_state_view_set_on_action(OneUiWidget* state_view, OneUiVoidCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_toast_create(const wchar_t* title, const wchar_t* message);
ONEUI_API void oneui_toast_set_title(OneUiWidget* toast, const wchar_t* title);
ONEUI_API void oneui_toast_set_message(OneUiWidget* toast, const wchar_t* message);
ONEUI_API void oneui_toast_set_primary_action(OneUiWidget* toast, const wchar_t* text);
ONEUI_API void oneui_toast_set_secondary_action(OneUiWidget* toast, const wchar_t* text);
ONEUI_API void oneui_toast_set_icon_symbol(OneUiWidget* toast, int symbol);
ONEUI_API void oneui_toast_clear_icon_symbol(OneUiWidget* toast);
ONEUI_API void oneui_toast_set_close_visible(OneUiWidget* toast, int visible);
ONEUI_API void oneui_toast_set_on_primary_action(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data);
ONEUI_API void oneui_toast_set_on_secondary_action(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data);
ONEUI_API void oneui_toast_set_on_close(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_radio_group_create();
ONEUI_API void oneui_radio_group_set_items(OneUiWidget* radio_group, const wchar_t* items);
ONEUI_API void oneui_radio_group_set_selected_index(OneUiWidget* radio_group, int index);
ONEUI_API int oneui_radio_group_selected_index(OneUiWidget* radio_group);
ONEUI_API void oneui_radio_group_set_orientation(OneUiWidget* radio_group, int orientation);
ONEUI_API void oneui_radio_group_set_on_changed(OneUiWidget* radio_group, OneUiIntCallback callback, void* user_data);

ONEUI_API OneUiWidget* oneui_text_field_create(const wchar_t* placeholder);
ONEUI_API void oneui_text_field_set_text(OneUiWidget* text_field, const wchar_t* text);
ONEUI_API OneUiWidget* oneui_text_field_create_utf8(OneUiUtf8String placeholder);
ONEUI_API void oneui_text_field_set_text_utf8(OneUiWidget* text_field, OneUiUtf8String text);
ONEUI_API void oneui_text_field_set_read_only(OneUiWidget* text_field, int read_only);
ONEUI_API void oneui_text_field_set_multiline(OneUiWidget* text_field, int multiline);
ONEUI_API void oneui_text_field_set_line_height(OneUiWidget* text_field, float line_height);
ONEUI_API OneUiWidget* oneui_text_area_create_utf8(OneUiUtf8String placeholder);
ONEUI_API void oneui_text_field_set_prefix_icon(OneUiWidget* text_field, int symbol);
ONEUI_API void oneui_text_field_clear_prefix_icon(OneUiWidget* text_field);
ONEUI_API void oneui_text_field_set_suffix_icon(OneUiWidget* text_field, int symbol);
ONEUI_API void oneui_text_field_clear_suffix_icon(OneUiWidget* text_field);
ONEUI_API void oneui_text_field_set_on_changed(OneUiWidget* text_field, OneUiTextCallback callback, void* user_data);
ONEUI_API void oneui_text_field_set_on_changed_utf8(OneUiWidget* text_field, OneUiUtf8TextCallback callback, void* user_data);
ONEUI_API void oneui_text_field_set_style(OneUiWidget* text_field, const OneUiTextFieldStyle* style);
ONEUI_API void oneui_text_field_clear_style(OneUiWidget* text_field);

ONEUI_API OneUiWidget* oneui_search_box_create(const wchar_t* placeholder);
ONEUI_API OneUiWidget* oneui_search_box_create_utf8(OneUiUtf8String placeholder);

ONEUI_API OneUiWidget* oneui_button_create(const wchar_t* text);
ONEUI_API void oneui_button_set_text(OneUiWidget* button, const wchar_t* text);
ONEUI_API OneUiWidget* oneui_button_create_utf8(OneUiUtf8String text);
ONEUI_API void oneui_button_set_text_utf8(OneUiWidget* button, OneUiUtf8String text);
/* symbol: IconSymbol ordinal; negative clears the leading icon */
ONEUI_API void oneui_button_set_icon(OneUiWidget* button, int symbol);
/* align: 0 = left, 1 = center, 2 = right */
ONEUI_API void oneui_button_set_content_align(OneUiWidget* button, int align);
ONEUI_API void oneui_button_set_trailing_text_utf8(OneUiWidget* button, OneUiUtf8String text);
ONEUI_API void oneui_button_set_variant(OneUiWidget* button, OneUiButtonVariant variant);
ONEUI_API void oneui_button_set_on_click(OneUiWidget* button, OneUiVoidCallback callback, void* user_data);
ONEUI_API void oneui_button_set_style(OneUiWidget* button, const OneUiButtonStyle* style);
ONEUI_API void oneui_button_clear_style(OneUiWidget* button);

#ifdef __cplusplus
}
#endif
