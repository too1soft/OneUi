#pragma once

#include "gl_flush_trace_win32.h"
#include "window_win32_deferred_gl.h"
#include "window_win32_gl_worker.h"
#include "window_win32_gpu_backing.h"
#include "window_win32_gpu_warmup.h"
#include "platform/shared/skia_gl_setup.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"

#include <GL/gl.h>
#include <atomic>
#include <cstring>

namespace oneui::win32::worker {

enum class RenderResult { Complete, Superseded, Obsolete, Failed };

class WglWorkerContext {
public:
    explicit WglWorkerContext(HWND window) : window_(window), dc_(GetDC(window)) {}
    ~WglWorkerContext() {
        if (context_ && wglGetCurrentContext() != gl_) context_->abandonContext();
        retained_.reset();
        surface_.reset();
        context_.reset();
        if (gl_) {
            if (wglGetCurrentContext() == gl_) wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(gl_);
        }
        if (dc_) ReleaseDC(window_, dc_);
    }

    bool initialize(bool warmDriver) {
        if (!dc_) return false;
        const bool tracing = internal::scrollTraceEnabled();
        const auto now = [tracing] { return tracing ? internal::scrollTraceNowMs() : 0.0; };
        const auto trace = [tracing](const char* phase, double started, bool success) {
            if (tracing) internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", phase, 0, 0, 0, 0, 0, 0, 0,
                internal::scrollTraceNowMs() - started, success ? 1.0 : 0.0, 0});
        };
        const auto formatStarted = now();
        const auto descriptor = openGlWindowPixelFormat();
        format_ = ChoosePixelFormat(dc_, &descriptor);
        const bool formatSet = format_ && DescribePixelFormat(dc_, format_, sizeof(descriptor_), &descriptor_) &&
            SetPixelFormat(dc_, format_, &descriptor);
        trace("wgl_worker_pixel_format", formatStarted, formatSet);
        if (!formatSet) return false;
        const auto contextStarted = now();
        gl_ = wglCreateContext(dc_);
        trace("wgl_worker_create_context", contextStarted, gl_ != nullptr);
        if (!gl_) return false;
        const auto bindStarted = now();
        const bool bound = wglMakeCurrent(dc_, gl_) != FALSE;
        trace("wgl_worker_bind_context", bindStarted, bound);
        if (!bound) return false;
        if (warmDriver) {
            static const bool skipCensus = [] {
                wchar_t value[2]{};
                return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_SKIP_CENSUS", value, 2) == 1 &&
                    value[0] == L'1';
            }();
            if (skipCensus) trace("wgl_worker_census_skipped", now(), true);
            else warmNvidiaDriverThreadCensus(dc_);
        }
        const auto ganeshStarted = now();
        GrContextOptions options;
        options.fPersistentCache = rendering::shaderCache();
        context_ = GrDirectContexts::MakeGL(
            internal::makeWin32GaneshGlInterface(internal::win32GlNvidiaVendor()), options);
        trace("wgl_worker_ganesh_context", ganeshStarted, static_cast<bool>(context_));
        return static_cast<bool>(context_);
    }

    bool adopt(HWND target, HMONITOR monitor) {
        if (MonitorFromWindow(target, MONITOR_DEFAULTTONULL) != monitor ||
            (descriptor_.dwFlags & PFD_GENERIC_FORMAT)) return false;
        HDC dc = GetDC(target);
        if (!dc) return false;
        PIXELFORMATDESCRIPTOR actual{};
        if (!DescribePixelFormat(dc, format_, sizeof(actual), &actual) ||
            std::memcmp(&actual, &descriptor_, sizeof(actual)) != 0 ||
            !SetPixelFormat(dc, format_, &descriptor_) || !wglMakeCurrent(dc, gl_)) {
            ReleaseDC(target, dc);
            return false;
        }
        ReleaseDC(window_, dc_);
        window_ = target;
        dc_ = dc;
        return true;
    }

    bool enableVsync() const {
        const auto interval = reinterpret_cast<BOOL(WINAPI*)(int)>(wglGetProcAddress("wglSwapIntervalEXT"));
        return interval && interval(1);
    }

    bool prewarm(const StartupGlScene& scene, bool retained) {
        static const bool reuse = [] {
            wchar_t value[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_REUSE_WARM_BACKING", value, 2) == 1 &&
                value[0] == L'1';
        }();
        return prewarmStartupGlScene(*context_, scene, retained, reuse && retained ? &retained_ : nullptr);
    }

    RenderResult render(const SkPicture& picture, const GlRenderWorker::Frame& frame,
                        const internal::InteractionSnapshot& snapshot,
                        const std::atomic_uint64_t& extent,
                        const std::atomic_uint64_t& queuedSerial, bool maySupersede) {
        const auto width = frame.width;
        const auto height = frame.height;
        const auto current = [&] {
            return extent.load(std::memory_order_acquire) == frame.extent && matchesClient(width, height);
        };
        if (!current()) return RenderResult::Obsolete;
        const bool warmedBackingFits = !surface_ && retained_ &&
            retained_->width() == width && retained_->height() == height;
        const bool drawableChanged = !surface_ || width_ != width || height_ != height;
        const bool full = frame.damage.left == 0 && frame.damage.top == 0 &&
            frame.damage.right == width && frame.damage.bottom == height;
        const bool needsFullRecording = frame.retained &&
            (!retained_ || retainedExtent_ != frame.extent || drawableChanged);
        if (needsFullRecording && !full) return RenderResult::Obsolete;
        if (drawableChanged) {
            GrGLFramebufferInfo framebuffer{};
            framebuffer.fFormat = GL_RGBA8;
            auto target = GrBackendRenderTargets::MakeGL(width, height, 0, 0, framebuffer);
            surface_ = SkSurfaces::WrapBackendRenderTarget(context_.get(), target,
                kBottomLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType, nullptr, nullptr);
            if (!surface_) return RenderResult::Failed;
            width_ = width;
            height_ = height;
        }

        const auto trace = [&](const char* phase, int success = -1) {
            internal::traceInteractionPaint(snapshot, phase, "opengl-worker", success);
        };
        if (needsFullRecording) {
            if (!retained_ || drawableChanged) {
                const bool tracing = internal::scrollTraceEnabled();
                const double started = tracing ? internal::scrollTraceNowMs() : 0.0;
                // The first live recording repaints the full window. Only the allocation is kept.
                auto backing = warmedBackingFits ? RetainedGpuBacking{retained_, true}
                    : resizeGpuBacking(*context_, retained_.get(), width, height, frame.interactiveResize);
                if (warmedBackingFits) trace("worker_startup_backing_reused", 1);
                retained_ = std::move(backing.surface);
                if (!retained_) return RenderResult::Failed;
                if (tracing && !backing.reused) {
                    internal::writeScrollTrace(internal::ScrollTraceEvent{
                        "win32", "worker_backing_store", snapshot.window, 0, 0, 0, 0, 0, 0,
                        internal::scrollTraceNowMs() - started, static_cast<double>(snapshot.paintSequence),
                        static_cast<double>(retained_->width()) * retained_->height()});
                }
            }
            retainedExtent_ = frame.extent;
        }
        trace("worker_draw_begin");
        auto* canvas = surface_->getCanvas();
        if (frame.retained) {
            retained_->getCanvas()->drawPicture(&picture);
            canvas->clear(SK_ColorBLACK);
            retained_->draw(canvas, 0.0f, 0.0f);
        } else {
            canvas->drawPicture(&picture);
        }
        const bool traceCalls = snapshot.paintSequence && internal::win32GlTraceEnabled();
        const bool tracing = traceCalls && internal::scrollTraceEnabled();
        trace("worker_flush_begin");
        if (traceCalls) {
            internal::GlCallTrace::measureWorkerFlush(snapshot, tracing, [&] { context_->flush(); });
        } else {
            context_->flush();
        }
        trace("worker_flush_end");
        trace("worker_submit_begin");
        bool submitted;
        if (tracing) {
            internal::GlCallTrace calls;
            {
                internal::GlCallTrace::Scope scope(&calls);
                submitted = context_->submit(GrSyncCpu::kNo);
            }
            calls.write(snapshot.window, static_cast<double>(snapshot.paintSequence), true);
        } else {
            submitted = context_->submit(GrSyncCpu::kNo);
        }
        trace("worker_submit_end", submitted ? 1 : 0);
        if (!submitted) return RenderResult::Failed;
        if (!current()) return RenderResult::Obsolete;
        if (maySupersede && presented_ && presentedExtent_ == frame.extent &&
            queuedSerial.load(std::memory_order_acquire) > frame.serial) {
            // The retained surface includes this scene's damage for the next frame.
            trace("worker_present_superseded");
            return RenderResult::Superseded;
        }
        trace("swap_begin");
        const bool swapped = SwapBuffers(dc_) != FALSE;
        trace("swap_return", swapped ? 1 : 0);
        if (!swapped) return RenderResult::Failed;
        presented_ = true;
        presentedExtent_ = frame.extent;
        return RenderResult::Complete;
    }

private:
    bool matchesClient(int width, int height) const {
        RECT bounds{};
        return GetClientRect(window_, &bounds) &&
            bounds.right - bounds.left == width && bounds.bottom - bounds.top == height;
    }

    HWND window_;
    HDC dc_;
    HGLRC gl_ = nullptr;
    PIXELFORMATDESCRIPTOR descriptor_{};
    int format_ = 0;
    sk_sp<GrDirectContext> context_;
    sk_sp<SkSurface> surface_;
    sk_sp<SkSurface> retained_;
    std::uint64_t retainedExtent_ = 0;
    std::uint64_t presentedExtent_ = 0;
    bool presented_ = false;
    int width_ = 0;
    int height_ = 0;
};


} // namespace oneui::win32::worker
