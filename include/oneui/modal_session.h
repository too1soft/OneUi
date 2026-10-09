#pragma once
#include "oneui/platform/window.h"

namespace oneui {
// Caller-owned windows, no nested event loop. Destroying the session releases
// modality and the temporary native owner; the child remains caller-owned.
// The last modal restores its parent's original widget focus when still valid.
// A child cannot join two simultaneous sessions. A result of zero denotes dismissal.
class ONEUI_API ModalSession final {
  public:
    static std::unique_ptr<ModalSession> show(Window &child, Window &parent,
                                              std::function<void(int)> completed);
    ~ModalSession();
    ModalSession(const ModalSession &) = delete;
    ModalSession &operator=(const ModalSession &) = delete;
    void finish(int result);
    bool active() const;

  private:
    struct Impl;
    explicit ModalSession(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> impl_;
};
} // namespace oneui
