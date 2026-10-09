#include "oneui/mobile/runtime.h"
#include <new>

struct OneUiMobileRuntime {
    oneui::mobile::Runtime value;
};
extern "C" {
uint32_t oneui_mobile_abi_version(void) { return 1; }
OneUiMobileResult oneui_mobile_create(OneUiMobileRuntime **out) {
    if (!out)
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    *out = nullptr;
    try {
        *out = new OneUiMobileRuntime;
        return ONEUI_MOBILE_OK;
    } catch (...) {
        return ONEUI_MOBILE_FAILED;
    }
}
OneUiMobileResult oneui_mobile_destroy(OneUiMobileRuntime *r) {
    if (!r)
        return ONEUI_MOBILE_OK;
    const auto result = r->value.checkThread();
    if (result == ONEUI_MOBILE_OK)
        delete r;
    return result;
}
OneUiMobileResult oneui_mobile_set_lifecycle(OneUiMobileRuntime *r, int32_t state) {
    return r ? r->value.setLifecycle(state) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_set_viewport_v1(OneUiMobileRuntime *r,
                                               const OneUiMobileViewportV1 *v) {
    return r && v ? r->value.setViewport(*v) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_set_surface(OneUiMobileRuntime *r, uint32_t value) {
    return r && value <= 1 ? r->value.setSurface(value != 0) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_invalidate(OneUiMobileRuntime *r) {
    return r ? r->value.invalidate() : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_set_animating(OneUiMobileRuntime *r, uint32_t value) {
    return r && value <= 1 ? r->value.setAnimating(value != 0) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_push_pointer_v1(OneUiMobileRuntime *r,
                                               const OneUiMobilePointerV1 *v) {
    return r && v ? r->value.pushPointer(*v) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_cancel_pointers(OneUiMobileRuntime *r) {
    return r ? r->value.cancelPointers() : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_poll_pointer_v1(OneUiMobileRuntime *r, OneUiMobilePointerV1 *v) {
    return r && v ? r->value.pollPointer(*v) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_take_frame_v1(OneUiMobileRuntime *r, uint64_t time,
                                             OneUiMobileFrameV1 *v) {
    return r && v ? r->value.takeFrame(time, *v) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_get_state_v1(OneUiMobileRuntime *r, OneUiMobileStateV1 *v) {
    return r && v ? r->value.state(*v) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
}
