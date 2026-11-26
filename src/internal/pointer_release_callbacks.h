#pragma once

#include <functional>
#include <utility>
#include <vector>

namespace oneui {
class Window;

namespace internal {

class PointerReleaseCallbacks {
public:
    using Actions = std::vector<std::function<void()>>;

    PointerReleaseCallbacks(const Window& window, Actions& actions)
        : window_(&window), actions_(actions), previous_(std::exchange(active_, this)) {}
    ~PointerReleaseCallbacks() { active_ = previous_; }
    PointerReleaseCallbacks(const PointerReleaseCallbacks&) = delete;
    PointerReleaseCallbacks& operator=(const PointerReleaseCallbacks&) = delete;

    static bool enqueue(const Window& window, std::function<void()>& action) {
        if (!active_ || active_->window_ != &window) return false;
        auto* owner = active_;
        // A nested native loop may release the same window again while an outer
        // widget handler is still active. Wait for that outer handler to return.
        for (auto* turn = owner->previous_; turn; turn = turn->previous_) {
            if (turn->window_ == &window) owner = turn;
        }
        owner->actions_.push_back(std::move(action));
        return true;
    }

private:
    inline static thread_local PointerReleaseCallbacks* active_ = nullptr;
    const Window* window_;
    Actions& actions_;
    PointerReleaseCallbacks* previous_;
};

} // namespace internal
} // namespace oneui
