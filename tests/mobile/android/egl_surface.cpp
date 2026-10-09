#include "egl_surface.h"
#include <GLES2/gl2.h>
#include <algorithm>

void EglSurface::reset() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        eglTerminate(display_);
    }
    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
    display_ = EGL_NO_DISPLAY;
    if (window_) ANativeWindow_release(window_);
    window_ = nullptr;
}

bool EglSurface::attach(ANativeWindow* acquiredWindow) {
    reset();
    window_ = acquiredWindow; // Take the ANativeWindow_fromSurface reference.
    if (!window_) return false;
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    const EGLint configAttributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLConfig config{};
    EGLint count = 0, visual = 0;
    if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, nullptr, nullptr) ||
        !eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(display_, configAttributes, &config, 1, &count) || count != 1 ||
        !eglGetConfigAttrib(display_, config, EGL_NATIVE_VISUAL_ID, &visual) ||
        ANativeWindow_setBuffersGeometry(window_, 0, 0, visual) != 0) {
        reset(); return false;
    }
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttributes);
    surface_ = eglCreateWindowSurface(display_, config, window_, nullptr);
    if (context_ == EGL_NO_CONTEXT || surface_ == EGL_NO_SURFACE ||
        !eglMakeCurrent(display_, surface_, surface_, context_) || !eglSwapInterval(display_, 1)) {
        reset(); return false;
    }
    return true;
}

const char* EglSurface::renderer() const {
    if (context_ == EGL_NO_CONTEXT) return "unavailable";
    const auto* name = glGetString(GL_RENDERER);
    return name ? reinterpret_cast<const char*>(name) : "unknown";
}

bool EglSurface::draw(int width, int height, float x, float y, float scale, bool pressed) {
    if (context_ == EGL_NO_CONTEXT || !eglMakeCurrent(display_, surface_, surface_, context_)) return false;
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(.93f, .96f, .94f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    const int side = std::max(1, std::min({static_cast<int>(72 * scale), width, height}));
    const int centerX = static_cast<int>(std::clamp(x * scale, 0.f, static_cast<float>(width)));
    const int centerY = static_cast<int>(std::clamp(y * scale, 0.f, static_cast<float>(height)));
    const int left = std::clamp(centerX - side / 2, 0, width - side);
    const int top = std::clamp(centerY - side / 2, 0, height - side);
    glEnable(GL_SCISSOR_TEST);
    glScissor(left, height - top - side, side, side);
    if (pressed) glClearColor(.20f, .52f, .32f, 1.f);
    else glClearColor(.16f, .38f, .24f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    return glGetError() == GL_NO_ERROR && eglSwapBuffers(display_, surface_) == EGL_TRUE;
}
