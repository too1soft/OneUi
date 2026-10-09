#include <oneui/oneui_c_api.h>

/* The caller retains user_data until the callback is cleared or the widget
   is destroyed. Destroy the returned handle with oneui_widget_destroy. */
OneUiWidget *example_c_button(OneUiVoidCallback on_click, void *user_data) {
    const char label[] = "执行";
    OneUiUtf8String text = {label, sizeof(label) - 1};
    OneUiWidget *button = oneui_button_create_utf8(text);
    if (button)
        oneui_button_set_on_click(button, on_click, user_data);
    return button;
}
