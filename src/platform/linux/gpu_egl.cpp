#include "platform/shared/gpu_renderer.h"
#include "include/core/SkSurface.h"
#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <X11/Xlib.h>
#include <wayland-egl.h>
#include <map>
#include <stdexcept>
#include <vector>

namespace oneui::platform {
namespace {
struct DisplayOwner {
    EGLDisplay display = EGL_NO_DISPLAY;
    ~DisplayOwner() { if (display != EGL_NO_DISPLAY) eglTerminate(display); }
};
std::shared_ptr<DisplayOwner> acquireDisplay(void* native, bool wayland) {
    // eglTerminate is display-wide; keep it alive until the last window closes.
    static std::map<std::pair<void*, bool>, std::weak_ptr<DisplayOwner>> displays;
    auto& weak = displays[{native, wayland}];
    if (auto shared = weak.lock()) return shared;
    auto result = std::make_shared<DisplayOwner>();
    auto get = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!get) throw std::runtime_error("EGL platform display unavailable");
    result->display = get(wayland ? EGL_PLATFORM_WAYLAND_EXT : EGL_PLATFORM_X11_EXT, native, nullptr);
    if (result->display == EGL_NO_DISPLAY || !eglInitialize(result->display, nullptr, nullptr))
        throw std::runtime_error("EGL display initialization failed");
    weak = result;
    return result;
}
class EglRenderer final : public GpuRenderer {
public:
    ~EglRenderer() override {
        if (!display_) return;
        auto d = display_->display;
        if (context_ != EGL_NO_CONTEXT && surface_ != EGL_NO_SURFACE) {
            if (!current_) saveCurrent();
            if (makeCurrent()) { frame_.reset(); skia_.reset(); }
            else { if (skia_) skia_->abandonContext(); frame_.reset(); skia_.reset(); }
            restoreCurrent();
        }
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(d, surface_);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(d, context_);
        if (waylandWindow_) wl_egl_window_destroy(waylandWindow_);
    }
    void initialize(void* nativeDisplay, void* nativeWindow, bool wayland) {
        display_ = acquireDisplay(nativeDisplay, wayland);
        auto d = display_->display;
        const EGLenum previousApi = eglQueryAPI();
        if (!eglBindAPI(EGL_OPENGL_ES_API)) throw std::runtime_error("EGL OpenGL ES unavailable");
        const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_NONE};
        EGLint count = 0;
        eglChooseConfig(d, attributes, nullptr, 0, &count);
        std::vector<EGLConfig> configs(count);
        eglChooseConfig(d, attributes, configs.data(), count, &count);
        EGLConfig config = nullptr;
        for (int i = 0; i < count; ++i) {
            EGLint visual = 0;
            eglGetConfigAttrib(d, configs[i], EGL_NATIVE_VISUAL_ID, &visual);
            auto* x = static_cast<Display*>(nativeDisplay);
            if (wayland || static_cast<VisualID>(visual) == XVisualIDFromVisual(DefaultVisual(x, DefaultScreen(x)))) {
                config = configs[i]; break;
            }
        }
        if (!config) { eglBindAPI(previousApi); throw std::runtime_error("EGL compatible window configuration unavailable"); }
        EGLNativeWindowType window = reinterpret_cast<EGLNativeWindowType>(nativeWindow);
        if (wayland) {
            waylandWindow_ = wl_egl_window_create(static_cast<wl_surface*>(nativeWindow), 1, 1);
            window = reinterpret_cast<EGLNativeWindowType>(waylandWindow_);
        }
        surface_ = eglCreateWindowSurface(d, config, window, nullptr);
        const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context_ = eglCreateContext(d, config, EGL_NO_CONTEXT, contextAttributes);
        eglBindAPI(previousApi);
        if (surface_ == EGL_NO_SURFACE || context_ == EGL_NO_CONTEXT)
            throw std::runtime_error("EGL window surface or context creation failed");
        saveCurrent();
        if (!makeCurrent()) { restoreCurrent(); throw std::runtime_error("EGL make-current failed"); }
        auto interface = GrGLMakeAssembledGLESInterface(nullptr, [](void*, const char* name) -> GrGLFuncPtr {
            return reinterpret_cast<GrGLFuncPtr>(eglGetProcAddress(name));
        });
        skia_ = GrDirectContexts::MakeGL(std::move(interface));
        const auto* renderer = glGetString(GL_RENDERER);
        device_ = renderer ? reinterpret_cast<const char*>(renderer) : "OpenGL ES";
        synced_ = eglSwapInterval(d, 1) == EGL_TRUE;
        eglGetConfigAttrib(d, config, EGL_STENCIL_SIZE, &stencil_);
        restoreCurrent();
        if (!skia_) throw std::runtime_error("Skia Ganesh OpenGL initialization failed");
    }
    SkCanvas* beginFrame(Size size, float) override {
        if (!skia_ || skia_->abandoned() || size.width <= 0 || size.height <= 0 ||
            size.width > 32768 || size.height > 32768) return nullptr;
        if (waylandWindow_) wl_egl_window_resize(waylandWindow_, size.width, size.height, 0, 0);
        saveCurrent();
        if (!makeCurrent()) { restoreCurrent(); return nullptr; }
        current_ = true;
        // An EGL window presents framebuffer 0. Skia can leave an offscreen
        // framebuffer bound after filters/layers; it is not the next back buffer.
        const GrGLFramebufferInfo info{0, GL_RGBA8};
        auto target = GrBackendRenderTargets::MakeGL(size.width, size.height, 0, stencil_, info);
        frame_ = SkSurfaces::WrapBackendRenderTarget(skia_.get(), target, kBottomLeft_GrSurfaceOrigin,
            kRGBA_8888_SkColorType, nullptr, nullptr);
        if (!frame_) { current_ = false; restoreCurrent(); return nullptr; }
        return frame_->getCanvas();
    }
    bool present() override {
        if (!current_) return false;
        skia_->flushAndSubmit();
        frame_.reset();
        const bool good = !skia_->abandoned() && eglSwapBuffers(display_->display, surface_) == EGL_TRUE;
        current_ = false;
        restoreCurrent();
        return good;
    }
    RenderBackend backend() const override { return RenderBackend::OpenGL; }
    std::string device() const override { return device_; }
    bool vsync() const override { return synced_; }
    RendererMemoryInfo memoryInfo() const override {
        RendererMemoryInfo result;
        if (skia_) {
            int count = 0; size_t bytes = 0;
            skia_->getResourceCacheUsage(&count, &bytes);
            result.gpuCacheAvailable = true; result.gpuResourceCount = count; result.gpuCacheBytes = bytes;
            result.gpuPurgeableBytes = skia_->getResourceCachePurgeableBytes();
            result.gpuCacheLimitBytes = skia_->getResourceCacheLimit();
        }
        return result;
    }
private:
    bool makeCurrent() {
        return eglBindAPI(EGL_OPENGL_ES_API) && eglMakeCurrent(display_->display, surface_, surface_, context_);
    }
    void saveCurrent() {
        previousApi_ = eglQueryAPI();
        previousDisplay_ = eglGetCurrentDisplay(); previousContext_ = eglGetCurrentContext();
        previousDraw_ = eglGetCurrentSurface(EGL_DRAW); previousRead_ = eglGetCurrentSurface(EGL_READ);
    }
    void restoreCurrent() {
        eglBindAPI(EGL_OPENGL_ES_API);
        eglMakeCurrent(display_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglBindAPI(previousApi_);
        if (previousDisplay_ != EGL_NO_DISPLAY)
            eglMakeCurrent(previousDisplay_, previousDraw_, previousRead_, previousContext_);
    }
    std::shared_ptr<DisplayOwner> display_;
    EGLContext context_ = EGL_NO_CONTEXT, previousContext_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE, previousDraw_ = EGL_NO_SURFACE, previousRead_ = EGL_NO_SURFACE;
    EGLDisplay previousDisplay_ = EGL_NO_DISPLAY;
    wl_egl_window* waylandWindow_ = nullptr;
    sk_sp<GrDirectContext> skia_;
    sk_sp<SkSurface> frame_;
    std::string device_;
    EGLint stencil_ = 0;
    EGLenum previousApi_ = EGL_OPENGL_ES_API;
    bool synced_ = false, current_ = false;
};
} // namespace
std::unique_ptr<GpuRenderer> makeEglRenderer(void* display, void* window, bool wayland, std::string& failure) {
    try { auto renderer = std::make_unique<EglRenderer>(); renderer->initialize(display, window, wayland); return renderer; }
    catch (const std::exception& error) { failure = error.what(); return {}; }
}
} // namespace oneui::platform
