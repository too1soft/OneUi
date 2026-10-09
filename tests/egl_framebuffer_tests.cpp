#include "platform/shared/gpu_renderer.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkSurface.h"
#include "include/gpu/GpuTypes.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include <GLES3/gl3.h>
#include <X11/Xlib.h>
#include <iostream>
#include <stdexcept>

namespace {
struct NativeWindow {
    Display* display = XOpenDisplay(nullptr);
    ::Window window = 0;
    NativeWindow() {
        if (!display) throw std::runtime_error("X11 display unavailable");
        window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 64, 64, 0, 0, 0);
        XMapWindow(display, window);
        XSync(display, False);
    }
    ~NativeWindow() {
        if (window) XDestroyWindow(display, window);
        if (display) XCloseDisplay(display);
    }
};
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        NativeWindow native;
        std::string failure;
        auto renderer = oneui::platform::makeEglRenderer(native.display,
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(native.window)), false, failure);
        if (!renderer) throw std::runtime_error(failure);
        auto* canvas = renderer->beginFrame({64, 64}, 1);
        require(canvas != nullptr, "Initial EGL frame unavailable");
        auto* context = GrAsDirectContext(canvas->recordingContext());
        require(context != nullptr, "Ganesh context unavailable");
        canvas->clear(SK_ColorRED);
        context->flushAndSubmit(GrSyncCpu::kYes);

        // Filters and saved layers render into scratch targets. Keep one alive
        // over the swap so its FBO remains valid and cannot be reused as zero.
        auto scratch = SkSurfaces::RenderTarget(context, skgpu::Budgeted::kYes,
            SkImageInfo::MakeN32Premul(64, 64));
        require(scratch != nullptr, "Scratch render target unavailable");
        scratch->getCanvas()->clear(SK_ColorGREEN);
        context->flushAndSubmit(scratch.get(), GrSyncCpu::kYes);
        GLint framebuffer = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
        require(framebuffer != 0, "Test did not establish an offscreen framebuffer");
        require(renderer->present(), "Initial EGL presentation failed");

        canvas = renderer->beginFrame({64, 64}, 1);
        require(canvas != nullptr, "Second EGL frame unavailable");
        canvas->clear(SK_ColorBLUE);
        context->flushAndSubmit(GrSyncCpu::kYes);
        // Read the actual native back buffer, not Skia's current scratch target.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        unsigned char pixel[4]{};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        require(glGetError() == GL_NO_ERROR, "Native back-buffer read failed");
        require(pixel[0] < 8 && pixel[1] < 8 && pixel[2] > 247,
            "Second frame rendered into the previous offscreen target instead of the native back buffer");
        scratch.reset();
        require(renderer->present(), "Second EGL presentation failed");
        std::cout << "EGL native back buffer survived a retained offscreen target: " << renderer->device() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
