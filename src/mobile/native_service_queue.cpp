#include "oneui/mobile/native_service_queue.h"
#include <limits>
#include <algorithm>

namespace oneui::mobile {
namespace {
void wipe(std::string &text) {
    volatile char *bytes = text.empty() ? nullptr : &text[0];
    for (size_t i = 0; i < text.size(); ++i) bytes[i] = 0;
    text.clear();
}
}
NativeServiceQueue::Pending::~Pending() { wipe(input); wipe(output); }
NativeServiceQueue::~NativeServiceQueue() {
    close();
    std::unique_lock<std::mutex> lock(mutex_);
    drained_.wait(lock, [&] { return pending_.empty(); });
}
NativeServiceReply NativeServiceQueue::invoke(int operation, std::string payload, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (closed_) { wipe(payload); return {-1, {}}; }
    if (operation < 1 || payload.size() > 1024 * 1024 + 4096 || timeout.count() < 1 || timeout.count() > 30000 ||
        pending_.size() >= 16 || bytes_ + payload.size() > 4 * 1024 * 1024 || next_ == std::numeric_limits<uint64_t>::max()) {
        wipe(payload); return {-2, {}};
    }
    auto pending = std::make_shared<Pending>();
    pending->operation = operation; pending->input = std::move(payload);
    const auto id = ++next_;
    pending->charged = pending->input.size();
    bytes_ += pending->charged; pending_.emplace(id, pending); ready_.push_back(id);
    const bool completed = pending->changed.wait_for(lock, timeout, [&] { return closed_ || pending->done; });
    pending_.erase(id); bytes_ -= pending->charged;
    ready_.erase(std::remove(ready_.begin(), ready_.end(), id), ready_.end());
    if (pending_.empty()) drained_.notify_all();
    if (closed_) return {-1, {}};
    if (!completed) return {-3, {}};
    return {pending->code, std::move(pending->output)};
}
bool NativeServiceQueue::take(NativeServiceRequest &request) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!closed_ && !ready_.empty()) {
        const auto id = ready_.front(); ready_.pop_front();
        const auto found = pending_.find(id);
        if (found == pending_.end()) continue;
        auto &pending = *found->second;
        pending.delivered = true;
        request = {id, pending.operation, std::move(pending.input)};
        return true;
    }
    return false;
}
bool NativeServiceQueue::complete(uint64_t id, int code, std::string payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = pending_.find(id);
    if (closed_ || payload.size() > 1024 * 1024 + 4096 || bytes_ + payload.size() > 4 * 1024 * 1024 ||
        found == pending_.end() || !found->second->delivered || found->second->done) {
        wipe(payload); return false;
    }
    auto &pending = *found->second;
    bytes_ += payload.size(); pending.charged += payload.size();
    pending.output = std::move(payload); pending.code = code; pending.done = true;
    pending.changed.notify_one(); return true;
}
void NativeServiceQueue::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true; ready_.clear();
    for (auto &item : pending_) { wipe(item.second->input); wipe(item.second->output); item.second->changed.notify_one(); }
}
}
