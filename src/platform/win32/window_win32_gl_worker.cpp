#include "window_win32_gl_worker.h"

#include "gl_flush_trace_win32.h"
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

#include <exception>
#include <utility>

namespace oneui::win32 {
namespace {

bool retainedGlWorkerEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_DAMAGE", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

enum class RenderResult { Complete, Obsolete, Failed };

class LiveContext {
public:
    explicit LiveContext(HWND window) : window_(window), dc_(GetDC(window)) {}
    ~LiveContext() {
        retained_.reset();
        surface_.reset();
        context_.reset();
        if (gl_) {
            if (wglGetCurrentContext() == gl_) wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(gl_);
        }
        if (dc_) ReleaseDC(window_, dc_);
    }

    bool initialize(bool& vsync) {
        if (!dc_) return false;
        const auto descriptor = openGlWindowPixelFormat();
        const int format = ChoosePixelFormat(dc_, &descriptor);
        if (!format || !SetPixelFormat(dc_, format, &descriptor)) return false;
        gl_ = wglCreateContext(dc_);
        if (!gl_ || !wglMakeCurrent(dc_, gl_)) return false;
        const auto interval = reinterpret_cast<BOOL(WINAPI*)(int)>(wglGetProcAddress("wglSwapIntervalEXT"));
        vsync = interval && interval(1);
        GrContextOptions options;
        options.fPersistentCache = rendering::shaderCache();
        context_ = internal::win32GlTraceEnabled()
            ? GrDirectContexts::MakeGL(internal::makeWin32GlTraceInterface(), options)
            : GrDirectContexts::MakeGL(options);
        return static_cast<bool>(context_);
    }

