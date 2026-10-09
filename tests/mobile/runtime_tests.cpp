#include "oneui/mobile/runtime.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using oneui::mobile::Runtime;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void ready(Runtime &runtime) {
    require(runtime.setViewport({1080, 2400, 3, 0, 90, 0, 60, 0}) == ONEUI_MOBILE_OK, "viewport");
    require(runtime.setSurface(true) == ONEUI_MOBILE_OK, "surface");
    require(runtime.setLifecycle(ONEUI_MOBILE_ACTIVE) == ONEUI_MOBILE_OK, "activate");
}
OneUiMobilePointerV1 pointer(int id, int phase, uint64_t time = 100) {
    return {id, phase, ONEUI_MOBILE_TOUCH, 300, 600, .5f, time, 99};
}
OneUiMobileStateV1 state(Runtime &runtime) {
    OneUiMobileStateV1 value{};
    require(runtime.state(value) == ONEUI_MOBILE_OK, "state");
    return value;
}
OneUiMobilePointerV1 poll(Runtime &runtime) {
    OneUiMobilePointerV1 event{};
    require(runtime.pollPointer(event) == ONEUI_MOBILE_OK, "poll");
    return event;
}
void idleAndLifecycle() {
    Runtime runtime;
    OneUiMobileFrameV1 frame{};
    require(runtime.takeFrame(1, frame) == ONEUI_MOBILE_EMPTY, "no surface means no frame");
    ready(runtime);
    require(runtime.takeFrame(2, frame) == ONEUI_MOBILE_OK, "first frame");
    require(frame.content_width == 360 && frame.content_y == 30 && frame.content_height == 750,
            "logical safe area");
    require(!state(runtime).needs_frame, "idle must stop frame scheduling");
    require(runtime.takeFrame(3, frame) == ONEUI_MOBILE_EMPTY, "idle takes no frame");
    runtime.setAnimating(true);
    require(state(runtime).needs_frame, "animation requests next frame");
    runtime.setLifecycle(ONEUI_MOBILE_BACKGROUND);
    runtime.invalidate();
    require(!state(runtime).needs_frame, "background suppresses animations and dirty frames");
    require(runtime.takeFrame(4, frame) == ONEUI_MOBILE_EMPTY, "background frame rejected");
    runtime.setLifecycle(ONEUI_MOBILE_ACTIVE);
    require(runtime.takeFrame(5, frame) == ONEUI_MOBILE_OK, "resume repaints");
    require(runtime.takeFrame(4, frame) == ONEUI_MOBILE_INVALID_ARGUMENT, "monotonic frame time");
    runtime.setAnimating(false);
    runtime.invalidate();
    require(runtime.takeFrame(6, frame) == ONEUI_MOBILE_OK, "repaint consumes dirty state");
    runtime.invalidate(); // Invalidation while the native host is painting.
    require(state(runtime).needs_frame, "invalidation during paint survives");
    runtime.setSurface(false);
    require(!state(runtime).needs_frame, "lost surface suspends render");
    runtime.setSurface(true);
    runtime.takeFrame(7, frame);
    require(frame.surface_generation == 2, "replacement surface increments generation");
    runtime.setSurface(true);
    require(state(runtime).surface_generation == 2, "duplicate callback is idempotent");
    runtime.setLifecycle(ONEUI_MOBILE_DESTROYED);
    require(runtime.invalidate() == ONEUI_MOBILE_INVALID_STATE, "destroyed runtime cannot restart");
    require(!state(runtime).surface_available, "destroy releases surface state");
}
void multiplePointers() {
    Runtime runtime;
    ready(runtime);
    runtime.pushPointer(pointer(7, ONEUI_MOBILE_DOWN));
    runtime.pushPointer(pointer(23, ONEUI_MOBILE_DOWN));
    auto first = poll(runtime);
    auto second = poll(runtime);
    require(first.id == 7 && first.primary == 1 && first.x == 100 && first.y == 200,
            "logical primary pointer");
    require(second.id == 23 && !second.primary, "stable secondary id");
    require(runtime.pushPointer(pointer(7, ONEUI_MOBILE_DOWN)) == ONEUI_MOBILE_INVALID_STATE,
            "duplicate down");
    require(runtime.pushPointer(pointer(91, ONEUI_MOBILE_MOVE)) == ONEUI_MOBILE_INVALID_STATE,
            "unknown move");
    runtime.pushPointer(pointer(7, ONEUI_MOBILE_UP));
    poll(runtime);
    runtime.pushPointer(pointer(23, ONEUI_MOBILE_MOVE));
    require(!poll(runtime).primary, "lifting first finger cannot promote second");
    runtime.pushPointer(pointer(41, ONEUI_MOBILE_DOWN));
    require(!poll(runtime).primary, "new third finger cannot become mouse during gesture");
    runtime.cancelPointers();
    auto cancelled = poll(runtime);
    require(cancelled.id == -1 && cancelled.phase == ONEUI_MOBILE_CANCEL &&
                state(runtime).active_pointers == 0,
            "cancel all");
    runtime.pushPointer(pointer(23, ONEUI_MOBILE_DOWN, 101));
    require(poll(runtime).primary == 1, "new gesture can reuse ids");
}
void cancellationAndBounds() {
    Runtime runtime;
    ready(runtime);
    runtime.pushPointer(pointer(0, ONEUI_MOBILE_DOWN));
    runtime.pushPointer(pointer(0, ONEUI_MOBILE_UP));
    runtime.setLifecycle(ONEUI_MOBILE_INACTIVE);
    require(poll(runtime).phase == ONEUI_MOBILE_CANCEL, "pause replaces queued tap with cancel");
    require(runtime.pushPointer(pointer(0, ONEUI_MOBILE_DOWN)) == ONEUI_MOBILE_INVALID_STATE,
            "paused input rejected");
    runtime.setLifecycle(ONEUI_MOBILE_ACTIVE);
    for (int i = 0; i < 16; ++i)
        require(runtime.pushPointer(pointer(i, ONEUI_MOBILE_DOWN)) == ONEUI_MOBILE_OK,
                "pointer slots");
    require(runtime.pushPointer(pointer(16, ONEUI_MOBILE_DOWN)) == ONEUI_MOBILE_CAPACITY,
            "bounded pointer storage");
    require(state(runtime).active_pointers == 0 && poll(runtime).phase == ONEUI_MOBILE_CANCEL,
            "overflow resets capture");
    runtime.pushPointer(pointer(0, ONEUI_MOBILE_DOWN));
    for (int i = 0; i < 127; ++i)
        require(runtime.pushPointer(pointer(0, ONEUI_MOBILE_MOVE)) == ONEUI_MOBILE_OK,
                "bounded queue");
    require(runtime.pushPointer(pointer(0, ONEUI_MOBILE_MOVE)) == ONEUI_MOBILE_CAPACITY,
            "backpressure reported");
    require(poll(runtime).phase == ONEUI_MOBILE_CANCEL,
            "queue overflow cannot leave pressed widget");
}
void viewportAndValidation() {
    Runtime runtime;
    ready(runtime);
    runtime.pushPointer(pointer(0, ONEUI_MOBILE_DOWN));
    poll(runtime);
    require(runtime.setViewport({1080, 2400, 3, 0, 90, 0, 60, 900}) == ONEUI_MOBILE_OK,
            "keyboard viewport");
    require(poll(runtime).phase == ONEUI_MOBILE_CANCEL, "resize cancels old coordinate gesture");
    OneUiMobileFrameV1 frame{};
    runtime.takeFrame(1, frame);
    require(frame.content_height == 470, "keyboard max safe bottom, not additive");
    require(runtime.setViewport({100, 100, 0, 0, 0, 0, 0, 0}) == ONEUI_MOBILE_INVALID_ARGUMENT,
            "zero scale");
    require(runtime.setViewport({100, 100, 1, 60, 0, 60, 0, 0}) == ONEUI_MOBILE_INVALID_ARGUMENT,
            "overlapping insets");
    require(runtime.setViewport({100, 100, 1, 0, 0, 0, 0, 100}) == ONEUI_MOBILE_OK,
            "fully occluded viewport");
    require(!state(runtime).needs_frame, "zero content stops render");
    ready(runtime);
    auto invalid = pointer(0, ONEUI_MOBILE_DOWN);
    invalid.x = std::numeric_limits<float>::quiet_NaN();
    require(runtime.pushPointer(invalid) == ONEUI_MOBILE_INVALID_ARGUMENT,
            "NaN cannot reach layout");
    require(runtime.setLifecycle(999) == ONEUI_MOBILE_INVALID_ARGUMENT, "unknown lifecycle");
    require(runtime.setLifecycle(ONEUI_MOBILE_CREATED) == ONEUI_MOBILE_INVALID_STATE,
            "no resurrection to created");
    OneUiMobileResult foreign = ONEUI_MOBILE_OK;
    std::thread other([&] { foreign = runtime.invalidate(); });
    other.join();
    require(foreign == ONEUI_MOBILE_WRONG_THREAD, "UI affinity enforced");
}
} // namespace
int main() {
    try {
        idleAndLifecycle();
        multiplePointers();
        cancellationAndBounds();
        viewportAndValidation();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout
        << "mobile runtime: lifecycle, input, backpressure, viewport, thread affinity passed\n";
}
