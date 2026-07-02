#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace oneui {

template <typename T>
class State {
public:
    using Listener = std::function<void(const T&)>;
    using ListenerId = std::size_t;

    State() = default;
    explicit State(T value) : value_(std::move(value)) {}

    const T& get() const {
        return value_;
    }

    void set(T value) {
        if (value_ == value) {
            return;
        }

        value_ = std::move(value);
        const auto listeners = listeners_;
        for (const auto& slot : listeners) {
            slot.listener(value_);
        }
    }

    void update(std::function<T(const T&)> updater) {
        set(updater(value_));
    }

    ListenerId subscribe(Listener listener) {
        const ListenerId id = nextListenerId_++;
        listeners_.push_back(ListenerSlot{id, std::move(listener)});
        return id;
    }

    void unsubscribe(ListenerId id) {
        for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
            if (it->id == id) {
                listeners_.erase(it);
                return;
            }
        }
    }

private:
    struct ListenerSlot {
        ListenerId id;
        Listener listener;
    };

    T value_{};
    ListenerId nextListenerId_ = 1;
    std::vector<ListenerSlot> listeners_;
};

template <typename T>
class Binding {
public:
    Binding() = default;
    explicit Binding(State<T>& state) : state_(&state) {}
    Binding(State<T>& state, std::function<void()> onChanged) {
        bind(state, std::move(onChanged));
    }

    Binding(const Binding&) = delete;
    Binding& operator=(const Binding&) = delete;

    Binding(Binding&& other) noexcept {
        moveFrom(other);
    }

    Binding& operator=(Binding&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    ~Binding() {
        reset();
    }

    void bind(State<T>& state, std::function<void()> onChanged = {}) {
        reset();
        state_ = &state;
        if (onChanged) {
            listenerId_ = state_->subscribe([callback = std::move(onChanged)](const T&) {
                callback();
            });
        }
    }

    bool bound() const {
        return state_ != nullptr;
    }

    const T& get(const T& fallback) const {
        return state_ ? state_->get() : fallback;
    }

    void set(T value, T& fallback) {
        if (state_) {
            state_->set(std::move(value));
        } else {
            fallback = std::move(value);
        }
    }

    void reset() {
        if (state_ && listenerId_ != 0) {
            state_->unsubscribe(listenerId_);
        }
        state_ = nullptr;
        listenerId_ = 0;
    }

private:
    void moveFrom(Binding& other) noexcept {
        state_ = other.state_;
        listenerId_ = other.listenerId_;
        other.state_ = nullptr;
        other.listenerId_ = 0;
    }

    State<T>* state_ = nullptr;
    typename State<T>::ListenerId listenerId_ = 0;
};

} // namespace oneui
