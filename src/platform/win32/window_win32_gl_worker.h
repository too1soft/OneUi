#pragma once

#include "internal/interaction_trace.h"
#include "include/core/SkPicture.h"

#include <windows.h>
#include <memory>

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
    bool ready() const noexcept;
    bool vsync() const noexcept;
    Frame plan(int width, int height, RECT damage);
    bool enqueue(sk_sp<SkPicture> picture, const Frame& frame,
                 const internal::InteractionSnapshot& trace);
    void invalidateExtent();
    void stop();

private:
    struct State;

    std::shared_ptr<State> state_;
};

bool experimentalGlWorkerEnabled() noexcept;
bool experimentalGlWorkerAfterFirstFrame() noexcept;

} // namespace oneui::win32
