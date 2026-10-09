#pragma once
#include "oneui/controls/toast.h"
#include "oneui/view.h"
#include <deque>

namespace oneui {
struct ToastMessage {
    std::string id;
    std::wstring title, message, primaryAction, secondaryAction;
    // Zero keeps the message until explicitly dismissed. Time pauses while inactive.
    double timeoutMs = 5000;
    std::function<void(const std::string &action)> onAction;
};
// Application notifications are distinct from OS notifications. This host owns
// a bounded FIFO; posting an existing ID updates it without changing its place.
class ONEUI_API ToastHost final : public View {
  public:
    explicit ToastHost(std::size_t capacity = 32);
    bool push(ToastMessage message);
    bool dismiss(const std::string &id);
    void clear();
    std::size_t pendingCount() const;
    void setActive(bool active);
    bool active() const { return active_; }
    bool tickAnimations(double nowMs) override;

  private:
    void present();
    void invoke(const std::string &id, const std::string &action);
    void layoutChildren() override;
    std::deque<ToastMessage> messages_;
    std::shared_ptr<Toast> toast_;
    std::size_t capacity_;
    bool active_ = true;
    double previousMs_ = -1, remainingMs_ = 0;
};
} // namespace oneui
