#include "oneui/mobile/android.h"
#include <algorithm>
#include <android/input.h>
#include <cmath>

namespace oneui::mobile {
namespace {
OneUiMobileResult dispatchPointer(Runtime &runtime, const AInputEvent *input, std::size_t index,
                                  int phase) {
    const int tool = AMotionEvent_getToolType(input, index);
    if (tool != AMOTION_EVENT_TOOL_TYPE_FINGER && tool != AMOTION_EVENT_TOOL_TYPE_STYLUS &&
        tool != AMOTION_EVENT_TOOL_TYPE_ERASER)
        return ONEUI_MOBILE_EMPTY;
    const auto time = AMotionEvent_getEventTime(input);
    const auto pressure = AMotionEvent_getPressure(input, index);
    if (time < 0 || !std::isfinite(pressure))
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    OneUiMobilePointerV1 event{AMotionEvent_getPointerId(input, index),
                               phase,
                               tool == AMOTION_EVENT_TOOL_TYPE_FINGER ? ONEUI_MOBILE_TOUCH
                                                                      : ONEUI_MOBILE_PEN,
                               AMotionEvent_getX(input, index),
                               AMotionEvent_getY(input, index),
                               std::clamp(pressure, 0.f, 1.f),
                               static_cast<uint64_t>(time),
                               0};
    return runtime.pushPointer(event);
}
} // namespace
OneUiMobileResult dispatchAndroidMotion(Runtime &runtime, const AInputEvent *input) {
    const auto thread = runtime.checkThread();
    if (thread != ONEUI_MOBILE_OK)
        return thread;
    if (!input)
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    if (AInputEvent_getType(input) != AINPUT_EVENT_TYPE_MOTION)
        return ONEUI_MOBILE_EMPTY;
    const auto source = AInputEvent_getSource(input);
    if ((source & AINPUT_SOURCE_TOUCHSCREEN) != AINPUT_SOURCE_TOUCHSCREEN &&
        (source & AINPUT_SOURCE_STYLUS) != AINPUT_SOURCE_STYLUS)
        return ONEUI_MOBILE_EMPTY;
    const auto action = AMotionEvent_getAction(input);
    const auto masked = action & AMOTION_EVENT_ACTION_MASK;
    if (masked == AMOTION_EVENT_ACTION_CANCEL)
        return runtime.cancelPointers();
    const auto count = AMotionEvent_getPointerCount(input);
    if (count == 0 || count > 16) {
        runtime.cancelPointers();
        return ONEUI_MOBILE_CAPACITY;
    }
    if (masked == AMOTION_EVENT_ACTION_MOVE) {
        auto result = ONEUI_MOBILE_EMPTY;
        for (std::size_t i = 0; i < count; ++i) {
            const auto next = dispatchPointer(runtime, input, i, ONEUI_MOBILE_MOVE);
            if (next != ONEUI_MOBILE_OK && next != ONEUI_MOBILE_EMPTY) {
                runtime.cancelPointers();
                return next;
            }
            if (next == ONEUI_MOBILE_OK)
                result = next;
        }
        return result;
    }
    int phase;
    if (masked == AMOTION_EVENT_ACTION_DOWN || masked == AMOTION_EVENT_ACTION_POINTER_DOWN)
        phase = ONEUI_MOBILE_DOWN;
    else if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP)
        phase = ONEUI_MOBILE_UP;
    else
        return ONEUI_MOBILE_EMPTY;
    const auto index =
        static_cast<std::size_t>((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                 AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    if (index >= count)
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    const auto result = dispatchPointer(runtime, input, index, phase);
    if (result != ONEUI_MOBILE_OK && result != ONEUI_MOBILE_EMPTY)
        runtime.cancelPointers();
    return result;
}
} // namespace oneui::mobile
