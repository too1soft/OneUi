
// NVIDIA's OpenGL driver ("Threaded optimization: Auto") takes a census of
// the process's threads the first time a thread starts once it has begun
// observing a context, which happens a couple of presented frames in. The
// census walks a system-wide CreateToolhelp32Snapshot list on a driver thread
// while every GL call waits for it: 60-200 ms on a machine running many
// processes. Left alone it lands on the first user action that starts a
// thread; in iShell that is the first terminal split, which then drops
// several animation frames. Starting a no-op thread during the first frames
// moves the census into startup and glFinish absorbs it there. The census
// runs once per process, so later checkpoints only cost a thread start and an
// idle finish, and they keep the fix working if a slower machine needs more
// frames before the driver starts observing.
void absorbDriverThreadCensus() {
    static int presentedFrames = 0;
    constexpr int kCheckpoints[] = {2, 3, 4, 6, 8, 12, 16};
    if (presentedFrames > kCheckpoints[std::size(kCheckpoints) - 1]) {
        return;
    }
    ++presentedFrames;
    if (std::find(std::begin(kCheckpoints), std::end(kCheckpoints), presentedFrames) == std::end(kCheckpoints)) {
        return;
    }
    std::thread([] {}).join();
    glFinish();
}


using rendering::PrimitivePaintTrace;
using rendering::g_primitivePaintTrace;

constexpr UINT kOneUiRunPostedCallbacks = WM_APP + 1;
constexpr UINT kOneUiFinishWindowStatePaint = WM_APP + 2;
constexpr UINT kOneUiDeferredFullPaint = WM_APP + 3;
constexpr UINT kOneUiFlushInvalidations = WM_APP + 4;
constexpr UINT kOneUiDpiChanged = 0x02E0;
constexpr UINT_PTR kOneUiAnimationTimer = 0x4f10;
constexpr UINT_PTR kOneUiInteractivePaintTimer = 0x4f11;
constexpr UINT_PTR kOneUiTooltipTimer = 0x4f12;
constexpr UINT kOneUiTooltipDelayMs = 420;
constexpr UINT kOneUiAnimationFrameIntervalMs = USER_TIMER_MINIMUM;
constexpr UINT kOneUiInteractivePaintIntervalMs = 8;
constexpr int kPaintSurfaceAlignment = 128;
constexpr int kPaintSurfaceGrowthPadding = 256;
constexpr float kDefaultDpi = 96.0f;
constexpr int kProcessPerMonitorDpiAware = 2;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

using WglSwapIntervalExtProc = BOOL(WINAPI*)(int);

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 reinterpret_cast<HANDLE>(-4)
#endif

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
using SetProcessDPIAwareFn = BOOL(WINAPI*)();
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

float scaleFromDpi(UINT dpi) {
    return oneui::scaleFromDpiValue(dpi);
}

UINT systemDpi() {
    return win32::systemDpi();
}

UINT dpiForMonitor(HMONITOR monitor) {
    return win32::monitorDpi(monitor);
}

float dpiScaleForWindowHandle(HWND hwnd) {
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
    // Test builds may render an application at deterministic DPI without
    // changing the developer machine's display settings. Production builds do
    // not compile this override.
    if (const char* value = std::getenv("ONEUI_TEST_DPI_SCALE")) {
        char* end = nullptr;
        const float forced = std::strtof(value, &end);
        if (end != value && *end == '\0' && forced >= 0.5f && forced <= 4.0f) {
            return forced;
        }
    }
#endif
    if (hwnd) {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        auto getDpiForWindow = user32
            ? reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"))
            : nullptr;
        if (getDpiForWindow) {
            const UINT dpi = getDpiForWindow(hwnd);
            if (dpi != 0) {
                return scaleFromDpi(dpi);
            }
        }

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (monitor) {
            return scaleFromDpi(dpiForMonitor(monitor));
        }
    }
    return scaleFromDpi(systemDpi());
}

void ensureProcessDpiAwareness() {
    static std::once_flag once;
    std::call_once(once, [] {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) {
            auto setProcessDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
                GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
            if (setProcessDpiAwarenessContext &&
                setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                return;
            }
        }

        HMODULE shcore = win32::loadSystemLibrary(L"Shcore.dll");
        if (shcore) {
            auto setProcessDpiAwareness = reinterpret_cast<SetProcessDpiAwarenessFn>(
                GetProcAddress(shcore, "SetProcessDpiAwareness"));
            const bool ok = setProcessDpiAwareness &&
                SUCCEEDED(setProcessDpiAwareness(kProcessPerMonitorDpiAware));
            FreeLibrary(shcore);
            if (ok) {
                return;
            }
        }

        if (user32) {
            auto setProcessDPIAware = reinterpret_cast<SetProcessDPIAwareFn>(
                GetProcAddress(user32, "SetProcessDPIAware"));
            if (setProcessDPIAware) {
                setProcessDPIAware();
            }
        }
    });
}

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

HCURSOR cursorForKind(CursorKind kind) {
    static HCURSOR arrow = LoadCursor(nullptr, IDC_ARROW);
    static HCURSOR hand = LoadCursor(nullptr, IDC_HAND);
    static HCURSOR text = LoadCursor(nullptr, IDC_IBEAM);
    static HCURSOR cross = LoadCursor(nullptr, IDC_CROSS);
    static HCURSOR grab = LoadCursor(nullptr, IDC_SIZEALL);
    static HCURSOR resizeHorizontal = LoadCursor(nullptr, IDC_SIZEWE);
    static HCURSOR resizeVertical = LoadCursor(nullptr, IDC_SIZENS);

    switch (kind) {
    case CursorKind::Hidden:
        return nullptr;
    case CursorKind::Pointer:
        return hand;
    case CursorKind::Text:
        return text;
    case CursorKind::Crosshair:
        return cross;
    case CursorKind::Grab:
        return grab;
    case CursorKind::ResizeHorizontal:
        return resizeHorizontal;
    case CursorKind::ResizeVertical:
        return resizeVertical;
    case CursorKind::ResizeNorthWestSouthEast:
        return LoadCursor(nullptr, IDC_SIZENWSE);
    case CursorKind::ResizeNorthEastSouthWest:
        return LoadCursor(nullptr, IDC_SIZENESW);
    case CursorKind::Default:
    default:
        return arrow;
    }
}
