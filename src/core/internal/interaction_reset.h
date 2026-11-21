#pragma once

namespace oneui {
class Widget;

namespace internal {
// External widget subclasses keep the conservative layout invalidation.
// Built-in controls that know their measurement contract can opt in for
// this reset only. Clearing a child cannot change its parent's decision.
class InteractionReset {
public:
    explicit InteractionReset(const Widget& widget) : widget_(widget), previous_(active_) {
        active_ = this;
    }
    ~InteractionReset() { active_ = previous_; }
    InteractionReset(const InteractionReset&) = delete;
    InteractionReset& operator=(const InteractionReset&) = delete;

    static void markPaintOnly(const Widget& widget) {
        if (active_ && &active_->widget_ == &widget)
            active_->paintOnly_ = true;
    }
    bool paintOnly() const { return paintOnly_; }

private:
    inline static thread_local InteractionReset* active_ = nullptr;
    const Widget& widget_;
    InteractionReset* previous_;
    bool paintOnly_ = false;
};
} // namespace internal
} // namespace oneui
