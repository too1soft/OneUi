#include "window_win32_gpu_warmup.h"

#include "gl_flush_trace_win32.h"
#include "internal/scroll_trace.h"
#include "platform/shared/skia_gl_setup.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"

#include <GL/gl.h>

#include <atomic>
#include <cstring>
#include <cwchar>
#include <exception>
#include <system_error>

namespace oneui::win32 {
namespace {

constexpr wchar_t kStartupWindowClass[] = L"OneUI.NvidiaOpenGlWarmup";
std::atomic_bool startupClaimed{false};

bool nvidiaDisplayOrigin(HWND window, POINT& origin) {
    MONITORINFOEXW monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
        return false;
    }
    for (DWORD index = 0;; ++index) {
        DISPLAY_DEVICEW display{};
        display.cb = sizeof(display);
        if (!EnumDisplayDevicesW(nullptr, index, &display, 0)) return false;
        if (std::wcscmp(display.DeviceName, monitor.szDevice) == 0) {
            origin = {monitor.rcMonitor.left, monitor.rcMonitor.top};
            return std::wcsstr(display.DeviceID, L"VEN_10DE") != nullptr;
        }
    }
}

template <typename Action>
bool traceCensusStep(bool trace, const char* phase, const Action& action) {
    if (!trace) return action();
    const double started = internal::scrollTraceNowMs();
    ULONG64 before = 0;
    ULONG64 after = 0;
    const BOOL counted = QueryThreadCycleTime(GetCurrentThread(), &before);
    const bool success = action();
    const BOOL measured = QueryThreadCycleTime(GetCurrentThread(), &after);
    internal::writeScrollTrace(internal::ScrollTraceEvent{
        "win32", phase, 0, 0, 0, 0, 0, 0, 0, internal::scrollTraceNowMs() - started,
        success ? 1.0 : 0.0, counted && measured ? static_cast<double>(after - before) : 0.0});
    return success;
}

struct StartupSurface {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    ATOM windowClass = 0;
    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC context = nullptr;

    ~StartupSurface() {
        if (context) {
            if (wglGetCurrentContext() == context) wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(context);
        }
        if (dc) ReleaseDC(window, dc);
        if (window) DestroyWindow(window);
        if (windowClass) UnregisterClassW(kStartupWindowClass, instance);
    }

    bool open(POINT origin) {
        WNDCLASSW options{};
        options.style = CS_OWNDC;
        options.lpfnWndProc = DefWindowProcW;
        options.hInstance = instance;
        options.lpszClassName = kStartupWindowClass;
        windowClass = RegisterClassW(&options);
        if (!windowClass) return false;
        window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kStartupWindowClass, L"",
            WS_POPUP, origin.x, origin.y, 32, 32, nullptr, nullptr, instance, nullptr);
        if (!window) return false;
        dc = GetDC(window);
        if (!dc) return false;
        const auto format = openGlWindowPixelFormat();
        const int pixelFormat = ChoosePixelFormat(dc, &format);
        if (!pixelFormat || !SetPixelFormat(dc, pixelFormat, &format)) return false;
        context = wglCreateContext(dc);
        return context && wglMakeCurrent(dc, context);
    }
};

bool warmColorQuadProgram() {
    auto* cache = rendering::shaderCache();
    if (!cache) return false;

    GrContextOptions options;
    options.fPersistentCache = cache;
    auto context = GrDirectContexts::MakeGL(
        internal::makeWin32GaneshGlInterface(internal::win32GlNvidiaVendor()), options);
    if (!context) return false;
    auto surface = SkSurfaces::RenderTarget(context.get(), skgpu::Budgeted::kYes,
        SkImageInfo::Make(32, 32, kBGRA_8888_SkColorType, kPremul_SkAlphaType),
        0, kTopLeft_GrSurfaceOrigin, nullptr);
    if (!surface) return false;

    // Fractional fill edges exercise the coverage program used by the
    // regular window canvas at fractional layout positions.
    auto* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SkColorSetARGB(200, 110, 160, 200));
    canvas->drawRect(SkRect::MakeLTRB(2.25f, 3.5f, 16.75f, 21.25f), paint);
    context->flush();
    return context->submit(GrSyncCpu::kNo);
}

void warmColorQuadInBackground(bool trace) {
    const double started = trace ? internal::scrollTraceNowMs() : 0.0;
    bool warmed = false;
    try {
        warmed = warmColorQuadProgram();
    } catch (const std::exception&) {
        // Cache or scratch-surface failures cannot affect the live window.
    }
    if (trace) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "win32", "gpu_color_quad_warmup", 0, 0, 0, 0, 0, 0, 0,
            internal::scrollTraceNowMs() - started, warmed ? 1.0 : 0.0, 0});
    }
}

} // namespace

bool nvidiaWindowDisplayOrigin(HWND window, POINT& origin) {
    return nvidiaDisplayOrigin(window, origin);
}

bool warmNvidiaDriverThreadCensus(HDC dc) {
    const bool tracing = internal::scrollTraceEnabled();
    if (!traceCensusStep(tracing, "wgl_census_vendor", internal::win32GlNvidiaVendor)) return false;
    if (!traceCensusStep(tracing, "wgl_census_swaps", [dc] {
        for (int i = 0; i < 2; ++i) {
            glClear(GL_COLOR_BUFFER_BIT);
            if (!SwapBuffers(dc)) return false;
        }
        return true;
    })) return false;
    // Let the driver observe the scratch context before starting the thread
    // that triggers process-wide enumeration. Finish on the worker context.
    if (!traceCensusStep(tracing, "wgl_census_thread", [] {
        try {
            std::thread([] {}).join();
        } catch (const std::system_error&) {
            return false;
        }
        return true;
    })) return false;
    return traceCensusStep(tracing, "wgl_census_finish", [] {
        glFinish();
        return true;
    });
}

PIXELFORMATDESCRIPTOR openGlWindowPixelFormat() noexcept {
    PIXELFORMATDESCRIPTOR format{};
    format.nSize = sizeof(format);
    format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA;
    format.cColorBits = 32;
    format.iLayerType = PFD_MAIN_PLANE;
    return format;
}

NvidiaOpenGlWarmup::~NvidiaOpenGlWarmup() {
    if (worker_.joinable()) worker_.join();
}

bool NvidiaOpenGlWarmup::censusReady() const noexcept {
    return censusReady_.load(std::memory_order_relaxed);
}

void NvidiaOpenGlWarmup::start(HWND window) {
    if (startupClaimed.load(std::memory_order_relaxed)) return;
    POINT origin{};
    if (!nvidiaDisplayOrigin(window, origin)) return;
    if (startupClaimed.exchange(true, std::memory_order_relaxed)) return;
    try {
        worker_ = std::thread([this, origin] {
            const bool trace = internal::scrollTraceEnabled();
            const double started = trace ? internal::scrollTraceNowMs() : 0.0;
            StartupSurface surface;
            const bool warmed = surface.open(origin) && warmNvidiaDriverThreadCensus(surface.dc);
            censusReady_.store(warmed, std::memory_order_relaxed);
            if (trace) {
                internal::writeScrollTrace(internal::ScrollTraceEvent{
                    "win32", "gpu_startup_warmup", 0, 0, 0, 0, 0, 0, 0,
                    internal::scrollTraceNowMs() - started, warmed ? 1.0 : 0.0, 0});
            }
            if (warmed) warmColorQuadInBackground(trace);
        });
    } catch (const std::system_error&) {
        startupClaimed.store(false, std::memory_order_relaxed);
    }
}

} // namespace oneui::win32
