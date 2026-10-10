#include "oneui/oneui_view_c_api.h"
#include <stddef.h>
_Static_assert(ONEUI_UTF8_ABI_VERSION == 44u, "ABI version");
_Static_assert(ONEUI_VIEW_PROTOCOL_VERSION == 1u, "View protocol");
_Static_assert(offsetof(OneUiValue, as) == 8, "value union offset");
_Static_assert(sizeof(((OneUiViewModelVTable*)0)->schema_hash) == 32, "schema SHA256");
int main(void) {
    OneUiValue value = {0};
    OneUiViewModelVTable table = {0};
    value.tag = ONEUI_VALUE_BOOL;
    table.struct_size = sizeof(table);
    return value.as.boolean != 0 || table.struct_size == 0;
}
