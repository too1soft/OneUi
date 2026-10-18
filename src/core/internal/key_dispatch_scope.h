#pragma once

namespace oneui {
class Widget;
namespace internal {
// Only the synchronous dispatch chain is tracked. No widget survives the call,
// and exceptions or nested dispatch restore the previous chain automatically.
class KeyDispatchScope {
public:
    explicit KeyDispatchScope(const Widget* widget) : widget_(widget), previous_(current_) {
        current_ = this;
    }
    ~KeyDispatchScope() { current_ = previous_; }
    KeyDispatchScope(const KeyDispatchScope&) = delete;
    KeyDispatchScope& operator=(const KeyDispatchScope&) = delete;
    bool hasParent() const {
        for (auto entry = previous_; entry; entry = entry->previous_)
            if (entry->widget_ != widget_) return true;
        return false;
    }
private:
    const Widget* widget_;
    const KeyDispatchScope* previous_;
    inline static thread_local const KeyDispatchScope* current_ = nullptr;
};
} // namespace internal
} // namespace oneui
