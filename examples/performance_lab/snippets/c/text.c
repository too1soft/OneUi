#include <oneui/oneui_c_api.h>

/* UTF-8 lengths are byte counts, excluding the trailing NUL. */
OneUiWidget *example_c_text(void) {
    const char placeholder[] = "输入说明";
    const char initial[] = "中文 · 👩‍💻 · العربية\n第二行";
    OneUiWidget *input =
        oneui_text_field_create_utf8((OneUiUtf8String){placeholder, sizeof(placeholder) - 1});
    if (!input)
        return NULL;
    oneui_text_field_set_multiline(input, 1);
    oneui_text_field_set_text_utf8(input, (OneUiUtf8String){initial, sizeof(initial) - 1});
    /* Return ownership; the caller eventually destroys this handle. */
    return input;
}
