#pragma once
#include "oneui/vpn/c_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional OneUI::vpn_json adapter. Strict bounded JSON -> the same portable
 * validator/state machine used by native structs. The call copies all data. */
OneUiVpnResult oneui_vpn_connect_json_v1(OneUiVpnSession *, const char *json, size_t length);
#ifdef __cplusplus
}
#endif
