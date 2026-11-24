#include "window_win32_gl_worker.h"

#include "gl_flush_trace_win32.h"
#include "gl_wait_resolution_win32.h"
#include "window_win32_deferred_gl.h"
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
#include <condition_variable>
#include <cstring>
#include <exception>
#include <mutex>
#include <optional>
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

bool scratchGlWorkerEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_SCRATCH", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

enum class StartupCensus { None, Additional, OwnContextOnly };

StartupCensus workerStartupCensus() noexcept {
    static const auto census = [] {
        wchar_t value[2]{};
        if (GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_STARTUP_CENSUS", value, 2) != 1)
            return StartupCensus::None;
        if (value[0] == L'1') return StartupCensus::Additional;
        if (value[0] == L'2') return StartupCensus::OwnContextOnly;
        return StartupCensus::None;
    }();
    return census;
}

bool supersedeGlWorkerFrameEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_PRESENT_LATEST", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

enum class RenderResult { Complete, Superseded, Obsolete, Failed };

class LiveContext {
public:
    explicit LiveContext(HWND window) : window_(window), dc_(GetDC(window)) {}
    ~LiveContext() {
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

    bool initialize(const internal::InteractionSnapshot& startupTrace, bool warmScratchContext) {
        if (!dc_) return false;
        const auto descriptor = openGlWindowPixelFormat();
        format_ = ChoosePixelFormat(dc_, &descriptor);
        if (!format_ || !DescribePixelFormat(dc_, format_, sizeof(descriptor_), &descriptor_) ||
            !SetPixelFormat(dc_, format_, &descriptor)) return false;
        gl_ = wglCreateContext(dc_);
        if (!gl_ || !wglMakeCurrent(dc_, gl_)) return false;
        GrContextOptions options;
        options.fPersistentCache = rendering::shaderCache();
        const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const bool nvidia = vendor && std::strstr(vendor, "NVIDIA");
        if (nvidia && warmScratchContext) {
            internal::traceInteractionPaint(startupTrace, "worker_census_begin", "opengl-worker");
            const bool warmed = warmNvidiaDriverThreadCensus(dc_);
            internal::traceInteractionPaint(startupTrace, "worker_census_end", "opengl-worker", warmed ? 1 : 0);
        }
        context_ = GrDirectContexts::MakeGL(internal::makeWin32GaneshGlInterface(nvidia), options);
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

    bool prewarm(const StartupGlScene& scene) {
        return prewarmStartupGlScene(*context_, scene, retainedGlWorkerEnabled());
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
        const auto timerPolicy = experimentalGlWaitResolution();
        const ScopedGlWaitResolution frameWait(timerPolicy == GlWaitResolution::Frame);
        if (timerPolicy == GlWaitResolution::Frame && !waitResolutionReported_) {
            internal::traceInteractionPaint(snapshot, "worker_frame_wait_resolution", "opengl-worker", frameWait.acquired() ? 1 : 0);
            waitResolutionReported_ = true;
        }
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
    bool waitResolutionReported_ = false;
    sk_sp<GrDirectContext> context_;
    sk_sp<SkSurface> surface_;
    sk_sp<SkSurface> retained_;
    std::uint64_t retainedExtent_ = 0;
    std::uint64_t presentedExtent_ = 0;
    bool presented_ = false;
    int width_ = 0;
    int height_ = 0;
};

} // namespace

struct GlRenderWorker::State {
    struct Scene {
        sk_sp<SkPicture> picture;
        Frame frame;
        internal::InteractionSnapshot trace;
    };

    State(HWND window, HMONITOR monitor, POINT origin, StartupGlScene scene)
        : window(window), monitor(monitor), origin(origin), captureStartup(monitor && scene.picture),
          startup(std::move(scene)), ownsLiveWindow(!monitor) {}

    void run() noexcept;
    void render();

    const HWND window;
    const HMONITOR monitor;
    const POINT origin;
    std::atomic_bool captureStartup;
    std::atomic_bool ready{false};
    std::atomic_bool vsync{false};
    std::atomic_uint64_t extent{0};
    std::atomic_uint64_t queuedSerial{0};
    std::mutex mutex;
    std::condition_variable changed;
    StartupGlScene startup;
    Scene pending;
    Frame lastQueued;
    Frame lastPlanned;
    std::uint64_t latestPlan = 0;
    bool stopping = false;
    bool ownsLiveWindow;
    bool finished = false;
};

bool experimentalGlWorkerEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

bool experimentalGlWorkerAfterFirstFrame() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_AFTER_FIRST_FRAME", value, 2) == 1 &&
            value[0] == L'1';
    }();
    return enabled;
}

bool experimentalGlWorkerPrewarm() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_PREWARM", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled && scratchGlWorkerEnabled();
}

