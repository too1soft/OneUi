#pragma once
#include <EGL/egl.h>
#include <android/native_window.h>

// Integration-test renderer, not the OneUI Canvas/Skia backend.
// The host owns UI-thread affinity; no hidden render thread or frame loop.
class EglSurface final {
public:
    ~EglSurface() { reset(); }
    EglSurface() = default;
    EglSurface(const EglSurface&) = delete;
    EglSurface& operator=(const EglSurface&) = delete;
    bool attach(ANativeWindow* acquiredWindow);
    void reset();
    bool draw(int width, int height, float x, float y, float scale, bool pressed);
    const char* renderer() const;
private:
    ANativeWindow* window_ = nullptr;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
};
