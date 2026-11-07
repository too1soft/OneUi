#include "oneui/mobile/application.h"
#include "oneui/view.h"
#include <limits>
#include <stdexcept>
#include <utility>

namespace oneui::mobile {
void ApplicationServices::wakeAfter(int milliseconds) {
    if (milliseconds < 0) return;
    const auto next = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    if (wake_ == std::chrono::steady_clock::time_point{} || next < wake_) wake_ = next;
}
int ApplicationServices::wakeDelayMillis() const {
    if (wake_ == std::chrono::steady_clock::time_point{}) return -1;
    const auto remaining = wake_ - std::chrono::steady_clock::now();
    return remaining <= decltype(remaining)::zero() ? 0 :
        static_cast<int>(std::min<int64_t>(60000,
            std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count() + 1));
}
bool ApplicationServices::consumeWakeup() {
    if (wakeDelayMillis() != 0) return false;
    wake_ = {};
    return true;
}
uint64_t ApplicationServices::request(std::string action, std::string payload) {
    if (action.empty() || action.size() > 128 || payload.size() > 1024 * 1024 ||
        pending_.size() >= 16 || bytes_ + payload.size() > 1024 * 1024 ||
        next_ == std::numeric_limits<uint64_t>::max()) return 0;
    for (const auto c : action)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')) return 0;
    bytes_ += payload.size();
    pending_.push_back({++next_, std::move(action), std::move(payload)});
    return next_;
}
bool ApplicationServices::take(ApplicationRequest &request) {
    if (pending_.empty()) return false;
    request = std::move(pending_.front());
    pending_.pop_front();
    bytes_ -= request.payload.size();
    return true;
}
ApplicationHost::ApplicationHost(const Factory &factory) {
    if (!factory) throw std::invalid_argument("OneUI application factory is required");
    application_ = factory(services_);
    if (!application_ || !application_->root()) throw std::invalid_argument("OneUI application root is required");
    auto root = application_->root();
    if (widgets_.setTouchScrolling(true) != ONEUI_MOBILE_OK || widgets_.setContent(root) != ONEUI_MOBILE_OK)
        throw std::invalid_argument("OneUI application root is already attached");
    input_ = std::make_unique<TextInputSession>(std::move(root));
    syncAppearance();
}
ApplicationHost::~ApplicationHost() {
    if (runtime().checkThread() != ONEUI_MOBILE_OK) std::terminate();
    input_->reset();
    application_->active(false);
    widgets_.setContent({});
    input_.reset();
    application_.reset();
    runtime().setLifecycle(ONEUI_MOBILE_DESTROYED);
}
void ApplicationHost::activate() {
    OneUiMobileStateV1 state{};
    runtime().state(state);
    const bool next = state.lifecycle == ONEUI_MOBILE_ACTIVE && state.surface_available;
    if (next != active_) { active_ = next; application_->active(next); }
}
OneUiMobileResult ApplicationHost::lifecycle(int state) {
    if (state != ONEUI_MOBILE_ACTIVE) input_->reset();
    const auto result = runtime().setLifecycle(state);
    widgets_.pumpInput(); activate();
    return result;
}
OneUiMobileResult ApplicationHost::viewport(const OneUiMobileViewportV1 &viewport) {
    const auto result = runtime().setViewport(viewport);
    if (result == ONEUI_MOBILE_OK)
        application_->viewport({viewport.width_px / viewport.scale, viewport.height_px / viewport.scale});
    return result;
}
OneUiMobileResult ApplicationHost::surface(bool available) {
    const auto result = runtime().setSurface(available);
    widgets_.pumpInput();
    if (available) input_->restoreComposition();
    activate();
    return result;
}
OneUiMobileResult ApplicationHost::pointer(const OneUiMobilePointerV1 &pointer) {
    const auto result = runtime().pushPointer(pointer);
    if (result != ONEUI_MOBILE_OK) runtime().cancelPointers();
    widgets_.pumpInput(); flush();
    return result;
}
OneUiMobileResult ApplicationHost::cancelPointers() {
    const auto result = runtime().cancelPointers();
    widgets_.pumpInput(); input_->restoreComposition();
    return result;
}
void ApplicationHost::flush() {
    if (services_.consumeWakeup()) runtime().invalidate();
    if (application_->hasPending()) {
        runtime().cancelPointers(); widgets_.pumpInput();
        application_->flush();
        // Retained editors keep their IME session and composition. snapshot()
        // invalidates a removed, disabled or changed editor through its weak
        // owner and conflict checks, without resetting an unrelated editor.
        input_->snapshot(); runtime().invalidate();
    }
    syncAppearance();
}
void ApplicationHost::syncAppearance() {
    OneUiMobileAppearanceStateV1 appearance{};
    application_->appearance().state(appearance);
    if (appearanceRevision_ != appearance.revision) {
        appearanceRevision_ = appearance.revision;
        application_->appearanceChanged(appearance);
        runtime().invalidate();
    }
}
OneUiMobileResult ApplicationHost::paint(const std::function<Canvas *()> &canvas, uint64_t timeNs) {
    flush();
    return widgets_.paintWhenNeeded([&] {
        // setFrame alone does not lay out legacy containers. Resolve descendants
        // first so IME scrolling uses the resized viewport before presentation.
        if (input_->snapshot().session)
            if (auto view = std::dynamic_pointer_cast<View>(application_->root())) view->layoutSubtree();
        input_->revealCaret();
        return canvas ? canvas() : nullptr;
    }, timeNs);
}
void ApplicationHost::reply(uint64_t id, int code, const std::string &payload) {
    if (runtime().checkThread() != ONEUI_MOBILE_OK || payload.size() > 1024 * 1024) return;
    application_->reply(id, code, payload); flush(); runtime().invalidate();
}
bool ApplicationHost::back() {
    cancelPointers();
    const auto handled = application_->back(); flush();
    return handled;
}
} // namespace oneui::mobile