bool experimentalGlWorkerOwnsStartupWarmup(HWND window) {
    if (workerStartupCensus() != StartupCensus::OwnContextOnly || !experimentalGlWorkerEnabled() ||
        !scratchGlWorkerEnabled()) return false;
    POINT origin{};
    return nvidiaWindowDisplayOrigin(window, origin);
}

GlRenderWorker::~GlRenderWorker() { stop(); }

bool GlRenderWorker::start(HWND window) {
    return start(window, {});
}

bool GlRenderWorker::start(HWND window, StartupGlScene scene) {
    if (state_) return false;
    POINT origin{};
    HMONITOR monitor = nullptr;
    if (scratchGlWorkerEnabled()) {
        monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
        if (!monitor || !nvidiaWindowDisplayOrigin(window, origin)) return false;
    }
    try {
        struct Task {
            std::shared_ptr<State> state;
            HMODULE module = nullptr;

            static void CALLBACK run(PTP_CALLBACK_INSTANCE instance, void* context) noexcept {
                std::unique_ptr<Task> task(static_cast<Task*>(context));
                FreeLibraryWhenCallbackReturns(instance, task->module);
                CallbackMayRunLong(instance);
                task->state->run();
            }
        };
        auto state = std::make_shared<State>(window, monitor, origin, std::move(scene));
        auto task = std::make_unique<Task>();
        task->state = state;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               reinterpret_cast<LPCWSTR>(&Task::run), &task->module)) return false;
        // An abandoned scratch context can finish using this module after its window has closed.
        HMODULE module = task->module;
        auto* submitted = task.release();
        if (!TrySubmitThreadpoolCallback(Task::run, submitted, nullptr)) {
            delete submitted;
            FreeLibrary(module);
            return false;
        }
        state_ = std::move(state);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool GlRenderWorker::wantsStartupPicture() const noexcept {
    return state_ && state_->captureStartup.load(std::memory_order_relaxed);
}

void GlRenderWorker::setStartupPicture(StartupGlScene scene) {
    if (!wantsStartupPicture()) return;
    StartupGlScene replaced;
    std::lock_guard lock(state_->mutex);
    if (state_->captureStartup.load(std::memory_order_relaxed)) {
        replaced = std::exchange(state_->startup, std::move(scene));
    }
}

bool GlRenderWorker::ready() const noexcept {
    return state_ && state_->ready.load(std::memory_order_acquire);
}

bool GlRenderWorker::vsync() const noexcept {
    return state_ && state_->vsync.load(std::memory_order_relaxed);
}

GlRenderWorker::Frame GlRenderWorker::plan(int width, int height, RECT damage) {
    if (!state_) return {};
    auto& state = *state_;
    std::lock_guard lock(state.mutex);
    Frame frame{width, height, damage, retainedGlWorkerEnabled(),
                state.extent.load(std::memory_order_relaxed), ++state.latestPlan};
    const bool sameExtent = state.lastQueued.extent == frame.extent &&
        state.lastQueued.width == width && state.lastQueued.height == height;
    if (!frame.retained || !sameExtent) {
        frame.damage = RECT{0, 0, width, height};
    } else {
        // Include work replaced in the mailbox, and damage from a recording
        // superseded by paint re-entry on the window thread.
        if (state.pending.picture) UnionRect(&frame.damage, &frame.damage, &state.pending.frame.damage);
        if (state.lastPlanned.serial > state.lastQueued.serial && state.lastPlanned.extent == frame.extent &&
            state.lastPlanned.width == width && state.lastPlanned.height == height) {
            UnionRect(&frame.damage, &frame.damage, &state.lastPlanned.damage);
        }
    }
    state.lastPlanned = frame;
    return frame;
}

bool GlRenderWorker::enqueue(sk_sp<SkPicture> picture, const Frame& frame,
                             const internal::InteractionSnapshot& trace) {
    if (!state_) return false;
    auto& state = *state_;
    State::Scene replaced;
    {
        std::lock_guard lock(state.mutex);
        if (state.stopping || !ready() || frame.serial != state.latestPlan ||
            frame.extent != state.extent.load(std::memory_order_relaxed)) return false;
        state.lastQueued = frame;
        replaced = std::exchange(state.pending, State::Scene{std::move(picture), frame, trace});
        state.queuedSerial.store(frame.serial, std::memory_order_release);
    }
    if (replaced.picture) {
        internal::traceInteractionPaint(replaced.trace, "worker_frame_replaced", "opengl-worker");
    }
    state.changed.notify_one();
    return true;
}

void GlRenderWorker::invalidateExtent() {
    if (!state_) return;
    auto& state = *state_;
    State::Scene obsolete;
    {
        std::lock_guard lock(state.mutex);
        ++state.extent;
        obsolete = std::exchange(state.pending, {});
    }
}

