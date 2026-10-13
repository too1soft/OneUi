#include "internal/view_profile.h"
#include "window_event_loop_win32.h"
#include "internal/frame_profile.h"
#include "internal/developer_tools.h"
#include "internal/runtime_config.h"
#include "internal/application_menu.h"
#include "internal/unicode.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "oneui/platform/window.h"
#include "oneui/platform/dpi.h"

#include "oneui/color.h"
#include "oneui/animation.h"
#include "oneui/view.h"
#include "oneui/controls/window_title_bar.h"
#include "internal/scroll_trace.h"
#include "platform/shared/skia_canvas.h"
#include "platform/win32/compat_win32.h"
#include "platform/win32/window_shadow.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <imm.h>
#include <shellapi.h>
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
#include <wincodec.h>
#endif

#include <GL/gl.h>
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "platform/shared/skia_gl_setup.h"
#include "internal/workspace_frame_probe.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"
#include "include/gpu/GpuTypes.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkMilestone.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#if SK_MILESTONE >= 150
#include "include/core/SkPathBuilder.h"
#endif
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkGradientShader.h"
#include "include/effects/SkImageFilters.h"
#include "include/ports/SkTypeface_win.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "platform/win32/accessibility_win32.h"

namespace oneui {
namespace {
#include "window_win32_platform.inc"
class Win32Window final : public Window {
public:
#include "window_win32_lifecycle.inc"
#include "window_win32_tray.inc"
#include "window_win32_frame_api.inc"
#include "window_win32_placement.inc"
#include "window_win32_decoration.inc"
#include "window_win32_native_host.inc"
#include "window_win32_messages.inc"
#include "window_win32_state.inc"
#include "window_win32_scheduling.inc"
#include "window_win32_paint.inc"
#include "window_win32_diagnostics.inc"
#include "window_win32_pointer.inc"
#include "window_win32_keyboard.inc"
#include "window_win32_ime.inc"
#include "window_win32_pointer_dispatch.inc"
#include "window_win32_gpu.inc"
    const internal::RuntimeConfig runtimeConfig_ = internal::RuntimeConfig::fromEnvironment();
#if ONEUI_DEVELOPER_TOOLS
    std::shared_ptr<internal::DeveloperRecorder> developerTools_;
    int developerExportStatus_=0;
#endif
    RendererInfo rendererInfo_;
    HWND hwnd_ = nullptr;
    const void* imeIdentity_ = nullptr;
    std::uint64_t imeSession_ = 0;
    bool imeActive_ = false;
    bool suppressKeyText_ = false;
    HANDLE animationFrameTimer_ = nullptr;
    IAccessible* accessibilityProvider_=nullptr;
    HRESULT accessibilityComResult_=E_FAIL;
    WindowOptions options_;
    float dpiScale_ = 1.0f;
    float contentScale_ = 1.0f;
    // 无边框窗口的拖拽命中区（逻辑像素）：标题栏高度内、且不在右侧预留区，视为可拖拽 caption。
    // 客户端可按自身标题栏与窗口按钮/账号按钮的实际布局配置。
    float titleBarHeightLogical_ = 34.0f;
    float titleButtonReservedWidthLogical_ = 132.0f;
    float titleBarInteractiveLeadingWidthLogical_ = -1.0f;
    float titleBarInteractiveTrailingWidthLogical_ = -1.0f;
    ClientSizeChangedHandler clientSizeChangedHandler_;
    ActivationChangedHandler activationChangedHandler_;
    FileDropHandler fileDropHandler_;
    Size maximumClientSize_{};
    Size minimumClientSize_{};
    bool clientSizeChangedFramePending_ = false;
    bool windowActive_ = false;
    HGLRC glContext_ = nullptr;
    HDC glDC_ = nullptr;
    sk_sp<GrDirectContext> grContext_;
    bool gpuInitializationAttempted_ = false;
    bool gpuAvailable_ = false;
    bool nvidiaDriver_ = false;
    bool imePositionAfterPaint_ = false;
    bool invalidationPending_ = false;
    bool invalidateAll_ = false;
    RECT pendingInvalidation_{};
    bool imeRectSent_ = false;
    RECT lastImeRect_{};
    bool swapIntervalEnabled_ = false;
    std::shared_ptr<Widget> content_;
    RawKeyHandler rawKeyHandler_;
    std::wstring defaultFontFamily_;
    wchar_t pendingHighSurrogate_ = 0;
    std::array<bool, 256> trackedKeyState_{};
    std::atomic_bool acceptingPostedCallbacks_{true};
    std::shared_ptr<std::atomic_bool> callbackWindowAlive_ = std::make_shared<std::atomic_bool>(true);
    std::mutex postedCallbacksMutex_;
    std::queue<std::function<void()>> postedCallbacks_;
    std::mutex animationCallbacksMutex_;
    std::queue<std::function<void(double)>> animationCallbacks_;
    WINDOWPLACEMENT savedPlacement_{sizeof(WINDOWPLACEMENT)};
    RECT savedBorderlessRect_{};
    RECT savedFullscreenRect_{};
    DWORD savedStyle_ = 0;
    DWORD savedExStyle_ = 0;
    bool fullscreenApplied_ = false;
    bool borderlessMaximized_ = false;
    RECT lastNormalRect_{};
    bool hasNormalPlacement_ = false;
    bool lastKnownMaximized_ = false;
    bool placementStatePending_ = false;
    bool pendingMaximized_ = false;
    float cornerRadiusLogical_ = 0.0f;
    bool closeToTray_ = false;
    bool trayEnabled_ = false;
    std::vector<TrayMenuItem> trayMenu_;
    DWORD menuUiThread_=GetCurrentThreadId();
    HICON trayCustomIcon_ = nullptr;
    std::shared_ptr<internal::ApplicationMenuHost> menuHost_;
    std::shared_ptr<Widget> applicationContent_;
    std::function<void(int)> onMenuAction_;
    std::wstring trayTooltip_;
    std::function<void(int)> onTrayAction_;
    std::function<void(bool)> onCloseRequested_;
    bool internalTrayVisible_ = false;
    UINT taskbarCreatedMessage_ = 0;
    static constexpr UINT kInternalTrayIconId = 0x5743; // Version-4 shell callbacks carry a 16-bit ID.
    // Win10 圆角回退（SetWindowRgn）会丢掉 DWM 柔和投影，用一个分层伴随窗口在主窗
    // 圆角轮廓外画一圈抗锯齿柔光投影补回来。Win11 走 DWM 圆角自带投影，不用它。
    HWND shadowHwnd_ = nullptr;
    bool shadowActive_ = false; // 当前是否处于“需要伴随投影”的状态（Win10 圆角且未最大化）
    int shadowBuiltW_ = 0;      // 已构建投影位图对应的主窗尺寸，尺寸不变则移动时只重定位不重绘
    int shadowBuiltH_ = 0;
    float shadowBuiltRadius_ = 0;
    float shadowBuiltScale_ = 0;
    bool mouseLeaveTracking_ = false;
    bool animationTimerActive_ = false;
    bool animationFramePending_ = false;
    double lastAnimationFrameMs_ = 0.0;
    HMONITOR animationFrameMonitor_ = nullptr;
    double animationFrameIntervalMs_ = 1000.0 / 60.0;
    bool contentAnimationFramePending_ = false;
    bool interactivePaintTimerActive_ = false;
    bool applyingWindowState_ = false;
    bool windowStatePaintPending_ = false;
    bool deferredFullPaintPending_ = false;
    bool interactiveResizeActive_ = false;
    bool renderTraceEnabled_ = false;
    std::wstring renderTraceFilePath_;
    double scrollTraceLastWheelMessageMs_ = 0.0;
    double scrollTraceLastAnimationFrameMs_ = 0.0;
    double scrollTraceLastPaintMs_ = 0.0;
    CursorKind lastCursorKind_ = CursorKind::Default;
    Point lastCursorPoint_{};
    bool hasLastCursorPoint_ = false;
    std::wstring hoveredTooltip_;
    Point tooltipPoint_{};
    bool tooltipVisible_ = false;
    sk_sp<SkSurface> paintSurface_;
    sk_sp<SkSurface> windowSurface_;
    int paintSurfaceWidth_ = 0;
    int paintSurfaceHeight_ = 0;
    int traceLastWidth_ = 0;
    int traceLastHeight_ = 0;
    std::uint64_t traceResizeMessages_ = 0;
    std::uint64_t traceInteractiveResizeMessages_ = 0;
    std::uint64_t traceMaximizeMessages_ = 0;
    std::uint64_t traceRestoreMessages_ = 0;
    std::uint64_t tracePaints_ = 0;
    std::uint64_t traceFullPaints_ = 0;
    std::uint64_t tracePartialPaints_ = 0;
    std::uint64_t traceFrameIntervals_ = 0;
    std::uint64_t traceSurfaceAllocations_ = 0;
    std::uint64_t traceTextCalls_ = 0;
    std::uint64_t traceTextMeasureCalls_ = 0;
    std::uint64_t traceShadowCalls_ = 0;
    std::uint64_t traceGradientCalls_ = 0;
    double traceLastPrintMs_ = 0.0;
    double traceLastPaintMs_ = 0.0;
    double traceFrameIntervalMs_ = 0.0;
    double traceMinFrameIntervalMs_ = 0.0;
    double traceMaxFrameIntervalMs_ = 0.0;
    double tracePaintMs_ = 0.0;
    double traceContentPaintMs_ = 0.0;
    double traceBlitMs_ = 0.0;
    double traceSurfaceAllocMs_ = 0.0;
    double traceTextMs_ = 0.0;
    double traceTextMeasureMs_ = 0.0;
    double traceShadowMs_ = 0.0;
    double traceGradientMs_ = 0.0;
    std::array<double, 4> traceStageMs_{};
    double traceSubmitMs_ = 0;
    std::uint64_t tracePaintInvalidations_ = 0;
    std::vector<std::uint32_t> blitScratch_;
};

} // namespace

std::unique_ptr<Window> Window::create(std::wstring title, int width, int height) {
    WindowOptions options;
    options.title = std::move(title);
    options.width = width;
    options.height = height;
    return Window::create(std::move(options));
}

std::unique_ptr<Window> Window::create(WindowOptions options) {
    const bool visible = options.visible;
    auto window = std::make_unique<Win32Window>(std::move(options));
    if (visible) {
        window->show();
    }
    return window;
}

} // namespace oneui
#include "oneui/localization.h"
