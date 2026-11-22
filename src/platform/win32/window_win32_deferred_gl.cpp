#include "window_win32_deferred_gl.h"

#include "gl_flush_trace_win32.h"
#include "window_win32_gpu_warmup.h"
#include "platform/shared/skia_gl_setup.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"

#include <GL/gl.h>
#include <exception>
#include <utility>

namespace oneui::win32 {
namespace {

constexpr wchar_t scratchClass[] = L"OneUI.DeferredStartupGl";
std::atomic_bool claimed{false};

class ScratchWindow {
public:
    HWND hwnd = nullptr;
    HDC dc = nullptr;
    HGLRC context = nullptr;

    ~ScratchWindow() {
        if (context) {
            if (wglGetCurrentContext() == context) wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(context);
        }
        if (dc) ReleaseDC(hwnd, dc);
        if (hwnd) DestroyWindow(hwnd);
    }

    bool open(POINT origin) {
        WNDCLASSW info{};
        info.style = CS_OWNDC;
        info.lpfnWndProc = DefWindowProcW;
        info.hInstance = GetModuleHandleW(nullptr);
        info.lpszClassName = scratchClass;
        if (!RegisterClassW(&info) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, scratchClass, L"",
                               WS_POPUP, origin.x, origin.y, 32, 32, nullptr, nullptr,
                               info.hInstance, nullptr);
        dc = hwnd ? GetDC(hwnd) : nullptr;
        return dc != nullptr;
    }
};

void trace(const char* stage, double started, double detailA = 0.0) {
    if (started < 0.0) return;
    internal::writeScrollTrace(internal::ScrollTraceEvent{
        "win32", stage, 0, 0, 0, 0, 0, 0, 0,
        internal::scrollTraceNowMs() - started, detailA, 0});
}

} // namespace

PreparedStartupGl::~PreparedStartupGl() {
    if (ganesh) ganesh->abandonContext();
    ganesh.reset();
    if (context) wglDeleteContext(context);
}

DeferredStartupGl::~DeferredStartupGl() {
    cancel();
    if (worker_.joinable()) worker_.join();
}

bool DeferredStartupGl::arm(HWND hwnd) {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_DEFER_OPENGL", value, 2) == 1 && value[0] == L'1';
    }();
    if (pending_) return true;
    if (!enabled || !hwnd || claimed.load(std::memory_order_relaxed)) return false;
    POINT origin{};
    if (!nvidiaWindowDisplayOrigin(hwnd, origin) || claimed.exchange(true, std::memory_order_relaxed))
        return false;
    std::lock_guard lock(mutex_);
    target_ = hwnd;
    pending_ = true;
    capture_.store(true, std::memory_order_relaxed);
    return true;
}

void DeferredStartupGl::submitPicture(sk_sp<SkPicture> picture, int width, int height) {
    if (!picture || !shouldCapture()) return;
    HWND hwnd = nullptr;
    {
        std::lock_guard lock(mutex_);
        if (!target_ || !capture_.load(std::memory_order_relaxed)) return;
        scene_ = {std::move(picture), width, height};
        if (++captures_ == 3) capture_.store(false, std::memory_order_relaxed);
        hwnd = target_;
    }
    if (worker_.joinable()) return;
    const auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    POINT origin{};
    if (!monitor || !nvidiaWindowDisplayOrigin(hwnd, origin)) {
        finish(nullptr);
        return;
    }
    try {
        worker_ = std::thread([this, origin, monitor] {
            try {
                finish(prepare(origin, monitor));
            } catch (const std::exception&) {
                finish(nullptr);
            }
        });
    } catch (const std::system_error&) {
        finish(nullptr);
    }
}

