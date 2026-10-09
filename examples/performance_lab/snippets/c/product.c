#include <oneui/oneui_c_api.h>
#define UTF8(s) ((OneUiUtf8String){s, sizeof(s) - 1})

/* The caller owns the returned handle and destroys it with oneui_widget_destroy. */
OneUiWidget *example_c_product(void) {
    OneUiWidget *shell = oneui_app_shell_create();
    OneUiWidget *bar = oneui_top_bar_create();
    OneUiWidget *title = oneui_label_create_utf8(UTF8("本地工作区"));
    OneUiWidget *sidebar = oneui_label_create_utf8(UTF8("设备列表"));
    OneUiWidget *editor = oneui_text_field_create_utf8(UTF8("输入草稿"));
    OneUiWidget *footer = oneui_label_create_utf8(UTF8("本地模拟 · 无网络连接"));
    if (!shell || !bar || !title || !sidebar || !editor || !footer) {
        oneui_widget_destroy(shell);
        shell = NULL;
    } else {
        oneui_top_bar_set_leading(bar, title);
        oneui_top_bar_set_leading_width(bar, 240);
        oneui_app_shell_set_header(shell, bar);
        oneui_app_shell_set_header_height(shell, 48);
        oneui_app_shell_set_sidebar(shell, sidebar);
        oneui_app_shell_set_sidebar_width(shell, 180);
        oneui_app_shell_set_content(shell, editor);
        oneui_app_shell_set_footer(shell, footer);
        oneui_app_shell_set_footer_height(shell, 28);
        oneui_app_shell_set_footer_span_sidebar(shell, 1);
        /* Your host can call oneui_app_shell_set_sidebar_visible on resize. */
    }
    /* Slots retain native children. These handles have no caller-owned callbacks. */
    oneui_widget_destroy(bar);
    oneui_widget_destroy(title);
    oneui_widget_destroy(sidebar);
    oneui_widget_destroy(editor);
    oneui_widget_destroy(footer);
    return shell;
}
