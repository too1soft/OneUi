#pragma once

#include <windows.h>

#include "include/core/SkPicture.h"
#include "include/gpu/ganesh/GrDirectContext.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace oneui::win32 {

inline constexpr UINT deferredGlReadyMessage = WM_APP + 5;

struct PreparedStartupGl {
    ~PreparedStartupGl();
    PreparedStartupGl() = default;
    PreparedStartupGl(const PreparedStartupGl&) = delete;
    PreparedStartupGl& operator=(const PreparedStartupGl&) = delete;

    HMONITOR monitor = nullptr;
    int format = 0;
    PIXELFORMATDESCRIPTOR descriptor{};
    HGLRC context = nullptr;
    sk_sp<GrDirectContext> ganesh;
};

// The background thread owns its scratch HWND and DC. The live window never
// draws into them. After unbinding, the worker publishes only the rendering
// context and notifies the live HWND while its notification target is valid.
class DeferredStartupGl {
public:
    ~DeferredStartupGl();
    bool arm(HWND hwnd);
    bool pending() const { return pending_; }
    bool shouldCapture() const { return pending_ && capture_.load(std::memory_order_relaxed); }
    void submitPicture(sk_sp<SkPicture> picture, int width, int height);
    std::unique_ptr<PreparedStartupGl> take();
    void cancel();

private:
    struct Scene {
        sk_sp<SkPicture> picture;
        int width = 0;
        int height = 0;
    };

    std::unique_ptr<PreparedStartupGl> prepare(POINT origin, HMONITOR monitor);
    void finish(std::unique_ptr<PreparedStartupGl> result);

    // pending_, captures_ and worker_ belong to the window thread.
    bool pending_ = false;
    unsigned int captures_ = 0;
    std::atomic_bool capture_{false};
    std::mutex mutex_;
    HWND target_ = nullptr;
    Scene scene_;
    std::unique_ptr<PreparedStartupGl> ready_;
    std::thread worker_;
};

} // namespace oneui::win32