    RenderResult render(const SkPicture& picture, const GlRenderWorker::Frame& frame,
                        const internal::InteractionSnapshot& snapshot,
                        const std::atomic_uint64_t& extent) {
        const auto width = frame.width;
        const auto height = frame.height;
        const auto current = [&] {
            return extent.load(std::memory_order_acquire) == frame.extent && matchesClient(width, height);
        };
        if (!current()) return RenderResult::Obsolete;
        if (!surface_ || width_ != width || height_ != height) {
            GrGLFramebufferInfo framebuffer{};
            framebuffer.fFormat = GL_RGBA8;
            auto target = GrBackendRenderTargets::MakeGL(width, height, 0, 0, framebuffer);
            surface_ = SkSurfaces::WrapBackendRenderTarget(context_.get(), target,
                kBottomLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType, nullptr, nullptr);
            if (!surface_) return RenderResult::Failed;
            retained_.reset();
            width_ = width;
            height_ = height;
        }

        const auto trace = [&](const char* phase, int success = -1) {
            internal::traceInteractionPaint(snapshot, phase, "opengl-worker", success);
        };
        const bool full = frame.damage.left == 0 && frame.damage.top == 0 &&
            frame.damage.right == width && frame.damage.bottom == height;
        if (frame.retained && (!retained_ || retainedExtent_ != frame.extent)) {
            if (!full) return RenderResult::Obsolete;
            if (!retained_) {
                retained_ = SkSurfaces::RenderTarget(context_.get(), skgpu::Budgeted::kYes,
                    SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType),
                    0, kTopLeft_GrSurfaceOrigin, nullptr);
                if (!retained_) return RenderResult::Failed;
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
        context_->flush();
        trace("worker_submit_begin");
        const bool tracing = snapshot.paintSequence && internal::win32GlTraceEnabled()
            && internal::scrollTraceEnabled();
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
        trace("swap_begin");
        const bool swapped = SwapBuffers(dc_) != FALSE;
        trace("swap_return", swapped ? 1 : 0);
        return swapped ? RenderResult::Complete : RenderResult::Failed;
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
    sk_sp<GrDirectContext> context_;
    sk_sp<SkSurface> surface_;
    sk_sp<SkSurface> retained_;
    std::uint64_t retainedExtent_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace

bool experimentalGlWorkerEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

GlRenderWorker::~GlRenderWorker() { stop(); }

bool GlRenderWorker::start(HWND window) {
    if (thread_.joinable()) return false;
    try {
        thread_ = std::thread([this, window] { run(window); });
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

GlRenderWorker::Frame GlRenderWorker::plan(int width, int height, RECT damage) {
    std::lock_guard lock(mutex_);
    Frame frame{width, height, damage, retainedGlWorkerEnabled(),
                extent_.load(std::memory_order_relaxed), ++latestPlan_};
    const bool sameExtent = lastQueued_.extent == frame.extent &&
        lastQueued_.width == width && lastQueued_.height == height;
    if (!frame.retained || !sameExtent) {
        frame.damage = RECT{0, 0, width, height};
    } else {
        // Include work replaced in the mailbox, and damage from a recording
        // superseded by paint re-entry on the window thread.
        if (pending_.picture) UnionRect(&frame.damage, &frame.damage, &pending_.frame.damage);
        if (lastPlanned_.serial > lastQueued_.serial && lastPlanned_.extent == frame.extent &&
            lastPlanned_.width == width && lastPlanned_.height == height) {
            UnionRect(&frame.damage, &frame.damage, &lastPlanned_.damage);
        }
    }
    lastPlanned_ = frame;
    return frame;
}

bool GlRenderWorker::enqueue(sk_sp<SkPicture> picture, const Frame& frame,
                             const internal::InteractionSnapshot& trace) {
    Scene replaced;
    {
        std::lock_guard lock(mutex_);
        if (stopping_ || !ready() || frame.serial != latestPlan_ ||
            frame.extent != extent_.load(std::memory_order_relaxed)) return false;
        lastQueued_ = frame;
        replaced = std::exchange(pending_, Scene{std::move(picture), frame, trace});
    }
    if (replaced.picture) {
        internal::traceInteractionPaint(replaced.trace, "worker_frame_replaced", "opengl-worker");
    }
    changed_.notify_one();
    return true;
}

void GlRenderWorker::invalidateExtent() {
    Scene obsolete;
    {
        std::lock_guard lock(mutex_);
        ++extent_;
        obsolete = std::exchange(pending_, {});
    }
}

void GlRenderWorker::stop() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    changed_.notify_one();
    if (thread_.joinable()) thread_.join();
    ready_.store(false, std::memory_order_release);
    pending_ = {};
}

void GlRenderWorker::run(HWND window) {
    try {
        LiveContext renderer(window);
        bool synced = false;
        if (renderer.initialize(synced)) {
            {
                std::lock_guard lock(mutex_);
                if (stopping_) return;
                vsync_.store(synced, std::memory_order_relaxed);
                ready_.store(true, std::memory_order_release);
                InvalidateRect(window, nullptr, FALSE);
            }
            for (;;) {
                Scene scene;
                {
                    std::unique_lock lock(mutex_);
                    changed_.wait(lock, [&] { return stopping_ || pending_.picture; });
                    if (stopping_) break;
                    scene = std::exchange(pending_, {});
                }
                const auto result = renderer.render(*scene.picture, scene.frame, scene.trace, extent_);
                if (result == RenderResult::Failed) break;
                if (result == RenderResult::Obsolete) {
                    Scene obsolete;
                    std::lock_guard lock(mutex_);
                    if (!stopping_ && extent_.load(std::memory_order_relaxed) == scene.frame.extent) {
                        ++extent_;
                        obsolete = std::exchange(pending_, {});
                        InvalidateRect(window, nullptr, FALSE);
                    }
                }
            }
        }
    } catch (const std::exception&) {
        // The window resumes the existing raster path on worker failure.
    }
    std::lock_guard lock(mutex_);
    ready_.store(false, std::memory_order_release);
    if (!stopping_) InvalidateRect(window, nullptr, FALSE);
}

} // namespace oneui::win32
