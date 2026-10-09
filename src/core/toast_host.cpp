#include "oneui/controls/toast_host.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oneui {
ToastHost::ToastHost(std::size_t capacity) : capacity_(capacity) {
    if (!capacity || capacity > 4096)
        throw std::invalid_argument("Invalid ToastHost capacity");
}
bool ToastHost::push(ToastMessage message) {
    if (message.id.empty() || !std::isfinite(message.timeoutMs) || message.timeoutMs < 0)
        return false;
    auto existing = std::find_if(messages_.begin(), messages_.end(),
                                 [&](auto &item) { return item.id == message.id; });
    if (existing != messages_.end()) {
        const bool current = existing == messages_.begin();
        *existing = std::move(message);
        if (current)
            present();
        return true;
    }
    if (messages_.size() == capacity_)
        return false;
    messages_.push_back(std::move(message));
    if (messages_.size() == 1)
        present();
    return true;
}
bool ToastHost::dismiss(const std::string &id) {
    auto existing =
        std::find_if(messages_.begin(), messages_.end(), [&](auto &item) { return item.id == id; });
    if (existing == messages_.end())
        return false;
    const bool current = existing == messages_.begin();
    messages_.erase(existing);
    if (current)
        present();
    return true;
}
void ToastHost::clear() {
    messages_.clear();
    present();
}
std::size_t ToastHost::pendingCount() const { return messages_.size(); }
void ToastHost::setActive(bool active) {
    active_ = active;
    previousMs_ = -1;
    if (active && remainingMs_ > 0)
        requestAnimationFrame();
}
void ToastHost::present() {
    clearChildren();
    toast_.reset();
    previousMs_ = -1;
    remainingMs_ = messages_.empty() ? 0 : messages_.front().timeoutMs;
    if (!messages_.empty()) {
        const auto &message = messages_.front();
        toast_ = std::make_shared<Toast>(message.title, message.message);
        toast_->setPrimaryAction(message.primaryAction);
        toast_->setSecondaryAction(message.secondaryAction);
        const auto id = message.id;
        const auto weak = lifetimeToken();
        toast_->setOnPrimaryAction([this, weak, id] {
            if (!weak.expired())
                invoke(id, "primary");
        });
        toast_->setOnSecondaryAction([this, weak, id] {
            if (!weak.expired())
                invoke(id, "secondary");
        });
        toast_->setOnClose([this, weak, id] {
            if (!weak.expired())
                invoke(id, "close");
        });
        add(toast_);
    }
    invalidate();
    if (active_ && remainingMs_ > 0)
        requestAnimationFrame();
}
void ToastHost::invoke(const std::string &id, const std::string &action) {
    if (messages_.empty() || messages_.front().id != id)
        return;
    auto callback = messages_.front().onAction;
    dismiss(id);
    if (callback)
        callback(action);
}
bool ToastHost::tickAnimations(double nowMs) {
    if (!active_ || remainingMs_ <= 0 || messages_.empty())
        return false;
    if (previousMs_ >= 0)
        remainingMs_ -= std::max(0.0, nowMs - previousMs_);
    previousMs_ = nowMs;
    if (remainingMs_ <= 0) {
        const auto id = messages_.front().id;
        dismiss(id);
    }
    return active_ && remainingMs_ > 0;
}
void ToastHost::layoutChildren() {
    if (toast_)
        toast_->setFrame(frame());
}
} // namespace oneui
