#include "oneui/oneui_view_c_api.h"
#include <stddef.h>
#include <string.h>
size_t oneui_test_view_value_layout(unsigned field) {
    OneUiValue value;
    switch (field) {
    case 0: return sizeof(value);
    case 1: return offsetof(OneUiValue, as);
    case 2: return sizeof(value.as);
    case 3: return sizeof(value.as.utf8);
    case 4: return (size_t)((char *)&value.as.utf8.length - (char *)&value.as.utf8);
    default: return 0;
    }
}
int oneui_test_view_value(const OneUiValue *input, OneUiValue *output) {
    static const char text[] = "value";
    memset(output, 0, sizeof(*output));
    if (input->tag == ONEUI_VALUE_I64 && input->as.integer == -123456789123LL) {
        output->tag = ONEUI_VALUE_F64;
        output->as.number = 3.25;
        return 1;
    }
    if (input->tag == ONEUI_VALUE_BOOL && input->as.boolean == 1) {
        output->tag = ONEUI_VALUE_UTF8;
        output->as.utf8.data = text;
        output->as.utf8.length = sizeof(text) - 1;
        return 1;
    }
    return 0;
}
