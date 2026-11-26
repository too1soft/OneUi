#include "window_win32_gl_worker.h"

#include "window_win32_deferred_gl.h"
#include "window_win32_gl_worker_context.h"
#include "window_win32_gpu_warmup.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <mutex>
#include <optional>
#include <utility>

namespace oneui::win32 {
namespace {

bool startupLowPriorityEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_ANGLE_STARTUP_LOW_PRIORITY", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

class StartupThreadPriority {
public:
    explicit StartupThreadPriority(bool enabled) noexcept
        : lowered_(enabled && GetThreadPriority(GetCurrentThread()) == THREAD_PRIORITY_NORMAL &&
                   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL)) {}
    ~StartupThreadPriority() { restore(); }
    StartupThreadPriority(const StartupThreadPriority&) = delete;
    StartupThreadPriority& operator=(const StartupThreadPriority&) = delete;

    bool lowered() const noexcept { return lowered_; }
    bool restore() noexcept {
        if (lowered_ && SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL)) lowered_ = false;
        return !lowered_;
    }

private:
    bool lowered_;
};

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

bool supersedeGlWorkerFrameEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_PRESENT_LATEST", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

bool softwareHandoffEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_ANGLE_SOFTWARE_HANDOFF", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

enum class ScratchCensusOrder { BeforeScene, AfterScene, RetryAfterScene };

ScratchCensusOrder scratchCensusOrder() noexcept {
    static const auto order = [] {
        wchar_t value[2]{};
        if (GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_CENSUS_AFTER_SCENE", value, 2) != 1)
            return ScratchCensusOrder::BeforeScene;
        if (value[0] == L'1') return ScratchCensusOrder::AfterScene;
        if (value[0] == L'2') return ScratchCensusOrder::RetryAfterScene;
        return ScratchCensusOrder::BeforeScene;
    }();
    return order;
}

bool rehearseScratchPresentsEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_REHEARSE_PRESENTS", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}
} // namespace

struct GlRenderWorker::State {
    struct Scene {
        sk_sp<SkPicture> picture;
        Frame frame;
        internal::InteractionSnapshot trace;
    };

    State(HWND window, HMONITOR monitor, POINT origin, bool angle, StartupGlScene scene, bool awaitSoftwareFrame)
        : window(window), monitor(monitor), origin(origin), angle(angle),
          captureStartup(monitor && (awaitSoftwareFrame || scene.picture)),
          softwareHandoff(angle && softwareHandoffEnabled()), startup(std::move(scene)),
          awaitingSoftwareFrame(awaitSoftwareFrame), ownsLiveWindow(!monitor) {}

    void run() noexcept;
    void render();
    bool claimPresent(const Scene& scene);

