#pragma once

#include "internal/interaction_trace.h"
#include "include/core/SkPicture.h"

#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace oneui::win32 {

// Experimental WGL renderer. The window thread records complete scenes, and
// the render thread owns the DC, context, and GPU resources for their lifetime.
class GlRenderWorker {
public:
    ~GlRenderWorker();
    bool start(HWND window);
    bool ready() const noexcept { return ready_.load(std::memory_order_acquire); }
    bool vsync() const noexcept { return vsync_.load(std::memory_order_relaxed); }
    bool enqueue(sk_sp<SkPicture> picture, int width, int height,
                 const internal::InteractionSnapshot& trace);
    void stop();

private:
    struct Scene {
        sk_sp<SkPicture> picture;
        int width = 0;
        int height = 0;
        internal::InteractionSnapshot trace;
    };

    void run(HWND window);

    std::atomic_bool ready_{false};
    std::atomic_bool vsync_{false};
    std::mutex mutex_;
    std::condition_variable changed_;
    Scene pending_;
    bool stopping_ = false;
    std::thread thread_;
};

bool experimentalGlWorkerEnabled() noexcept;

} // namespace oneui::win32