std::unique_ptr<PreparedStartupGl> DeferredStartupGl::prepare(POINT origin, HMONITOR monitor) {
    const bool tracing = internal::scrollTraceEnabled();
    const double started = tracing ? internal::scrollTraceNowMs() : -1.0;
    ScratchWindow scratch;
    if (!scratch.open(origin)) return nullptr;
    const auto pfd = openGlWindowPixelFormat();
    const int format = ChoosePixelFormat(scratch.dc, &pfd);
    PIXELFORMATDESCRIPTOR descriptor{};
    if (!format || !DescribePixelFormat(scratch.dc, format, sizeof(descriptor), &descriptor) ||
        (descriptor.dwFlags & PFD_GENERIC_FORMAT) || !SetPixelFormat(scratch.dc, format, &pfd)) {
        return nullptr;
    }
    scratch.context = wglCreateContext(scratch.dc);
    if (!scratch.context || !wglMakeCurrent(scratch.dc, scratch.context)) return nullptr;
    const double census = tracing ? internal::scrollTraceNowMs() : -1.0;
    const bool censusWarmed = warmNvidiaDriverThreadCensus(scratch.dc);
    trace("deferred_gl_worker_census", census, censusWarmed ? 1.0 : 0.0);
    if (!censusWarmed) return nullptr;
    GrContextOptions options;
    options.fPersistentCache = rendering::shaderCache();
    auto ganesh = internal::win32GlTraceEnabled()
        ? GrDirectContexts::MakeGL(internal::makeWin32GlTraceInterface(), options)
        : GrDirectContexts::MakeGL(options);
    if (!ganesh) return nullptr;
    trace("deferred_gl_worker_context", started);

    Scene current;
    {
        std::lock_guard lock(mutex_);
        if (!target_) return nullptr;
        capture_.store(false, std::memory_order_relaxed);
        current = std::move(scene_);
    }
    if (!current.picture) return nullptr;
    const double warming = tracing ? internal::scrollTraceNowMs() : -1.0;
    {
        const auto info = SkImageInfo::Make(current.width, current.height,
                                            kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        auto retained = SkSurfaces::RenderTarget(ganesh.get(), skgpu::Budgeted::kYes, info,
                                                 0, kTopLeft_GrSurfaceOrigin, nullptr);
        auto destination = SkSurfaces::RenderTarget(ganesh.get(), skgpu::Budgeted::kYes, info,
                                                    0, kBottomLeft_GrSurfaceOrigin, nullptr);
        if (!retained || !destination) return nullptr;
        retained->getCanvas()->drawPicture(current.picture);
        destination->getCanvas()->clear(SK_ColorBLACK);
        retained->draw(destination->getCanvas(), 0, 0);
        ganesh->flush();
        // Drain the scratch surface before handing the context to the UI.
        // Otherwise the first live submit can wait for the warmup workload.
        if (!ganesh->submit(GrSyncCpu::kYes)) return nullptr;
    }
    trace("deferred_gl_worker_prewarm", warming);
    auto result = std::make_unique<PreparedStartupGl>();
    if (!wglMakeCurrent(nullptr, nullptr)) {
        ganesh->abandonContext();
        return nullptr;
    }
    result->monitor = monitor;
    result->format = format;
    result->descriptor = descriptor;
    result->context = std::exchange(scratch.context, nullptr);
    result->ganesh = std::move(ganesh);
    return result;
}

void DeferredStartupGl::finish(std::unique_ptr<PreparedStartupGl> result) {
    capture_.store(false, std::memory_order_relaxed);
    std::lock_guard lock(mutex_);
    if (!target_) return;
    ready_ = std::move(result);
    PostMessageW(target_, deferredGlReadyMessage, 0, 0);
}

std::unique_ptr<PreparedStartupGl> DeferredStartupGl::take() {
    if (!pending_) return nullptr;
    pending_ = false;
    std::lock_guard lock(mutex_);
    return std::move(ready_);
}

void DeferredStartupGl::cancel() {
    pending_ = false;
    capture_.store(false, std::memory_order_relaxed);
    std::lock_guard lock(mutex_);
    target_ = nullptr;
}

} // namespace oneui::win32
