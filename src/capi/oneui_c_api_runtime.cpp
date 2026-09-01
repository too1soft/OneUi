#include "oneui/oneui_c_api.h"

const char* oneui_version(void) {
    return "0.1.0";
}

unsigned int oneui_utf8_abi_version(void) {
    return ONEUI_UTF8_ABI_VERSION;
}