    const HWND window;
    const HMONITOR monitor;
    const POINT origin;
    const bool angle;
    std::atomic_bool captureStartup;
    std::atomic_bool softwareHandoff;
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
    bool recordingWaiter = false;
    bool awaitingSoftwareFrame;
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

bool experimentalAngleWorkerEnabled() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return AngleD3D11Context::requested() &&
            GetEnvironmentVariableW(L"ONEUI_WIN32_ANGLE_WORKER", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

bool experimentalAngleWorkerStartsEarly() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return experimentalAngleWorkerEnabled() &&
            GetEnvironmentVariableW(L"ONEUI_WIN32_ANGLE_WORKER_EARLY_CONTEXT", value, 2) == 1 && value[0] == L'1';
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

bool experimentalGlWorkerDefersCensus() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_DEFER_CENSUS", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled && experimentalGlWorkerEnabled() && experimentalGlWorkerAfterFirstFrame() &&
        scratchGlWorkerEnabled();
}

bool experimentalGlWorkerPacesRecording() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_WORKER_PACE_RECORDING", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

bool experimentalAngleCoalescesSoftwareHandoff() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return experimentalAngleWorkerEnabled() && softwareHandoffEnabled() &&
            GetEnvironmentVariableW(L"ONEUI_WIN32_ANGLE_COALESCE_SOFTWARE_HANDOFF", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

GlRenderWorker::~GlRenderWorker() { stop(); }

bool GlRenderWorker::start(HWND window) {
    return start(window, {});
}

bool GlRenderWorker::start(HWND window, StartupGlScene scene) {
    return start(window, std::move(scene), false);
}

bool GlRenderWorker::startBeforeSoftwareFrame(HWND window) {
    return start(window, {}, true);
}

bool GlRenderWorker::start(HWND window, StartupGlScene scene, bool awaitSoftwareFrame) {
    if (state_) return false;
    const bool angle = experimentalAngleWorkerEnabled();
    if (awaitSoftwareFrame && !angle) return false;
    POINT origin{};
    HMONITOR monitor = nullptr;
    if (angle) {
        monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!monitor || !GetMonitorInfoW(monitor, &info)) return false;
        origin = POINT{info.rcMonitor.left, info.rcMonitor.top};
    } else if (scratchGlWorkerEnabled()) {
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
        auto state = std::make_shared<State>(window, monitor, origin, angle, std::move(scene), awaitSoftwareFrame);
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

bool GlRenderWorker::setStartupPicture(StartupGlScene scene) {
    if (!wantsStartupPicture() || !scene.picture) return false;
    auto& state = *state_;
    StartupGlScene replaced;
    bool firstSoftwareFrame;
    {
        std::lock_guard lock(state.mutex);
        if (!state.captureStartup.load(std::memory_order_relaxed)) return false;
        replaced = std::exchange(state.startup, std::move(scene));
        firstSoftwareFrame = std::exchange(state.awaitingSoftwareFrame, false);
    }
    if (firstSoftwareFrame) state.changed.notify_one();
    return firstSoftwareFrame;
}

bool GlRenderWorker::ready() const noexcept {
    return state_ && state_->ready.load(std::memory_order_acquire);
}

bool GlRenderWorker::softwareHandoffPending() const noexcept {
    return ready() && state_->softwareHandoff.load(std::memory_order_relaxed);
}

bool GlRenderWorker::vsync() const noexcept {
    return state_ && state_->vsync.load(std::memory_order_relaxed);
}

bool GlRenderWorker::deferRecording() {
    if (!state_) return false;
    auto& state = *state_;
    std::lock_guard lock(state.mutex);
    state.recordingWaiter = !state.stopping && state.ready.load(std::memory_order_relaxed) &&
        !state.softwareHandoff.load(std::memory_order_relaxed) &&
        static_cast<bool>(state.pending.picture);
    return state.recordingWaiter;
}

GlRenderWorker::Frame GlRenderWorker::plan(int width, int height, RECT damage, bool interactiveResize) {
    if (!state_) return {};
    auto& state = *state_;
    std::lock_guard lock(state.mutex);
    Frame frame{width, height, damage, state.angle || retainedGlWorkerEnabled(), interactiveResize,
                state.extent.load(std::memory_order_relaxed), ++state.latestPlan,
                state.softwareHandoff.load(std::memory_order_relaxed)};
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

bool GlRenderWorker::State::claimPresent(const Scene& scene) {
    if (!softwareHandoff.load(std::memory_order_relaxed)) return true;
    bool claimed;
    {
        std::lock_guard lock(mutex);
        // The window reserves a serial before drawing in software and publishes
        // it after EndPaint. A newer reservation still owns the visible window.
        claimed = !stopping && latestPlan == scene.frame.serial &&
            extent.load(std::memory_order_relaxed) == scene.frame.extent;
        if (claimed) softwareHandoff.store(false, std::memory_order_relaxed);
    }
    internal::traceInteractionPaint(scene.trace, claimed ? "worker_handoff_claimed" : "worker_handoff_superseded",
                                    "opengl-worker");
    return claimed;
}

void GlRenderWorker::invalidateExtent() {
    if (!state_) return;
    auto& state = *state_;
    State::Scene obsolete;
    {
        std::lock_guard lock(state.mutex);
        ++state.extent;
        state.recordingWaiter = false;
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
    state->recordingWaiter = false;
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
    recordingWaiter = false;
    startup = {};
    if (!stopping) InvalidateRect(window, nullptr, FALSE);
    finished = true;
    changed.notify_all();
}

void GlRenderWorker::State::render() {
    const internal::InteractionSnapshot startupTrace{reinterpret_cast<std::uintptr_t>(window)};
    const bool limitStartupPriority = angle && monitor && startupLowPriorityEnabled();
    StartupThreadPriority startupPriority(limitStartupPriority);
    if (limitStartupPriority)
        internal::traceInteractionPaint(startupTrace, "worker_startup_priority_lowered", "opengl-worker",
                                        startupPriority.lowered() ? 1 : 0);
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
    worker::GlWorkerContext renderer(scratch ? scratch->hwnd : window, angle);
    // With deferred warmup, the first software frame has already been copied.
    const bool census = !angle && scratch && experimentalGlWorkerDefersCensus();
    const auto censusOrder = census ? scratchCensusOrder() : ScratchCensusOrder::BeforeScene;
    const bool censusAfterScene = censusOrder == ScratchCensusOrder::AfterScene;
    const bool retryCensus = censusOrder == ScratchCensusOrder::RetryAfterScene;
    double firstFinishMs = -1.0;
    const bool initialized = renderer.initialize(census && !censusAfterScene, retryCensus ? &firstFinishMs : nullptr);
    StartupGlScene warmup;
    {
        std::unique_lock lock(mutex);
        if (stopping) return;
        internal::traceInteractionPaint(startupTrace, "worker_context_end", "opengl-worker", initialized ? 1 : 0);
        if (!initialized) return;
        changed.wait(lock, [&] { return stopping || !awaitingSoftwareFrame; });
        if (stopping) return;
        captureStartup.store(false, std::memory_order_relaxed);
        warmup = std::move(startup);
        if (warmup.picture) {
            internal::traceInteractionPaint(startupTrace, "worker_prewarm_begin", "opengl-worker");
        }
    }
    const bool needsWarmup = scratch && warmup.picture;
    const bool warmed = !needsWarmup || renderer.prewarm(warmup, angle || retainedGlWorkerEnabled());
    warmup = {};
    // A nearly immediate glFinish may have missed the process-wide driver wait.
    // Retry once, after more real GL work, while the live HWND is still unowned.
    const bool missedWait = retryCensus && firstFinishMs >= 0.0 && firstFinishMs < 1.0;
    if (warmed && (censusAfterScene || missedWait)) renderer.warmDriverCensus();
    // Exercise the default framebuffer and real Ganesh submits before
    // acquiring the live window, where close would need to wait for us.
    if (warmed && !angle && needsWarmup && rehearseScratchPresentsEnabled() && !renderer.rehearsePresents()) return;
    if (limitStartupPriority)
        internal::traceInteractionPaint(startupTrace, "worker_startup_priority_restored", "opengl-worker",
                                        startupPriority.restore() ? 1 : 0);
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
        if (const auto* angleContext = renderer.angleContext()) {
            internal::traceInteractionPaint(startupTrace, "angle_worker_ready", "opengl-worker", 1);
            std::fprintf(stderr, "OneUI ANGLE worker enabled (%s)\n", angleContext->device().c_str());
        }
        vsync.store(synced, std::memory_order_relaxed);
        ready.store(true, std::memory_order_release);
        InvalidateRect(window, nullptr, FALSE);
    }
    const bool supersede = supersedeGlWorkerFrameEnabled();
    bool mustPresent = false;
    for (;;) {
        Scene scene;
        bool notifyRecorder;
        {
            std::unique_lock lock(mutex);
            changed.wait(lock, [&] { return stopping || pending.picture; });
            if (stopping) break;
            scene = std::exchange(pending, {});
            notifyRecorder = std::exchange(recordingWaiter, false);
        }
        if (notifyRecorder && !PostMessageW(window, glWorkerRecordingReadyMessage, 0, 0))
            InvalidateRect(window, nullptr, FALSE);
        internal::traceInteractionPaint(scene.trace, "worker_scene_taken", "opengl-worker");
        const auto result = renderer.render(*scene.picture, scene.frame, scene.trace, extent,
                                            queuedSerial, supersede && !mustPresent,
                                            [&] { return claimPresent(scene); });
        // Keep presenting regularly even while newer recordings keep arriving.
        if (result == worker::RenderResult::Superseded) mustPresent = true;
        if (result == worker::RenderResult::Complete) mustPresent = false;
        if (result == worker::RenderResult::Failed) break;
        if (result == worker::RenderResult::Scaled) {
            std::lock_guard lock(mutex);
            if (!stopping && !pending.picture && !IsIconic(window)) InvalidateRect(window, nullptr, FALSE);
        }
        if (result == worker::RenderResult::Obsolete) {
            Scene obsolete;
            std::lock_guard lock(mutex);
            if (!stopping && extent.load(std::memory_order_relaxed) == scene.frame.extent) {
                ++extent;
                recordingWaiter = false;
                obsolete = std::exchange(pending, {});
                if (!IsIconic(window)) InvalidateRect(window, nullptr, FALSE);
            }
        }
    }
}

} // namespace oneui::win32