void GlRenderWorker::stop() {
    auto state = std::exchange(state_, {});
    if (!state) return;
    State::Scene discarded;
    StartupGlScene abandoned;
    std::unique_lock lock(state->mutex);
    state->stopping = true;
    state->captureStartup.store(false, std::memory_order_relaxed);
    abandoned = std::move(state->startup);
    ++state->extent;
    discarded = std::exchange(state->pending, {});
    state->changed.notify_one();
    if (state->ownsLiveWindow) {
        state->changed.wait(lock, [&] { return state->finished; });
    } else if (!state->finished) {
        internal::traceInteractionPaint({reinterpret_cast<std::uintptr_t>(state->window)},
                                        "worker_scratch_cancel", "opengl-worker", 1);
    }
}

void GlRenderWorker::State::run() noexcept {
    try {
        render();
    } catch (const std::exception&) {
        // The window resumes the existing raster path on worker failure.
    }
    std::lock_guard lock(mutex);
    ready.store(false, std::memory_order_release);
    captureStartup.store(false, std::memory_order_relaxed);
    startup = {};
    if (!stopping) InvalidateRect(window, nullptr, FALSE);
    finished = true;
    changed.notify_all();
}

void GlRenderWorker::State::render() {
    const internal::InteractionSnapshot startupTrace{reinterpret_cast<std::uintptr_t>(window)};
    {
        std::lock_guard lock(mutex);
        if (stopping) return;
        internal::traceInteractionPaint(startupTrace, "worker_context_begin", "opengl-worker");
    }
    std::optional<StartupGlScratchWindow> scratch;
    if (monitor) {
        scratch.emplace();
        if (!scratch->open(origin)) return;
    }
    const auto timerPolicy = experimentalGlWaitResolution();
    const ScopedGlWaitResolution workerWait(timerPolicy == GlWaitResolution::WorkerLifetime);
    if (timerPolicy == GlWaitResolution::WorkerLifetime) {
        internal::traceInteractionPaint(startupTrace, "worker_lifetime_wait_resolution", "opengl-worker", workerWait.acquired() ? 1 : 0);
    }
    LiveContext renderer(scratch ? scratch->hwnd : window);
    const bool initialized = renderer.initialize(startupTrace,
        scratch && workerStartupCensus() != StartupCensus::None);
    StartupGlScene warmup;
    {
        std::lock_guard lock(mutex);
        if (stopping) return;
        internal::traceInteractionPaint(startupTrace, "worker_context_end", "opengl-worker", initialized ? 1 : 0);
        if (!initialized) return;
        captureStartup.store(false, std::memory_order_relaxed);
        warmup = std::move(startup);
        if (warmup.picture) {
            internal::traceInteractionPaint(startupTrace, "worker_prewarm_begin", "opengl-worker");
        }
    }
    const bool needsWarmup = scratch && warmup.picture;
    const bool warmed = !needsWarmup || renderer.prewarm(warmup);
    warmup = {};
    {
        std::lock_guard lock(mutex);
        if (stopping) return;
        if (needsWarmup) {
            internal::traceInteractionPaint(startupTrace, "worker_prewarm_end", "opengl-worker", warmed ? 1 : 0);
        }
        if (!warmed) return;
        // Close may proceed while the worker owns only its scratch resources.
        // After this point the worker can acquire the live HWND and DC.
        ownsLiveWindow = true;
    }
    if (scratch) {
        internal::traceInteractionPaint(startupTrace, "worker_adopt_begin", "opengl-worker");
        const bool adopted = renderer.adopt(window, monitor);
        internal::traceInteractionPaint(startupTrace, "worker_adopt_end", "opengl-worker", adopted ? 1 : 0);
        if (!adopted) return;
        scratch.reset();
    }
    const bool synced = renderer.enableVsync();
    {
        std::lock_guard lock(mutex);
        if (stopping) return;
        vsync.store(synced, std::memory_order_relaxed);
        ready.store(true, std::memory_order_release);
        InvalidateRect(window, nullptr, FALSE);
    }
    const bool supersede = supersedeGlWorkerFrameEnabled();
    bool mustPresent = false;
    for (;;) {
        Scene scene;
        {
            std::unique_lock lock(mutex);
            changed.wait(lock, [&] { return stopping || pending.picture; });
            if (stopping) break;
            scene = std::exchange(pending, {});
        }
        internal::traceInteractionPaint(scene.trace, "worker_scene_taken", "opengl-worker");
        const auto result = renderer.render(*scene.picture, scene.frame, scene.trace, extent,
                                            queuedSerial, supersede && !mustPresent);
        // Keep presenting regularly even while newer recordings keep arriving.
        if (result == RenderResult::Superseded) mustPresent = true;
        if (result == RenderResult::Complete) mustPresent = false;
        if (result == RenderResult::Failed) break;
        if (result == RenderResult::Obsolete) {
            Scene obsolete;
            std::lock_guard lock(mutex);
            if (!stopping && extent.load(std::memory_order_relaxed) == scene.frame.extent) {
                ++extent;
                obsolete = std::exchange(pending, {});
                InvalidateRect(window, nullptr, FALSE);
            }
        }
    }
}

} // namespace oneui::win32
