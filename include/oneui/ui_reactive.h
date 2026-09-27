#pragma once
#include "oneui/reactive.h"
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace oneui::ui {
namespace detail {
struct BatchQueue {
    unsigned depth = 0;
    bool draining = false;
    std::map<const void*, std::function<void()>> pending;
};
inline thread_local BatchQueue batchQueue;
inline void defer(const void* key, std::function<void()> job) {
    auto& q = batchQueue;
    if (!q.depth && !q.draining) { job(); return; }
    q.pending[key] = std::move(job);
}
inline void drainBatch() {
    auto& q = batchQueue;
    if (q.draining || q.depth) return;
    q.draining = true;
    try {
        while (!q.pending.empty()) {
            auto jobs = std::move(q.pending); q.pending.clear();
            for (auto& job : jobs) job.second();
        }
    } catch (...) { q.pending.clear(); q.draining = false; throw; }
    q.draining = false;
}
}
// Batches Computed and mounted property work, never changes legacy State listeners.
class Batch {
public:
    Batch() { ++detail::batchQueue.depth; }
    Batch(const Batch&) = delete;
    ~Batch() noexcept(false) { if (--detail::batchQueue.depth == 0) detail::drainBatch(); }
};

template<class T> class Computed {
    struct Data { State<T> value; std::function<T()> compute; };
    std::shared_ptr<Data> data_;
    std::vector<Subscription> subscriptions_;
public:
    template<class F, class... Sources> Computed(F compute, Sources&... sources)
        : data_(std::make_shared<Data>()) {
        data_->compute = std::move(compute); data_->value.set(data_->compute());
        auto weak = std::weak_ptr<Data>(data_);
        auto change = [weak](const auto&) {
            if (auto data = weak.lock()) detail::defer(data.get(), [weak] {
                if (auto current = weak.lock()) current->value.set(current->compute());
            });
        };
        (subscriptions_.push_back(sources.subscribeScoped(change)), ...);
    }
    Computed(const Computed&) = delete;
    const T& get() const { return data_->value.get(); }
    Subscription subscribeScoped(typename State<T>::Listener fn) { return data_->value.subscribeScoped(std::move(fn)); }
};

// The host's UI-thread wake callback is installed before work starts and revoked
// before the Window dies. Workers never own or dereference a Window pointer.
class UiMailbox {
    struct Data {
        std::mutex mutex;
        bool alive = true;
        std::function<void()> wake;
        std::vector<std::function<void()>> pending;
    };
    std::shared_ptr<Data> data_ = std::make_shared<Data>();
public:
    class Sender {
        std::weak_ptr<Data> data_;
        friend class UiMailbox;
        explicit Sender(std::weak_ptr<Data> data) : data_(std::move(data)) {}
    public:
        bool cancelled() const {
            auto d = data_.lock(); if (!d) return true;
            std::lock_guard<std::mutex> lock(d->mutex); return !d->alive;
        }
        bool post(std::function<void()> job) const {
            auto d = data_.lock(); if (!d) return false;
            std::lock_guard<std::mutex> lock(d->mutex);
            if (!d->alive) return false;
            d->pending.push_back(std::move(job));
            if (d->wake) d->wake();
            return true;
        }
    };
    UiMailbox() = default;
    UiMailbox(const UiMailbox&) = delete;
    ~UiMailbox() { close(); }
    Sender sender() const { return Sender(data_); }
    void setWake(std::function<void()> wake) {
        std::lock_guard<std::mutex> lock(data_->mutex); data_->wake = std::move(wake);
    }
    void drain() {
        auto data=data_;
        std::vector<std::function<void()>> pending;
        { std::lock_guard<std::mutex> lock(data->mutex); if (!data->alive) return; pending.swap(data->pending); }
        for (auto& job : pending) {
            {std::lock_guard<std::mutex> lock(data->mutex);if(!data->alive)break;}
            job();
        }
    }
    void close() {
        std::lock_guard<std::mutex> lock(data_->mutex);
        data_->alive = false; data_->wake = {}; data_->pending.clear();
    }
};

class VmCommand {
    struct Data { bool alive=true; State<bool> enabled{true}, running{false}; State<std::wstring> error; };
    std::shared_ptr<Data> data_ = std::make_shared<Data>();
    std::function<void()> action_;
public:
    State<bool>& canExecute = data_->enabled;
    State<bool>& running = data_->running;
    State<std::wstring>& error = data_->error;
    VmCommand() = default;
    ~VmCommand() {data_->alive=false;}
    VmCommand(const VmCommand&) = delete;
    void setAction(std::function<void()> action) { action_ = std::move(action); }
    void execute() { auto action=action_; if (canExecute.get() && !running.get() && action) action(); }
    // work is self-contained and returns an error string (empty means success).
    // finish runs only on the UI thread, while both command and mount are alive.
    template<class Work> void runAsync(UiMailbox::Sender sender, Work work, std::function<void(bool)> finish = {}) {
        if (!canExecute.get() || running.get() || sender.cancelled()) return;
        auto data=data_;auto weak = std::weak_ptr<Data>(data);
        data->running.set(true);if(!data->alive)return;data->error.set({});if(!data->alive)return;
        std::thread([weak, sender, work = std::move(work), finish = std::move(finish)]() mutable {
            std::wstring message;
            try { message = work(sender); }
            catch (const std::exception& e) { std::string s = e.what(); message.assign(s.begin(), s.end()); }
            catch (...) { message = L"Unexpected task failure"; }
            sender.post([weak, message = std::move(message), finish = std::move(finish)] {
                if (auto d = weak.lock()) {
                    if(!d->alive)return;
                    Batch batch; d->running.set(false);if(!d->alive)return;d->error.set(message);
                    if (d->alive && finish) finish(message.empty());
                }
            });
        }).detach();
    }
};
} // namespace oneui::ui
