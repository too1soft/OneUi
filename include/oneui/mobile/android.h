#pragma once
#include "oneui/mobile/runtime.h"

#if defined(__ANDROID__)
struct AInputEvent;
namespace oneui::mobile {
// Invoke on the Runtime's UI thread. The caller retains the AInputEvent.
// Mouse/joystick/key/hover/scroll are not translated into fake touch events.
// CANCEL clears the entire gesture; MOVE dispatches every active pointer.
ONEUI_MOBILE_API OneUiMobileResult dispatchAndroidMotion(Runtime &, const AInputEvent *);
} // namespace oneui::mobile
#endif
