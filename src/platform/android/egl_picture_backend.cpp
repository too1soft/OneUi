#include "picture_raster_worker.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>

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

class EglPictureBackend final : public PictureRasterBackend {
public:
    bool attach(ANativeWindow* acquiredWindow, int width, int height) override;
    void reset() override;
    bool render(const SkPicture& picture) override;
    std::string renderer() const override { return renderer_; }
    std::string error() const override { return error_; }
    std::uint64_t contextCount() const override { return contexts_; }
    std::uint64_t resizeCount() const override { return resizes_; }
private:
    ANativeWindow* window_ = nullptr;
    std::shared_ptr<Display> display_;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    sk_sp<GrDirectContext> ganesh_;
    int width_ = 0, height_ = 0, stencil_ = 0;
    std::uint64_t contexts_ = 0, resizes_ = 0;
    std::string renderer_ = "unavailable", error_;
    bool makeCurrent() const {
        return display_ && eglBindAPI(EGL_OPENGL_ES_API) &&
            eglMakeCurrent(display_->value, surface_, surface_, context_);
    }
};

void EglPictureBackend::reset() {
    if (display_) {
        if (context_ != EGL_NO_CONTEXT && makeCurrent()) ganesh_.reset();
        else {
            if (ganesh_) ganesh_->abandonContext();
            ganesh_.reset();
        }
        eglMakeCurrent(display_->value, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_->value, surface_);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_->value, context_);
    }
    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
    display_.reset();
    if (window_) ANativeWindow_release(window_);
    window_ = nullptr;
    renderer_ = "unavailable";
    eglReleaseThread();
}

bool EglPictureBackend::attach(ANativeWindow* acquiredWindow, int width, int height) {
    if (acquiredWindow && acquiredWindow == window_ && width > 0 && height > 0 && ganesh_ && error_.empty()) {
        ANativeWindow_release(acquiredWindow);
        if (width_ != width || height_ != height) ++resizes_;
        width_ = width;
        height_ = height;
        return true;
    }
    reset();
    window_ = acquiredWindow;
    error_.clear();
    const auto fail = [&](const char* message) {
        error_ = message;
        reset();
        return false;
    };
    if (!window_ || width <= 0 || height <= 0) return fail("Invalid native surface dimensions");
    display_ = acquireDisplay();
    if (!display_) return fail("EGL display initialization failed");
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_STENCIL_SIZE, 8, EGL_NONE};
    EGLConfig config{};
    EGLint count = 0, visual = 0;
    if (!eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(display_->value, attributes, &config, 1, &count) || count != 1 ||
        !eglGetConfigAttrib(display_->value, config, EGL_NATIVE_VISUAL_ID, &visual) ||
        !eglGetConfigAttrib(display_->value, config, EGL_STENCIL_SIZE, &stencil_) ||
        ANativeWindow_setBuffersGeometry(window_, 0, 0, visual) != 0)
        return fail("EGL ES3 window configuration unavailable");
    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_->value, config, EGL_NO_CONTEXT, contextAttributes);
    surface_ = eglCreateWindowSurface(display_->value, config, window_, nullptr);
    if (context_ == EGL_NO_CONTEXT || surface_ == EGL_NO_SURFACE || !makeCurrent())
        return fail("EGL window/context creation failed");
    if (!eglSwapInterval(display_->value, 1)) return fail("EGL swap interval rejected");
    auto interface = GrGLMakeAssembledGLESInterface(nullptr, [](void*, const char* name) {
        return reinterpret_cast<GrGLFuncPtr>(eglGetProcAddress(name));
    });
    ganesh_ = GrDirectContexts::MakeGL(interface);
    if (!ganesh_) return fail("Skia Ganesh GLES initialization failed");
    const auto* name = glGetString(GL_RENDERER);
    renderer_ = name ? reinterpret_cast<const char*>(name) : "unknown";
    width_ = width;
    height_ = height;
    ++contexts_;
    return true;
}

bool EglPictureBackend::render(const SkPicture& picture) {
    if (!ganesh_ || !error_.empty()) return false;
    const GrGLFramebufferInfo info{0, GL_RGBA8};
    auto target = GrBackendRenderTargets::MakeGL(width_, height_, 0, stencil_, info);
    auto frame = SkSurfaces::WrapBackendRenderTarget(ganesh_.get(), target,
        kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, nullptr, nullptr);
    if (!frame) { error_ = "Skia window framebuffer unavailable"; return false; }
    frame->getCanvas()->drawPicture(&picture);
    ganesh_->flushAndSubmit();
    frame.reset();
    const bool success = eglSwapBuffers(display_->value, surface_) == EGL_TRUE;
    if (!success) error_ = "EGL presentation failed; surface recreation required";
    return success;
}

} // namespace

std::unique_ptr<PictureRasterBackend> makeEglPictureBackend() {
    return std::make_unique<EglPictureBackend>();
}

} // namespace oneui::mobile
