#pragma once

#include "oneui/subscription.h"
#include <algorithm>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace oneui {
template <typename T> class Binding;

// State and its subscriptions are UI-thread-only. Worker updates use post.
template <typename T>
class State {
public:
    using Listener = std::function<void(const T&)>;
    using ListenerId = std::size_t;
    State() : data_(std::make_shared<Data>(T{})) {}
    explicit State(T value) : data_(std::make_shared<Data>(std::move(value))) {}
    // Copies copy values, not callbacks. Moves transfer observable identity.
    State(const State& other) : State(other.get()) {}
    State& operator=(const State& other) { if (this != &other) set(other.get()); return *this; }
    State(State&& other) noexcept : data_(std::move(other.data_)) {}
    State& operator=(State&& other) noexcept {
        if (this != &other) { invalidate(); data_ = std::move(other.data_); }
        return *this;
    }
    ~State() { invalidate(); }
    const T& get() const { return data()->value; }
    void set(T value) { setData(data(), std::move(value)); }
    void update(std::function<T(const T&)> updater) {
        auto current = data();
        const T snapshot = current->value;
        setData(std::move(current), updater(snapshot));
    }
    ListenerId subscribe(Listener listener) {
        if (!listener) return 0;
        auto current = data();
        const auto id = current->nextId++;
        current->listeners.push_back(std::make_shared<Slot>(Slot{id, std::move(listener), true}));
        return id;
    }
    void unsubscribe(ListenerId id) { unsubscribeData(data(), id); }
    Subscription subscribeScoped(Listener listener) {
        const auto id = subscribe(std::move(listener));
        if (!id) return {};
        return Subscription([weak = std::weak_ptr<Data>(data()), id] {
            if (auto current = weak.lock()) unsubscribeData(current, id);
        });
    }
private:
    friend class Binding<T>;
    struct Slot { ListenerId id; Listener listener; bool active; };
    struct Notification { T value; std::vector<std::shared_ptr<Slot>> listeners; };
    struct Data {
        explicit Data(T initial) : value(std::move(initial)) {}
        T value;
        ListenerId nextId = 1;
        bool alive = true;
        bool notifying = false;
        std::vector<std::shared_ptr<Slot>> listeners;
        std::deque<Notification> notifications;
    };
    std::shared_ptr<Data> data() const {
        if (!data_) data_ = std::make_shared<Data>(T{});
        return data_;
    }
    void invalidate() noexcept {
        if (!data_) return;
        data_->alive = false;
        for (auto& slot : data_->listeners) slot->active = false;
        data_->listeners.clear();
        data_->notifications.clear();
    }
    static void unsubscribeData(const std::shared_ptr<Data>& current, ListenerId id) {
        for (auto& slot : current->listeners) if (slot->id == id) slot->active = false;
        current->listeners.erase(std::remove_if(current->listeners.begin(), current->listeners.end(),
            [id](const auto& slot) { return slot->id == id; }), current->listeners.end());
    }
    static void setData(std::shared_ptr<Data> current, T value) {
        if (!current->alive || current->value == value) return;
        current->value = std::move(value);
        current->notifications.push_back({current->value, current->listeners});
        if (current->notifying) return;
        current->notifying = true;
        struct Guard { Data& data; ~Guard() { data.notifying = false; data.notifications.clear(); } } guard{*current};
        while (current->alive && !current->notifications.empty()) {
            auto notification = std::move(current->notifications.front());
            current->notifications.pop_front();
            for (const auto& slot : notification.listeners) {
                if (!current->alive) break;
                if (slot->active) slot->listener(notification.value);
            }
        }
    }
    mutable std::shared_ptr<Data> data_;
};

template <typename T>
class Binding {
public:
    Binding() = default;
    explicit Binding(State<T>& state) { bind(state); }
    Binding(State<T>& state, std::function<void()> changed) { bind(state, std::move(changed)); }
    Binding(const Binding&) = delete;
    Binding& operator=(const Binding&) = delete;
    Binding(Binding&&) noexcept = default;
    Binding& operator=(Binding&&) noexcept = default;
    ~Binding() = default;
    void bind(State<T>& state, std::function<void()> changed = {}) {
        reset();
        data_ = state.data();
        if (changed) subscription_ = state.subscribeScoped([callback = std::move(changed)](const T&) { callback(); });
    }
    bool bound() const { auto current = data_.lock(); return current && current->alive; }
    const T& get(const T& fallback) const {
        auto current = data_.lock();
        return current && current->alive ? current->value : fallback;
    }
    void set(T value, T& fallback) {
        auto current = data_.lock();
        if (current && current->alive) State<T>::setData(std::move(current), std::move(value));
        else fallback = std::move(value);
    }
    void reset() noexcept { subscription_.reset(); data_.reset(); }
private:
    std::weak_ptr<typename State<T>::Data> data_;
    Subscription subscription_;
};
} // namespace oneui
