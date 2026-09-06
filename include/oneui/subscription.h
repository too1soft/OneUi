#pragma once
#include <functional>
#include <utility>

namespace oneui {
// UI-thread scoped connection. Moving transfers cancellation responsibility.
class Subscription {
public:
    Subscription() = default;
    explicit Subscription(std::function<void()> cancel) : cancel_(std::move(cancel)) {}
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Subscription(Subscription&& other) noexcept : cancel_(std::exchange(other.cancel_, {})) {}
    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) { reset(); cancel_ = std::exchange(other.cancel_, {}); }
        return *this;
    }
    ~Subscription() { reset(); }
    void reset() noexcept { if (auto cancel = std::exchange(cancel_, {})) cancel(); }
    explicit operator bool() const noexcept { return static_cast<bool>(cancel_); }
private:
    std::function<void()> cancel_;
};
} // namespace oneui
