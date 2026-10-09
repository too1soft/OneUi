#include <oneui/oneui_c_api.h>
#define UTF8(s) ((OneUiUtf8String){s, sizeof(s) - 1})

OneUiWidget *example_c_layout(void) {
    /* Direction 1 is Row. Grid and Wrap currently have no C constructors. */
    OneUiWidget *row = oneui_stack_create((OneUiStackDirection)1);
    OneUiWidget *label = oneui_label_create_utf8(UTF8("布局示例"));
    if (!row || !label) {
        if (row) oneui_widget_destroy(row);
        if (label) oneui_widget_destroy(label);
        return NULL;
    }
    oneui_stack_set_gap(row, 12);
    oneui_stack_add(row, label);
    /* Stack keeps the native child alive. This label has no caller-owned
       callbacks; release the temporary ABI handle, not the retained child. */
    oneui_widget_destroy(label);
    return row;
}
