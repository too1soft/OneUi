#pragma once
#include "oneui/mobile/c_api.h"
#include <array>
#include <cstddef>
#include <thread>

namespace oneui::mobile {
// Allocation-free state machine. Native hosts own scheduling and GPU resources.
// Pointer capacity and event queue are bounded; overflow cancels the gesture.
class ONEUI_MOBILE_API Runtime final {
  public:
    Runtime();
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    OneUiMobileResult checkThread() const;
    OneUiMobileResult setLifecycle(int32_t state);
    OneUiMobileResult setViewport(const OneUiMobileViewportV1 &viewport);
    OneUiMobileResult setSurface(bool available);
    OneUiMobileResult invalidate();
    OneUiMobileResult setAnimating(bool enabled);
    OneUiMobileResult pushPointer(const OneUiMobilePointerV1 &pointer);
    OneUiMobileResult cancelPointers();
    OneUiMobileResult pollPointer(OneUiMobilePointerV1 &pointer);
    OneUiMobileResult takeFrame(uint64_t timeNs, OneUiMobileFrameV1 &frame);
    OneUiMobileResult state(OneUiMobileStateV1 &state) const;

  private:
    OneUiMobileResult writable() const;
    bool drawable() const;
    void resetPointers();
    bool enqueue(const OneUiMobilePointerV1 &event);
    std::thread::id thread_;
    int32_t lifecycle_ = ONEUI_MOBILE_CREATED;
    OneUiMobileViewportV1 viewport_{};
    bool surface_ = false, viewportSet_ = false, dirty_ = true, animating_ = false;
    uint64_t generation_ = 0, frameTime_ = 0, pointerTime_ = 0;
    std::array<OneUiMobilePointerV1, 16> active_{};
    std::size_t activeCount_ = 0;
    std::array<OneUiMobilePointerV1, 128> events_{};
    std::size_t head_ = 0, count_ = 0;
};
} // namespace oneui::mobile
