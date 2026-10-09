#include "oneui/mobile/c_api.h"
#include <stdio.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "line %d: %s\n", __LINE__, #test);                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    OneUiMobileRuntime *runtime = 0;
    OneUiMobileViewportV1 viewport = {800, 1200, 2, 0, 40, 0, 20, 0};
    OneUiMobileFrameV1 frame = {0};
    OneUiMobileStateV1 state = {0};
    OneUiMobilePointerV1 pointer = {9, ONEUI_MOBILE_DOWN, ONEUI_MOBILE_TOUCH, 60, 80, 1, 100, 0};
    CHECK(oneui_mobile_abi_version() == 1);
    CHECK(oneui_mobile_create(0) == ONEUI_MOBILE_INVALID_ARGUMENT);
    CHECK(oneui_mobile_destroy(0) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_create(&runtime) == ONEUI_MOBILE_OK && runtime);
    CHECK(oneui_mobile_set_surface(runtime, 2) == ONEUI_MOBILE_INVALID_ARGUMENT);
    CHECK(oneui_mobile_set_viewport_v1(runtime, &viewport) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_set_surface(runtime, 1) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_set_lifecycle(runtime, ONEUI_MOBILE_ACTIVE) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_take_frame_v1(runtime, 1, &frame) == ONEUI_MOBILE_OK);
    CHECK(frame.content_width == 400 && frame.content_height == 570);
    CHECK(oneui_mobile_take_frame_v1(runtime, 2, &frame) == ONEUI_MOBILE_EMPTY);
    CHECK(oneui_mobile_push_pointer_v1(runtime, &pointer) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_poll_pointer_v1(runtime, &pointer) == ONEUI_MOBILE_OK);
    CHECK(pointer.id == 9 && pointer.x == 30 && pointer.primary == 1);
    CHECK(oneui_mobile_cancel_pointers(runtime) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_poll_pointer_v1(runtime, &pointer) == ONEUI_MOBILE_OK && pointer.id == -1);
    CHECK(oneui_mobile_get_state_v1(runtime, &state) == ONEUI_MOBILE_OK && !state.active_pointers);
    CHECK(oneui_mobile_set_lifecycle(runtime, ONEUI_MOBILE_DESTROYED) == ONEUI_MOBILE_OK);
    CHECK(oneui_mobile_invalidate(runtime) == ONEUI_MOBILE_INVALID_STATE);
    CHECK(oneui_mobile_destroy(runtime) == ONEUI_MOBILE_OK);
    puts("mobile C ABI V1 consumer passed");
    return 0;
}
