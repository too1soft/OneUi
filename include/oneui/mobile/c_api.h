#pragma once
#include <stdint.h>

/* This optional library has its own ABI. Desktop OneUI ABI 43 is unchanged.
 * V1 records are immutable; future records/functions use new versioned names.
 * All calls, including destroy, belong to the thread that created the runtime.
 * No callback, OS service, renderer, or event loop is installed implicitly. */
#if defined(_WIN32) && !defined(ONEUI_MOBILE_STATIC)
#if defined(ONEUI_MOBILE_BUILD)
#define ONEUI_MOBILE_API __declspec(dllexport)
#else
#define ONEUI_MOBILE_API __declspec(dllimport)
#endif
#elif defined(_WIN32)
#define ONEUI_MOBILE_API
#else
#define ONEUI_MOBILE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef struct OneUiMobileRuntime OneUiMobileRuntime;
typedef enum OneUiMobileResult {
    ONEUI_MOBILE_OK = 0,
    ONEUI_MOBILE_EMPTY = 1,
    ONEUI_MOBILE_INVALID_ARGUMENT = 2,
    ONEUI_MOBILE_INVALID_STATE = 3,
    ONEUI_MOBILE_WRONG_THREAD = 4,
    ONEUI_MOBILE_CAPACITY = 5,
    ONEUI_MOBILE_FAILED = 6
} OneUiMobileResult;
typedef enum OneUiMobileLifecycle {
    ONEUI_MOBILE_CREATED = 0,
    ONEUI_MOBILE_ACTIVE = 1,
    ONEUI_MOBILE_INACTIVE = 2,
    ONEUI_MOBILE_BACKGROUND = 3,
    ONEUI_MOBILE_DESTROYED = 4
} OneUiMobileLifecycle;
typedef enum OneUiMobilePointerPhase {
    ONEUI_MOBILE_DOWN = 0,
    ONEUI_MOBILE_MOVE = 1,
    ONEUI_MOBILE_UP = 2,
    ONEUI_MOBILE_CANCEL = 3
} OneUiMobilePointerPhase;
typedef enum OneUiMobilePointerKind {
    ONEUI_MOBILE_TOUCH = 0,
    ONEUI_MOBILE_PEN = 1
} OneUiMobilePointerKind;

typedef struct OneUiMobileViewportV1 {
    float width_px, height_px, scale;
    float safe_left_px, safe_top_px, safe_right_px, safe_bottom_px;
    /* Bottom obstruction measured from the surface bottom, not added to safe
     * bottom. Pass zero when the OS already resized the surface for the IME. */
    float keyboard_bottom_px;
} OneUiMobileViewportV1;

typedef struct OneUiMobilePointerV1 {
    int32_t id; /* Stable across down/move/up; -1 is reserved for cancel-all. */
    int32_t phase, kind;
    float x, y, pressure;
    uint64_t time_ns; /* Monotonic OS event timestamp, never wall-clock time. */
    uint32_t primary; /* Output only. Never promotes a second finger mid-gesture. */
} OneUiMobilePointerV1;

typedef struct OneUiMobileFrameV1 {
    uint64_t time_ns, surface_generation;
    float content_x, content_y, content_width, content_height;
    float scale;
} OneUiMobileFrameV1;

typedef struct OneUiMobileStateV1 {
    int32_t lifecycle;
    uint32_t surface_available, needs_frame, active_pointers;
    uint64_t surface_generation;
} OneUiMobileStateV1;

ONEUI_MOBILE_API uint32_t oneui_mobile_abi_version(void);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_create(OneUiMobileRuntime **out_runtime);
/* Returns WRONG_THREAD without destroying on a foreign thread. NULL is OK. */
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_destroy(OneUiMobileRuntime *runtime);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_set_lifecycle(OneUiMobileRuntime *, int32_t state);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_set_viewport_v1(OneUiMobileRuntime *,
                                                                const OneUiMobileViewportV1 *);
/* Pass 0 before releasing a native surface and 1 after acquiring its replacement. */
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_set_surface(OneUiMobileRuntime *,
                                                            uint32_t available);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_invalidate(OneUiMobileRuntime *);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_set_animating(OneUiMobileRuntime *,
                                                              uint32_t enabled);
/* Input coordinates are physical pixels; polled coordinates are logical pixels. */
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_push_pointer_v1(OneUiMobileRuntime *,
                                                                const OneUiMobilePointerV1 *);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_cancel_pointers(OneUiMobileRuntime *);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_poll_pointer_v1(OneUiMobileRuntime *,
                                                                OneUiMobilePointerV1 *);
/* Consumes dirty state before painting, preserving invalidation during paint.
 * EMPTY means do not render/re-arm a frame unless state.needs_frame becomes 1. */
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_take_frame_v1(OneUiMobileRuntime *,
                                                              uint64_t time_ns,
                                                              OneUiMobileFrameV1 *);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_get_state_v1(OneUiMobileRuntime *,
                                                             OneUiMobileStateV1 *);
#ifdef __cplusplus
}
#endif
