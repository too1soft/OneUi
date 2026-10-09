#pragma once
#include "notification_runtime_win32.h"
#include "platform/shared/notification_backend.h"

namespace oneui::win32 {
class NotificationActivation final {
  public:
    NotificationActivation(const ApplicationOptions &, platform::NotificationBackend::Activation);
    ~NotificationActivation();
    bool primary() const { return primary_; }
    bool registered() const { return cookie_ != 0; }

  private:
    static LRESULT CALLBACK dispatch(HWND, UINT, WPARAM, LPARAM);
    std::wstring windowClass_;
    std::string id_;
    platform::NotificationBackend::Activation callback_;
    DWORD cookie_ = 0;
    HWND window_ = nullptr;
    HANDLE mutex_ = nullptr;
    bool primary_ = true, comInitialized_ = false, ownsMutex_ = false;
};
} // namespace oneui::win32
