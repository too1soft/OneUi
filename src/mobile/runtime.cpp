#include "oneui/mobile/runtime.h"
#include <algorithm>
#include <cmath>

namespace oneui::mobile {
namespace {
bool validViewport(const OneUiMobileViewportV1 &v) {
    const float values[] = {v.width_px,    v.height_px,     v.scale,          v.safe_left_px,
                            v.safe_top_px, v.safe_right_px, v.safe_bottom_px, v.keyboard_bottom_px};
    for (float value : values)
        if (!std::isfinite(value) || value < 0)
            return false;
    return v.width_px > 0 && v.height_px > 0 && v.scale > 0 &&
           std::isfinite(v.width_px / v.scale) && std::isfinite(v.height_px / v.scale) &&
           v.safe_left_px <= v.width_px - v.safe_right_px &&
           v.safe_top_px <= v.height_px - v.safe_bottom_px && v.keyboard_bottom_px <= v.height_px;
}
bool sameViewport(const OneUiMobileViewportV1 &a, const OneUiMobileViewportV1 &b) {
    return a.width_px == b.width_px && a.height_px == b.height_px && a.scale == b.scale &&
           a.safe_left_px == b.safe_left_px && a.safe_top_px == b.safe_top_px &&
           a.safe_right_px == b.safe_right_px && a.safe_bottom_px == b.safe_bottom_px &&
           a.keyboard_bottom_px == b.keyboard_bottom_px;
}
} // namespace
Runtime::Runtime() : thread_(std::this_thread::get_id()) {}
OneUiMobileResult Runtime::checkThread() const {
    return thread_ == std::this_thread::get_id() ? ONEUI_MOBILE_OK : ONEUI_MOBILE_WRONG_THREAD;
}
OneUiMobileResult Runtime::writable() const {
    const auto result = checkThread();
    if (result != ONEUI_MOBILE_OK)
        return result;
    return lifecycle_ == ONEUI_MOBILE_DESTROYED ? ONEUI_MOBILE_INVALID_STATE : ONEUI_MOBILE_OK;
}
bool Runtime::drawable() const {
    return lifecycle_ == ONEUI_MOBILE_ACTIVE && surface_ && viewportSet_ &&
           viewport_.width_px > viewport_.safe_left_px + viewport_.safe_right_px &&
           viewport_.height_px > viewport_.safe_top_px + std::max(viewport_.safe_bottom_px,
                                                                  viewport_.keyboard_bottom_px);
}
void Runtime::resetPointers() {
    // Cancel-all must supersede queued down/up pairs, including an already lifted
    // finger whose up has not reached the widget yet. Never synthesize a click.
    head_ = 0;
    count_ = 1;
    activeCount_ = 0;
    events_[0] = {-1, ONEUI_MOBILE_CANCEL, ONEUI_MOBILE_TOUCH, 0, 0, 0, pointerTime_, 0};
}
OneUiMobileResult Runtime::setLifecycle(int32_t value) {
    const auto result = writable();
    if (result != ONEUI_MOBILE_OK)
        return result;
    if (value < ONEUI_MOBILE_CREATED || value > ONEUI_MOBILE_DESTROYED)
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    if (value == lifecycle_)
        return ONEUI_MOBILE_OK;
    if (value == ONEUI_MOBILE_CREATED)
        return ONEUI_MOBILE_INVALID_STATE;
    lifecycle_ = value;
    if (value != ONEUI_MOBILE_ACTIVE)
        resetPointers();
    else
        dirty_ = true;
    if (value == ONEUI_MOBILE_DESTROYED) {
        surface_ = false;
        animating_ = false;
    }
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Runtime::setViewport(const OneUiMobileViewportV1 &value) {
    const auto result = writable();
    if (result != ONEUI_MOBILE_OK)
        return result;
    if (!validViewport(value))
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    if (viewportSet_ && sameViewport(viewport_, value))
        return ONEUI_MOBILE_OK;
    if (viewportSet_)
        resetPointers();
    viewport_ = value;
    viewportSet_ = true;
    dirty_ = true;
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Runtime::setSurface(bool available) {
    const auto result = writable();
    if (result != ONEUI_MOBILE_OK)
        return result;
    if (surface_ == available)
        return ONEUI_MOBILE_OK;
    surface_ = available;
    if (available) {
        ++generation_;
        dirty_ = true;
    } else
        resetPointers();
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Runtime::invalidate() {
    const auto result = writable();
    if (result == ONEUI_MOBILE_OK)
        dirty_ = true;
    return result;
}
OneUiMobileResult Runtime::setAnimating(bool enabled) {
    const auto result = writable();
    if (result == ONEUI_MOBILE_OK)
        animating_ = enabled;
    return result;
}
bool Runtime::enqueue(const OneUiMobilePointerV1 &event) {
    if (count_ == events_.size()) {
        resetPointers();
        return false;
    }
    events_[(head_ + count_) % events_.size()] = event;
    ++count_;
    return true;
}
OneUiMobileResult Runtime::pushPointer(const OneUiMobilePointerV1 &value) {
    const auto result = writable();
    if (result != ONEUI_MOBILE_OK)
        return result;
    if (!drawable())
        return ONEUI_MOBILE_INVALID_STATE;
    if (value.id < 0 || value.phase < ONEUI_MOBILE_DOWN || value.phase > ONEUI_MOBILE_CANCEL ||
        value.kind < ONEUI_MOBILE_TOUCH || value.kind > ONEUI_MOBILE_PEN ||
        !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.pressure) ||
        value.pressure < 0 || value.pressure > 1 || value.time_ns < pointerTime_)
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    std::size_t index = 0;
    while (index < activeCount_ && active_[index].id != value.id)
        ++index;
    if (value.phase == ONEUI_MOBILE_DOWN) {
        if (index != activeCount_)
            return ONEUI_MOBILE_INVALID_STATE;
        if (activeCount_ == active_.size()) {
            resetPointers();
            return ONEUI_MOBILE_CAPACITY;
        }
    } else if (index == activeCount_ || value.kind != active_[index].kind) {
        return ONEUI_MOBILE_INVALID_STATE;
    }
    auto event = value;
    event.x /= viewport_.scale;
    event.y /= viewport_.scale;
    if (!std::isfinite(event.x) || !std::isfinite(event.y))
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    event.primary =
        value.phase == ONEUI_MOBILE_DOWN ? (activeCount_ == 0 ? 1u : 0u) : active_[index].primary;
    pointerTime_ = value.time_ns;
    if (!enqueue(event))
        return ONEUI_MOBILE_CAPACITY;
    if (value.phase == ONEUI_MOBILE_DOWN)
        active_[activeCount_++] = event;
    else if (value.phase == ONEUI_MOBILE_UP || value.phase == ONEUI_MOBILE_CANCEL) {
        for (std::size_t i = index + 1; i < activeCount_; ++i)
            active_[i - 1] = active_[i];
        --activeCount_;
    } else
        active_[index] = event;
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Runtime::cancelPointers() {
    const auto result = writable();
    if (result == ONEUI_MOBILE_OK)
        resetPointers();
    return result;
}
OneUiMobileResult Runtime::pollPointer(OneUiMobilePointerV1 &event) {
    event = {};
    const auto result = checkThread();
    if (result != ONEUI_MOBILE_OK)
        return result;
    if (!count_)
        return ONEUI_MOBILE_EMPTY;
    event = events_[head_];
    head_ = (head_ + 1) % events_.size();
    --count_;
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Runtime::takeFrame(uint64_t time, OneUiMobileFrameV1 &frame) {
    frame = {};
    const auto result = writable();
    if (result != ONEUI_MOBILE_OK)
        return result;
    if (time < frameTime_)
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    if (!drawable() || (!dirty_ && !animating_))
        return ONEUI_MOBILE_EMPTY;
    frameTime_ = time;
    dirty_ = false;
    const auto &v = viewport_;
    frame = {time,
             generation_,
             v.safe_left_px / v.scale,
             v.safe_top_px / v.scale,
             (v.width_px - v.safe_left_px - v.safe_right_px) / v.scale,
             std::max(0.f, v.height_px - v.safe_top_px -
                               std::max(v.safe_bottom_px, v.keyboard_bottom_px)) /
                 v.scale,
             v.scale};
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Runtime::state(OneUiMobileStateV1 &value) const {
    value = {};
    const auto result = checkThread();
    if (result != ONEUI_MOBILE_OK)
        return result;
    value = {lifecycle_, surface_ ? 1u : 0u, drawable() && (dirty_ || animating_) ? 1u : 0u,
             static_cast<uint32_t>(activeCount_), generation_};
    return ONEUI_MOBILE_OK;
}
} // namespace oneui::mobile
