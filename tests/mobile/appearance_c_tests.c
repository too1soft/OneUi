#include "oneui/mobile/appearance_c.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    OneUiMobileAppearance *a = 0;
    OneUiMobileAppearanceStateV1 state = {0};
    OneUiMobileAppearanceOptionsV1 options = {0};
    CHECK(oneui_mobile_appearance_create(0) == ONEUI_MOBILE_INVALID_ARGUMENT);
    CHECK(oneui_mobile_appearance_create(&a) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_appearance_observe_v1(a, 2, 1) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_appearance_observe_v1(a, 3, 1) == ONEUI_MOBILE_INVALID_ARGUMENT);
    CHECK(oneui_mobile_appearance_get_state_v1(a, &state) == ONEUI_MOBILE_OK);
    CHECK(state.system_theme == 2 && state.effective_theme == 2 && state.applied_revision == 0);
    options.theme_mode = ONEUI_MOBILE_THEME_LIGHT;
    CHECK(oneui_mobile_appearance_set_options_v1(a, &options) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_appearance_get_state_v1(a, &state) == ONEUI_MOBILE_OK);
    CHECK(state.system_theme == 2 && state.effective_theme == 1);
    CHECK(oneui_mobile_appearance_acknowledge_v1(a, state.revision) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_appearance_destroy(a) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_appearance_destroy(0) == ONEUI_MOBILE_OK);
    puts("Appearance C V1 consumer passed");
    return 0;
}
