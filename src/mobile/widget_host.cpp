#include "oneui/mobile/widget_host.h"
#include "touch_scroll.h"
#include <cassert>
#include <chrono>

namespace oneui::mobile {
struct WidgetHost::State {
    Runtime runtime;
    std::shared_ptr<Widget> content;
    bool alive = true;
    unsigned dispatchDepth = 0;
    bool touchScrolling = false;
    TouchScroll scroll;
};
namespace {
class DispatchScope {
  public:
    explicit DispatchScope(unsigned &depth) : depth_(depth) { ++depth_; }
    ~DispatchScope() { --depth_; }

  private:
    unsigned &depth_;
};
} // namespace
WidgetHost::WidgetHost() : state_(std::make_shared<State>()) {}
WidgetHost::~WidgetHost() {
    auto state = state_;
    assert(state->runtime.checkThread() == ONEUI_MOBILE_OK);
    state->alive = false;
    if (auto content = std::move(state->content)) {
        content->detachFromOwner(state.get());
        content->clearInteractionState();
        content->onFocusChanged(false);
    }
}
Runtime &WidgetHost::runtime() { return state_->runtime; }
OneUiMobileResult WidgetHost::setTouchScrolling(bool enabled) {
    auto state = state_;
    const auto thread = state->runtime.checkThread();
    if (thread != ONEUI_MOBILE_OK)
        return thread;
    if (state->dispatchDepth || !state->alive)
        return ONEUI_MOBILE_INVALID_STATE;
    state->runtime.cancelPointers();
    pumpInput();
    if (!state->alive)
        return ONEUI_MOBILE_INVALID_STATE;
    state->scroll.reset();
    state->touchScrolling = enabled;
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult WidgetHost::setContent(std::shared_ptr<Widget> content) {
    auto state = state_;
    const auto thread = state->runtime.checkThread();
    if (thread != ONEUI_MOBILE_OK)
        return thread;
    if (!state->alive || state->dispatchDepth)
        return ONEUI_MOBILE_INVALID_STATE;
    if (content == state->content)
        return ONEUI_MOBILE_OK;
    if (content && content->hasCompositionOwner())
        return ONEUI_MOBILE_INVALID_STATE;
    if (state->runtime.invalidate() != ONEUI_MOBILE_OK)
        return ONEUI_MOBILE_INVALID_STATE;
    DispatchScope scope(state->dispatchDepth);
    auto old = std::move(state->content);
    if (old) {
        old->detachFromOwner(state.get());
        old->clearInteractionState();
        old->onFocusChanged(false);
    }
    if (!state->alive)
        return ONEUI_MOBILE_INVALID_STATE;
    state->runtime.cancelPointers();
    state->scroll.reset();
    state->runtime.setAnimating(false);
    state->content = std::move(content);
    if (state->content) {
        std::weak_ptr<State> weak = state;
        const auto invalidate = [weak] {
            if (auto owner = weak.lock(); owner && owner->alive)
                owner->runtime.invalidate();
        };
        state->content->attachToOwner(
            state.get(), invalidate, [invalidate](Rect) { invalidate(); },
            [weak] {
                if (auto owner = weak.lock(); owner && owner->alive)
                    owner->runtime.setAnimating(true);
            });
    }
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult WidgetHost::pumpInput() {
    auto state = state_;
    const auto thread = state->runtime.checkThread();
    if (thread != ONEUI_MOBILE_OK)
        return thread;
    if (!state->alive || state->dispatchDepth)
        return ONEUI_MOBILE_INVALID_STATE;
    DispatchScope scope(state->dispatchDepth);
    OneUiMobilePointerV1 event{};
    while (state->alive && state->runtime.pollPointer(event) == ONEUI_MOBILE_OK) {
        auto content = state->content;
        if (!content)
            continue;
        if (state->touchScrolling && state->scroll.dispatch(content, event))
            continue;
        if (event.phase == ONEUI_MOBILE_CANCEL && (event.id == -1 || event.primary)) {
            content->clearInteractionState();
            // Pointer cancellation (including an IME-driven Surface resize)
            // must not blur the text editor or discard its composition.
        } else if (event.primary) {
            MouseEvent mouse{{event.x, event.y}, MouseButton::Left};
            if (event.phase == ONEUI_MOBILE_DOWN)
                content->onMouseDown(mouse);
            else if (event.phase == ONEUI_MOBILE_MOVE)
                content->onMouseMove(mouse);
            else if (event.phase == ONEUI_MOBILE_UP)
                content->onMouseUp(mouse);
        }
    }
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult WidgetHost::paint(Canvas &canvas, uint64_t time) {
    return paintWhenNeeded([&canvas] { return &canvas; }, time);
}
OneUiMobileResult WidgetHost::paintWhenNeeded(const std::function<Canvas *()> &factory,
                                              uint64_t time) {
    auto state = state_;
    const auto input = pumpInput();
    if (input != ONEUI_MOBILE_OK)
        return input;
    if (!state->alive)
        return ONEUI_MOBILE_INVALID_STATE;
    auto content = state->content;
    const bool invalidated = state->runtime.dirty_;
    OneUiMobileFrameV1 frame{};
    const auto result = state->runtime.takeFrame(time, frame);
    if (result != ONEUI_MOBILE_OK || !content)
        return result;
    DispatchScope scope(state->dispatchDepth);
    try {
        state->runtime.setAnimating(false);
        content->setTextEnvironment(content->textFontFamily(), frame.scale);
        if (!state->alive)
            return ONEUI_MOBILE_INVALID_STATE;
        content->setFrame(
            {frame.content_x, frame.content_y, frame.content_width, frame.content_height});
        if (!state->alive)
            return ONEUI_MOBILE_INVALID_STATE;
        // Native display timestamps can use a different epoch. Existing widgets
        // start transitions on the C++ steady clock, so keep that same clock here.
        const auto animationTime = std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count();
        if (content->tickAnimations(animationTime))
            state->runtime.setAnimating(true);
        if (!invalidated && !state->runtime.dirty_)
            return ONEUI_MOBILE_EMPTY;
        if (!state->alive)
            return ONEUI_MOBILE_INVALID_STATE;
        OneUiMobileStateV1 live{};
        state->runtime.state(live);
        if (live.lifecycle != ONEUI_MOBILE_ACTIVE || !live.surface_available ||
            live.surface_generation != frame.surface_generation)
            return ONEUI_MOBILE_INVALID_STATE;
        auto *canvas = factory ? factory() : nullptr;
        if (!canvas)
            return ONEUI_MOBILE_FAILED;
        content->paint(*canvas);
    } catch (...) {
        state->runtime.invalidate();
        throw;
    }
    return ONEUI_MOBILE_OK;
}
} // namespace oneui::mobile
