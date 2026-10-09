#include "skia_surface_android.h"
#include "platform/shared/skia_canvas.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <cmath>
#include <mutex>

namespace oneui::mobile {
namespace {
struct Display {
    EGLDisplay value = EGL_NO_DISPLAY;
    ~Display() { if (value != EGL_NO_DISPLAY) eglTerminate(value); }
};
std::shared_ptr<Display> acquireDisplay() {
    static std::mutex mutex;
    static std::weak_ptr<Display> live;
    std::lock_guard<std::mutex> lock(mutex);
    if (auto owner = live.lock()) return owner;
    const auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) return {};
    auto owner = std::make_shared<Display>();
    owner->value = display;
    live = owner;
    return owner;
}
struct CurrentContext {
    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext context = eglGetCurrentContext();
    EGLSurface draw = eglGetCurrentSurface(EGL_DRAW), read = eglGetCurrentSurface(EGL_READ);
    EGLenum api = eglQueryAPI();
    void restore(EGLDisplay fallback) const {
        eglMakeCurrent(display == EGL_NO_DISPLAY ? fallback : display, draw, read, context);
        eglBindAPI(api);
    }
};
} // namespace
struct SkiaSurface::State {
    ANativeWindow *window = nullptr;
    std::shared_ptr<Display> display;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    sk_sp<GrDirectContext> ganesh;
    sk_sp<SkSurface> frame;
    std::unique_ptr<CurrentContext> previous;
    int width = 0, height = 0, stencil = 0;
    std::string renderer = "unavailable", error;
    bool makeCurrent() const {
        return display && eglBindAPI(EGL_OPENGL_ES_API) &&
               eglMakeCurrent(display->value, surface, surface, context);
    }
    void restore() {
        if (previous && display) previous->restore(display->value);
        previous.reset();
    }
};
SkiaSurface::SkiaSurface() : state_(std::make_unique<State>()) {}
SkiaSurface::~SkiaSurface() { reset(); }
void SkiaSurface::reset() {
    auto &s = *state_;
    s.restore();
    if (s.display) {
        const CurrentContext previous;
        if (s.context != EGL_NO_CONTEXT && s.makeCurrent()) {
            s.frame.reset();
            s.ganesh.reset();
        } else {
            if (s.ganesh) s.ganesh->abandonContext();
            s.frame.reset();
            s.ganesh.reset();
        }
        previous.restore(s.display->value);
        if (s.surface != EGL_NO_SURFACE) eglDestroySurface(s.display->value, s.surface);
        if (s.context != EGL_NO_CONTEXT) eglDestroyContext(s.display->value, s.context);
    }
    s.surface = EGL_NO_SURFACE;
    s.context = EGL_NO_CONTEXT;
    s.display.reset();
    if (s.window) ANativeWindow_release(s.window);
    s.window = nullptr;
    s.renderer = "unavailable";
}
bool SkiaSurface::attach(ANativeWindow *window, int width, int height) {
    reset();
    auto &s = *state_;
    s.window = window; // Takes the ANativeWindow_fromSurface reference on all paths.
    s.error.clear();
    auto fail = [&](const char *message) { s.error = message; reset(); return false; };
    if (!window || width <= 0 || height <= 0) return fail("Invalid native surface dimensions");
    s.display = acquireDisplay();
    if (!s.display) return fail("EGL display initialization failed");
    s.previous = std::make_unique<CurrentContext>();
    const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_NONE};
    EGLConfig config{};
    EGLint count = 0, visual = 0;
    if (!eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(s.display->value, attributes, &config, 1, &count) || count != 1 ||
        !eglGetConfigAttrib(s.display->value, config, EGL_NATIVE_VISUAL_ID, &visual) ||
        !eglGetConfigAttrib(s.display->value, config, EGL_STENCIL_SIZE, &s.stencil) ||
        ANativeWindow_setBuffersGeometry(window, 0, 0, visual) != 0)
        return fail("EGL ES3 window configuration unavailable");
    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    s.context = eglCreateContext(s.display->value, config, EGL_NO_CONTEXT, contextAttributes);
    s.surface = eglCreateWindowSurface(s.display->value, config, window, nullptr);
    if (s.context == EGL_NO_CONTEXT || s.surface == EGL_NO_SURFACE || !s.makeCurrent())
        return fail("EGL window/context creation failed");
    if (!eglSwapInterval(s.display->value, 1)) return fail("EGL swap interval rejected");
    auto interface = GrGLMakeAssembledGLESInterface(nullptr, [](void *, const char *name) {
        return reinterpret_cast<GrGLFuncPtr>(eglGetProcAddress(name));
    });
    s.ganesh = GrDirectContexts::MakeGL(interface);
    if (!s.ganesh) return fail("Skia Ganesh GLES initialization failed");
    const auto *name = glGetString(GL_RENDERER);
    s.renderer = name ? reinterpret_cast<const char *>(name) : "unknown";
    s.width = width; s.height = height;
    s.restore();
    return true;
}
std::unique_ptr<Canvas> SkiaSurface::begin(float scale) {
    auto &s = *state_;
    if (!s.ganesh || s.previous || !std::isfinite(scale) || scale <= 0) return {};
    s.previous = std::make_unique<CurrentContext>();
    if (!s.makeCurrent()) { s.error = "EGL context lost"; s.restore(); return {}; }
    // The window framebuffer is always zero, irrespective of Skia's last FBO.
    const GrGLFramebufferInfo info{0, GL_RGBA8};
    auto target = GrBackendRenderTargets::MakeGL(s.width, s.height, 0, s.stencil, info);
    s.frame = SkSurfaces::WrapBackendRenderTarget(s.ganesh.get(), target,
        kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, nullptr, nullptr);
    if (!s.frame) { s.error = "Skia window framebuffer unavailable"; s.restore(); return {}; }
    auto &canvas = *s.frame->getCanvas();
    canvas.clear(SK_ColorWHITE);
    canvas.scale(scale, scale);
    return rendering::makeSkiaCanvas(canvas, nullptr,
        Rect{0, 0, s.width / scale, s.height / scale});
}
bool SkiaSurface::present() {
    auto &s = *state_;
    if (!s.frame || !s.previous) return false;
    s.ganesh->flushAndSubmit();
    s.frame.reset();
    const bool success = eglSwapBuffers(s.display->value, s.surface) == EGL_TRUE;
    if (!success) s.error = "EGL presentation failed; surface recreation required";
    s.restore();
    return success;
}
const std::string &SkiaSurface::renderer() const { return state_->renderer; }
const std::string &SkiaSurface::error() const { return state_->error; }
} // namespace oneui::mobile
