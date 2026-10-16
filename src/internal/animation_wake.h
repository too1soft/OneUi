#pragma once

namespace oneui::internal {
// While a window ticks content animations it can open an AnimationWakeScope.
// A widget whose picture only changes at a known later time may then return
// false from tickAnimations and call requestAnimationWakeAt instead of asking
// for the next display refresh; the window wakes the content at the earliest
// requested time. Without an open scope (other backends, tests), the call
// returns false and the widget keeps requesting frames as before.
inline thread_local bool animationWakeOpen = false;
inline thread_local double animationWakeDueMs = 0.0;

inline bool requestAnimationWakeAt(double dueMs) {
    if (!animationWakeOpen || !(dueMs > 0.0)) return false;
    if (animationWakeDueMs <= 0.0 || dueMs < animationWakeDueMs) animationWakeDueMs = dueMs;
    return true;
}

class AnimationWakeScope {
public:
    AnimationWakeScope() : open_(animationWakeOpen), due_(animationWakeDueMs) {
        animationWakeOpen = true;
        animationWakeDueMs = 0.0;
    }
    ~AnimationWakeScope() {
        animationWakeOpen = open_;
        animationWakeDueMs = due_;
    }
    AnimationWakeScope(const AnimationWakeScope&) = delete;
    AnimationWakeScope& operator=(const AnimationWakeScope&) = delete;
    // Earliest requested wake, or 0 when no widget asked for one.
    double dueMs() const { return animationWakeDueMs; }

private:
    bool open_;
    double due_;
};
}
