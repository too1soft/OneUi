#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace oneui {
class Window;
namespace internal {

// Collect only the view refreshes scheduled by this window's size callback.
// Posted application work stays in the normal message queue.
class ResizeViewFlush {
public:
    explicit ResizeViewFlush(Window& window) : window_(window), previous_(current_) {
        current_ = this;
    }
    ResizeViewFlush(const ResizeViewFlush&) = delete;
    ResizeViewFlush& operator=(const ResizeViewFlush&) = delete;
    ~ResizeViewFlush() { leave(); }

    static bool collect(Window& window, std::function<void()>& callback) {
        if (!current_ || &current_->window_ != &window) return false;
        current_->callbacks_.push_back(std::move(callback));
        return true;
    }

    template<class IsAlive>
    std::size_t drain(IsAlive isAlive) {
        leave();
        std::size_t drained = 0;
        for (auto& callback : callbacks_) {
            if (!isAlive()) break;
            callback();
            ++drained;
        }
        return drained;
    }

private:
    void leave() {
        if (current_ == this) current_ = previous_;
    }

    Window& window_;
    ResizeViewFlush* previous_;
    std::vector<std::function<void()>> callbacks_;
    inline static thread_local ResizeViewFlush* current_ = nullptr;
};

} // namespace internal
} // namespace oneui
