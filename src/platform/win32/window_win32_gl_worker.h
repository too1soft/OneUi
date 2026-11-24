#pragma once

#include "internal/interaction_trace.h"
#include "include/core/SkPicture.h"

#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace oneui::win32 {

// Experimental WGL renderer. The render thread owns the DC, context and GPU
// surfaces. The window thread produces immutable recordings in client pixels.
class GlRenderWorker {
public:
    struct Frame {
        int width = 0;
        int height = 0;
        RECT damage{};
        bool retained = false;
        std::uint64_t extent = 0;
        std::uint64_t serial = 0;
    };

    ~GlRenderWorker();
    bool start(HWND window);
    bool ready() const noexcept { return ready_.load(std::memory_order_acquire); }
    bool vsync() const noexcept { return vsync_.load(std::memory_order_relaxed); }
    Frame plan(int width, int height, RECT damage);
    bool enqueue(sk_sp<SkPicture> picture, const Frame& frame,
                 const internal::InteractionSnapshot& trace);
    void invalidateExtent();
    void stop();

private:
    struct Scene {
        sk_sp<SkPicture> picture;
        Frame frame;
        internal::InteractionSnapshot trace;
    };

    void run(HWND window);

    std::atomic_bool ready_{false};
    std::atomic_bool vsync_{false};
    std::atomic_uint64_t extent_{0};
    std::mutex mutex_;
    std::condition_variable changed_;
    Scene pending_;
    Frame lastQueued_;
    Frame lastPlanned_;
    std::uint64_t latestPlan_ = 0;
    bool stopping_ = false;
    std::thread thread_;
};

bool experimentalGlWorkerEnabled() noexcept;
bool experimentalGlWorkerAfterFirstFrame() noexcept;

} // namespace oneui::win32
