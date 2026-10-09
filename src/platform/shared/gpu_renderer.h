#pragma once
#include "oneui/platform/window.h"
#include <memory>

class SkCanvas;
namespace oneui::platform {
// A frame is painted directly into the native GPU back buffer. The caller
// redraws the complete scene because swapchain contents are not retained.
class GpuRenderer {
public:
    virtual ~GpuRenderer() = default;
    virtual SkCanvas* beginFrame(Size pixels, float scale) = 0;
    virtual bool present() = 0;
    virtual RenderBackend backend() const = 0;
    virtual std::string device() const = 0;
    virtual bool vsync() const = 0;
    virtual RendererMemoryInfo memoryInfo() const = 0;
};
#ifdef ONEUI_ENABLE_GPU
#ifdef __APPLE__
std::unique_ptr<GpuRenderer> makeMetalRenderer(void* view, std::string& failure);
#else
std::unique_ptr<GpuRenderer> makeEglRenderer(void* display, void* window, bool wayland, std::string& failure);
#endif
#endif
} // namespace oneui::platform
