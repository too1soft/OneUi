#include "oneui/platform/window.h"

#include "oneui/color.h"
#include "oneui/view.h"
#include "internal/scroll_trace.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <imm.h>

#include <GL/gl.h>
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
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
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "include/ports/SkTypeface_win.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdio>
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
#include <tuple>
#include <utility>
#include <vector>

namespace oneui {
namespace {

SkColor toSkColor(Color color) {
    return SkColorSetARGB(color.a, color.r, color.g, color.b);
}

SkRect toSkRect(Rect rect) {
    return SkRect::MakeXYWH(rect.x, rect.y, rect.width, rect.height);
}

constexpr UINT kOneUiRunPostedCallbacks = WM_APP + 1;
constexpr UINT kOneUiFinishWindowStatePaint = WM_APP + 2;
constexpr UINT kOneUiDeferredFullPaint = WM_APP + 3;
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

std::atomic<int> g_liveWindowCount{0};

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
using SetProcessDPIAwareFn = BOOL(WINAPI*)();
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);

float scaleFromDpi(UINT dpi) {
    if (dpi == 0) {
        return 1.0f;
    }
    return std::max(0.25f, static_cast<float>(dpi) / kDefaultDpi);
}

UINT systemDpi() {
    HDC dc = GetDC(nullptr);
    if (!dc) {
        return static_cast<UINT>(kDefaultDpi);
    }
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : static_cast<UINT>(kDefaultDpi);
}

UINT dpiForMonitor(HMONITOR monitor) {
    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (shcore) {
        auto getDpiForMonitor = reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"));
        UINT dpiX = static_cast<UINT>(kDefaultDpi);
        UINT dpiY = static_cast<UINT>(kDefaultDpi);
        const bool ok = getDpiForMonitor && SUCCEEDED(getDpiForMonitor(monitor, 0, &dpiX, &dpiY));
        FreeLibrary(shcore);
        if (ok && dpiX != 0) {
            return dpiX;
        }
    }
    return systemDpi();
}

float dpiScaleForWindowHandle(HWND hwnd) {
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

        HMODULE shcore = LoadLibraryW(L"Shcore.dll");
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

struct PrimitivePaintTrace {
    std::uint64_t textCalls = 0;
    std::uint64_t textMeasureCalls = 0;
    std::uint64_t shadowCalls = 0;
    std::uint64_t gradientCalls = 0;
    double textMs = 0.0;
    double textMeasureMs = 0.0;
    double shadowMs = 0.0;
    double gradientMs = 0.0;
};

thread_local PrimitivePaintTrace g_primitivePaintTrace;

struct GradientShaderKey {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int angle = 0;
    SkColor start = SK_ColorTRANSPARENT;
    SkColor end = SK_ColorTRANSPARENT;

    bool operator<(const GradientShaderKey& other) const {
        return std::tie(x, y, width, height, angle, start, end) <
            std::tie(other.x, other.y, other.width, other.height, other.angle, other.start, other.end);
    }
};

struct GradientImageKey {
    int width = 0;
    int height = 0;
    int radius = 0;
    int angle = 0;
    SkColor start = SK_ColorTRANSPARENT;
    SkColor end = SK_ColorTRANSPARENT;

    bool operator<(const GradientImageKey& other) const {
        return std::tie(width, height, radius, angle, start, end) <
            std::tie(other.width, other.height, other.radius, other.angle, other.start, other.end);
    }
};

struct ShadowImageKey {
    int width = 0;
    int height = 0;
    int radius = 0;
    int blur = 0;
    int spread = 0;
    SkColor color = SK_ColorTRANSPARENT;

    bool operator<(const ShadowImageKey& other) const {
        return std::tie(width, height, radius, blur, spread, color) <
            std::tie(other.width, other.height, other.radius, other.blur, other.spread, other.color);
    }
};

struct ShadowImageEntry {
    sk_sp<SkImage> image;
    int pad = 0;
};

struct TextBlobKey {
    std::wstring text;
    int size = 0;
    int weight = 0;
    TextFontFamily family = TextFontFamily::Default;
    std::wstring familyName;

    bool operator<(const TextBlobKey& other) const {
        return std::tie(text, size, weight, family, familyName) <
            std::tie(other.text, other.size, other.weight, other.family, other.familyName);
    }
};

struct TextBlobRun {
    sk_sp<SkTextBlob> blob;
    float x = 0.0f;
};

struct TextBlobEntry {
    std::vector<TextBlobRun> runs;
    SkRect bounds = SkRect::MakeEmpty();
    SkFontMetrics metrics{};
    float advanceWidth = 0.0f;
};

bool renderTraceEnabled() {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(L"ONEUI_RENDER_TRACE", value, static_cast<DWORD>(std::size(value)));
    return length > 0 && value[0] != L'0';
}

bool gpuRenderingEnabled() {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(L"ONEUI_ENABLE_GPU", value, static_cast<DWORD>(std::size(value)));
    return length == 0 || value[0] != L'0';
}

std::wstring renderTraceFilePath() {
    wchar_t value[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"ONEUI_RENDER_TRACE_FILE", value, static_cast<DWORD>(std::size(value)));
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return std::wstring(value, value + length);
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
    case CursorKind::Default:
    default:
        return arrow;
    }
}

LRESULT CALLBACK clipboardOwnerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

HWND clipboardOwnerWindow() {
    static std::once_flag once;
    static HWND owner = nullptr;
    std::call_once(once, [] {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        const wchar_t* className = L"OneUIClipboardOwner";

        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = &clipboardOwnerProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        RegisterClassW(&windowClass);

        owner = CreateWindowExW(
            0,
            className,
            L"",
            WS_OVERLAPPED,
            0,
            0,
            0,
            0,
            nullptr,
            nullptr,
            instance,
            nullptr);
    });
    return owner;
}

class ClipboardGuard {
public:
    ClipboardGuard() {
        constexpr int kMaxAttempts = 8;
        constexpr DWORD kRetryDelayMs = 5;
        const HWND owner = clipboardOwnerWindow();

        // Clipboard ownership is process-global, so brief contention is expected.
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            if (OpenClipboard(owner) != FALSE) {
                open_ = true;
                return;
            }
            if (attempt + 1 < kMaxAttempts) {
                Sleep(kRetryDelayMs);
            }
        }
    }

    ~ClipboardGuard() {
        if (open_) {
            CloseClipboard();
        }
    }

    bool isOpen() const {
        return open_;
    }

private:
    bool open_ = false;
};

class SkiaCanvas final : public Canvas {
public:
    explicit SkiaCanvas(
        SkCanvas& canvas,
        const std::wstring* defaultFontFamily = nullptr,
        std::optional<Rect> viewportBounds = std::nullopt)
        : canvas_(canvas)
        , defaultFontFamily_(defaultFontFamily)
        , viewportBounds_(viewportBounds) {}

    void clear(Color color) override {
        canvas_.clear(toSkColor(color));
    }

    void save() override {
        clipStack_.push_back(clipBounds_);
        canvas_.save();
    }

    void restore() override {
        canvas_.restore();
        if (!clipStack_.empty()) {
            clipBounds_ = clipStack_.back();
            clipStack_.pop_back();
        } else {
            clipBounds_.reset();
        }
    }

    void clipRect(Rect rect) override {
        canvas_.clipRect(toSkRect(rect), true);
        if (clipBounds_) {
            const float left = std::max(clipBounds_->x, rect.x);
            const float top = std::max(clipBounds_->y, rect.y);
            const float right = std::min(clipBounds_->x + clipBounds_->width, rect.x + rect.width);
            const float bottom = std::min(clipBounds_->y + clipBounds_->height, rect.y + rect.height);
            clipBounds_ = Rect{left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)};
        } else {
            clipBounds_ = rect;
        }
    }

    std::optional<Rect> clipBounds() const override {
        return clipBounds_;
    }

    std::optional<Rect> viewportBounds() const override {
        return viewportBounds_;
    }

    void fillRect(Rect rect, Color color, float radius) override {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));
        canvas_.drawRRect(SkRRect::MakeRectXY(toSkRect(rect), radius, radius), paint);
    }

    void fillLinearGradient(Rect rect, Color start, Color end, float angleDegrees, float radius) override {
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }

        const double traceStartMs = currentTimeMs();
        if (auto cached = gradientImage(rect.width, rect.height, radius, start, end, angleDegrees)) {
            canvas_.drawImage(cached, rect.x, rect.y);
            ++g_primitivePaintTrace.gradientCalls;
            g_primitivePaintTrace.gradientMs += currentTimeMs() - traceStartMs;
            return;
        }

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setShader(linearGradientShader(rect, start, end, angleDegrees));
        canvas_.drawRRect(SkRRect::MakeRectXY(toSkRect(rect), radius, radius), paint);
        ++g_primitivePaintTrace.gradientCalls;
        g_primitivePaintTrace.gradientMs += currentTimeMs() - traceStartMs;
    }

    void fillRadialGradient(Rect rect, Color center, Color edge, Point centerNorm, float radiusNorm, float radius) override {
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }
        const float shaderRadius = std::max(rect.width, rect.height) * std::max(0.01f, radiusNorm);
        const SkPoint shaderCenter = SkPoint::Make(
            rect.x + rect.width * centerNorm.x,
            rect.y + rect.height * centerNorm.y);
        const SkColor4f colors[2] = {
            SkColor4f::FromColor(toSkColor(center)),
            SkColor4f::FromColor(toSkColor(edge)),
        };
        const SkGradient gradient(SkGradient::Colors(colors, SkTileMode::kClamp), {});
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setShader(SkShaders::RadialGradient(shaderCenter, shaderRadius, gradient));
        canvas_.drawRRect(SkRRect::MakeRectXY(toSkRect(rect), radius, radius), paint);
        ++g_primitivePaintTrace.gradientCalls;
    }

    void strokeRect(Rect rect, Color color, float radius, float width) override {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(width);
        canvas_.drawRRect(SkRRect::MakeRectXY(toSkRect(rect), radius, radius), paint);
    }

    void fillEllipse(Rect rect, Color color) override {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));
        canvas_.drawOval(toSkRect(rect), paint);
    }

    void strokeEllipse(Rect rect, Color color, float width) override {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(width);
        canvas_.drawOval(toSkRect(rect), paint);
    }

    void drawLine(Point from, Point to, Color color, float width) override {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));
        paint.setStrokeWidth(width);
        paint.setStrokeCap(SkPaint::kRound_Cap);
        canvas_.drawLine(from.x, from.y, to.x, to.y, paint);
    }

    void drawBoxShadow(Rect rect, const BoxShadow& shadow, float radius) override {
        if (shadow.color.a == 0 || rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }

        const double traceStartMs = currentTimeMs();
        const float spread = shadow.spreadRadius;
        const Rect shadowRect{
            rect.x + shadow.offset.x - spread,
            rect.y + shadow.offset.y - spread,
            rect.width + spread * 2.0f,
            rect.height + spread * 2.0f};
        if (shadowRect.width <= 0.0f || shadowRect.height <= 0.0f) {
            return;
        }

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(shadow.color));
        const float shadowRadius = std::max(0.0f, radius + spread);
        if (shadow.blurRadius > 0.0f) {
            const auto cached = shadowImage(shadowRect.width, shadowRect.height, shadowRadius, shadow.blurRadius, shadow.spreadRadius, shadow.color);
            if (cached.image) {
                canvas_.drawImage(cached.image, shadowRect.x - static_cast<float>(cached.pad), shadowRect.y - static_cast<float>(cached.pad));
                ++g_primitivePaintTrace.shadowCalls;
                g_primitivePaintTrace.shadowMs += currentTimeMs() - traceStartMs;
                return;
            }
            paint.setMaskFilter(shadowMaskFilter(shadow.blurRadius));
        }

        canvas_.save();
        canvas_.clipRect(toSkRect(Rect{
            shadowRect.x - shadow.blurRadius * 2.0f,
            shadowRect.y - shadow.blurRadius * 2.0f,
            shadowRect.width + shadow.blurRadius * 4.0f,
            shadowRect.height + shadow.blurRadius * 4.0f}));
        canvas_.drawRRect(SkRRect::MakeRectXY(toSkRect(shadowRect), shadowRadius, shadowRadius), paint);
        canvas_.restore();
        ++g_primitivePaintTrace.shadowCalls;
        g_primitivePaintTrace.shadowMs += currentTimeMs() - traceStartMs;
    }

    void drawText(const std::wstring& text, Rect rect, Color color, float size, TextAlign align = TextAlign::Center) override {
        drawTextStyled(text, rect, color, size, align, 400);
    }

    void drawTextStyled(const std::wstring& text, Rect rect, Color color, float size, TextAlign align = TextAlign::Center, int weight = 400) override {
        drawTextStyledWithFont(text, rect, color, size, align, TextFontFamily::Default, weight);
    }

    void drawTextStyledWithFont(
        const std::wstring& text,
        Rect rect,
        Color color,
        float size,
        TextAlign align,
        TextFontFamily family,
        int weight = 400) override {
        drawTextStyledWithNamedFont(
            text, rect, color, size, align, {}, family, weight);
    }

    void drawTextStyledWithNamedFont(
        const std::wstring& text,
        Rect rect,
        Color color,
        float size,
        TextAlign align,
        const std::wstring& familyName,
        TextFontFamily fallbackFamily,
        int weight = 400) override {
        if (text.empty() || rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }

        const double traceStartMs = currentTimeMs();
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));

        const std::wstring& resolvedFamily =
            familyName.empty() && fallbackFamily == TextFontFamily::Default &&
                    defaultFontFamily_ && !defaultFontFamily_->empty()
                ? *defaultFontFamily_
                : familyName;
        const TextBlobEntry& textBlob =
            cachedTextBlob(text, size, fallbackFamily, resolvedFamily, weight);
        if (textBlob.runs.empty()) {
            return;
        }

        float x = rect.x;
        if (align == TextAlign::Center) {
            x = rect.x + (rect.width - textBlob.advanceWidth) / 2.0f - textBlob.bounds.left();
        } else if (align == TextAlign::Right) {
            x = rect.x + rect.width - textBlob.advanceWidth - textBlob.bounds.left();
        }

        const float baseline = rect.y + (rect.height - textBlob.metrics.fDescent - textBlob.metrics.fAscent) / 2.0f;
        canvas_.save();
        canvas_.clipRect(toSkRect(rect));
        for (const auto& run : textBlob.runs) {
            if (run.blob) {
                canvas_.drawTextBlob(run.blob, x + run.x, baseline, paint);
            }
        }
        canvas_.restore();
        ++g_primitivePaintTrace.textCalls;
        g_primitivePaintTrace.textMs += currentTimeMs() - traceStartMs;
    }

    float measureTextWidth(const std::wstring& text, float size, int weight = 400) const override {
        return measureTextWidthWithFont(text, size, TextFontFamily::Default, weight);
    }

    std::vector<float> measureTextPrefixWidths(
        const std::wstring& text,
        float size,
        int weight = 400) const override {
        std::vector<float> widths(text.size() + 1, 0.0f);
        if (text.empty()) {
            return widths;
        }

        const std::wstring familyName = defaultFontFamily_ ? *defaultFontFamily_ : std::wstring{};
        const auto primary = typeface(TextFontFamily::Default, weight, familyName);
        float advance = 0.0f;
        for (std::size_t offset = 0; offset < text.size();) {
            const CodepointSpan span = codepointAt(text, offset);
            const std::size_t length = std::max<std::size_t>(span.length, 1);
            const auto face = typefaceForCodepoint(
                TextFontFamily::Default, weight, familyName, span.codepoint, primary);
            SkFont font(face ? face : primary, size);
            font.setSubpixel(true);
            font.setEdging(SkFont::Edging::kAntiAlias);
            advance += font.measureText(
                text.data() + offset,
                length * sizeof(wchar_t),
                SkTextEncoding::kUTF16);
            for (std::size_t index = 1; index <= length; ++index) {
                widths[offset + index] = index == length ? advance : widths[offset];
            }
            offset += length;
        }
        return widths;
    }

    float measureTextWidthWithFont(
        const std::wstring& text,
        float size,
        TextFontFamily family,
        int weight = 400) const override {
        return measureTextWidthWithNamedFont(text, size, {}, family, weight);
    }

    float measureTextWidthWithNamedFont(
        const std::wstring& text,
        float size,
        const std::wstring& familyName,
        TextFontFamily fallbackFamily,
        int weight = 400) const override {
        if (text.empty()) {
            return 0.0f;
        }
        const double traceStartMs = currentTimeMs();
        const std::wstring& resolvedFamily =
            familyName.empty() && fallbackFamily == TextFontFamily::Default &&
                    defaultFontFamily_ && !defaultFontFamily_->empty()
                ? *defaultFontFamily_
                : familyName;
        const TextBlobEntry& textBlob =
            cachedTextBlob(text, size, fallbackFamily, resolvedFamily, weight);
        ++g_primitivePaintTrace.textMeasureCalls;
        g_primitivePaintTrace.textMeasureMs += currentTimeMs() - traceStartMs;
        return textBlob.advanceWidth;
    }

    void drawPixels(Rect rect, const std::uint8_t* pixels, int width, int height, int stride, CanvasPixelFormat format) override {
        if (!pixels || width <= 0 || height <= 0 || stride <= 0 || rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }

        SkColorType colorType = kBGRA_8888_SkColorType;
        if (format == CanvasPixelFormat::Rgba8888) {
            colorType = kRGBA_8888_SkColorType;
        }

        const SkImageInfo imageInfo = SkImageInfo::Make(width, height, colorType, kPremul_SkAlphaType);
        const SkPixmap pixmap(imageInfo, pixels, static_cast<size_t>(stride));
        sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(pixmap);
        if (!image) {
            return;
        }

        SkPaint paint;
        paint.setAntiAlias(true);
        // 高质量采样：缩放（尤其缩小，如把大 logo 缩到小尺寸）时用三次 Mitchell 滤波，
        // 避免最近邻的糊边与锯齿。视频按 1:1/放大提交时同样清晰。
        canvas_.drawImageRect(image, toSkRect(rect), SkSamplingOptions(SkCubicResampler::Mitchell()), &paint);
    }

private:
    static sk_sp<SkFontMgr> fontManager() {
        static sk_sp<SkFontMgr> fontMgr = [] {
            auto mgr = SkFontMgr_New_DirectWrite();
            if (!mgr) {
                mgr = SkFontMgr_New_GDI();
            }
            return mgr;
        }();
        return fontMgr;
    }

    static std::string utf8FontFamily(const std::wstring& familyName) {
        if (familyName.empty()) {
            return {};
        }
        const int length = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            familyName.data(),
            static_cast<int>(familyName.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (length <= 0) {
            return {};
        }
        std::string result(static_cast<std::size_t>(length), '\0');
        if (WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                familyName.data(),
                static_cast<int>(familyName.size()),
                result.data(),
                length,
                nullptr,
                nullptr) != length) {
            return {};
        }
        return result;
    }

    static sk_sp<SkTypeface> typeface(
        TextFontFamily family,
        int weight,
        const std::wstring& familyName = {}) {
        const auto fontMgr = fontManager();
        if (!fontMgr) {
            return {};
        }

        const int clampedWeight = std::clamp(weight, 100, 900);
        using TypefaceKey = std::tuple<TextFontFamily, int, std::wstring>;
        static std::map<TypefaceKey, sk_sp<SkTypeface>> cache;
        const TypefaceKey cacheKey{family, clampedWeight, familyName};
        if (auto cached = cache.find(cacheKey); cached != cache.end()) {
            return cached->second;
        }

        const SkFontStyle style(clampedWeight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant);
        if (const std::string requested = utf8FontFamily(familyName); !requested.empty()) {
            if (auto face = fontMgr->matchFamilyStyle(requested.c_str(), style);
                face && (family != TextFontFamily::Monospace || face->isFixedPitch())) {
                cache[cacheKey] = face;
                return face;
            }
        }
        if (family == TextFontFamily::Monospace) {
            // legacyMakeTypeface may silently substitute the system UI font when
            // a requested family is missing.  That turns terminal text
            // proportional while the grid is still measured from "M", causing
            // cumulative cursor drift.  matchFamilyStyle is strict, and the
            // fixed-pitch check keeps the terminal grid contract explicit.
            for (const char* candidate : {
                     "JetBrains Mono",
                     "Cascadia Mono",
                     "Cascadia Code",
                     "Consolas",
                     "Courier New",
                     "NSimSun"}) {
                if (auto face = fontMgr->matchFamilyStyle(candidate, style);
                    face && face->isFixedPitch()) {
                    cache[cacheKey] = face;
                    return face;
                }
            }
        }
        if (auto face = fontMgr->matchFamilyStyle("Microsoft YaHei", style)) {
            cache[cacheKey] = face;
            return face;
        }
        if (auto face = fontMgr->matchFamilyStyle("SimSun", style)) {
            cache[cacheKey] = face;
            return face;
        }
        auto face = fontMgr->matchFamilyStyle("Segoe UI", style);
        if (!face) {
            face = fontMgr->legacyMakeTypeface(nullptr, style);
        }
        cache[cacheKey] = face;
        return face;
    }

    struct CodepointSpan {
        SkUnichar codepoint = 0;
        std::size_t offset = 0;
        std::size_t length = 0;
    };

    static CodepointSpan codepointAt(const std::wstring& text, std::size_t offset) {
        if (offset >= text.size()) {
            return {};
        }
#if WCHAR_MAX <= 0xFFFF
        const auto first = static_cast<std::uint16_t>(text[offset]);
        if (first >= 0xD800 && first <= 0xDBFF && offset + 1 < text.size()) {
            const auto second = static_cast<std::uint16_t>(text[offset + 1]);
            if (second >= 0xDC00 && second <= 0xDFFF) {
                return {
                    static_cast<SkUnichar>(
                        0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00)),
                    offset,
                    2};
            }
        }
#endif
        return {static_cast<SkUnichar>(text[offset]), offset, 1};
    }

    static sk_sp<SkTypeface> typefaceForCodepoint(
        TextFontFamily family,
        int weight,
        const std::wstring& familyName,
        SkUnichar codepoint,
        const sk_sp<SkTypeface>& primary) {
        if (codepoint == 0 || (primary && primary->unicharToGlyph(codepoint) != 0)) {
            return primary;
        }

        const auto fontMgr = fontManager();
        if (!fontMgr) {
            return primary;
        }
        const int clampedWeight = std::clamp(weight, 100, 900);
        const SkFontStyle style(clampedWeight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant);
        if (family == TextFontFamily::Monospace) {
            for (const char* candidate : {"NSimSun", "Microsoft YaHei Mono"}) {
                if (auto face = fontMgr->matchFamilyStyle(candidate, style);
                    face && face->isFixedPitch() && face->unicharToGlyph(codepoint) != 0) {
                    return face;
                }
            }
        }
        const char* locales[] = {"zh-CN", "en-US"};
        auto fallback = fontMgr->matchFamilyStyleCharacter(
            nullptr,
            style,
            locales,
            static_cast<int>(std::size(locales)),
            codepoint);
        return fallback ? fallback : primary;
    }

    static const TextBlobEntry& cachedTextBlob(
        const std::wstring& text,
        float size,
        TextFontFamily family,
        const std::wstring& familyName,
        int weight) {
        static TextBlobEntry empty;
        if (text.empty()) {
            return empty;
        }

        const TextBlobKey key{
            text,
            static_cast<int>(std::round(size * 10.0f)),
            std::clamp(weight, 100, 900),
            family,
            familyName};
        static std::map<TextBlobKey, TextBlobEntry> cache;
        if (auto cached = cache.find(key); cached != cache.end()) {
            return cached->second;
        }
        if (cache.size() > 2048) {
            cache.clear();
        }

        TextBlobEntry entry;
        const auto primary = typeface(family, weight, familyName);
        std::size_t runStart = 0;
        sk_sp<SkTypeface> runTypeface;
        bool hasBounds = false;
        bool hasMetrics = false;

        auto appendRun = [&](std::size_t start, std::size_t end, const sk_sp<SkTypeface>& face) {
            if (end <= start || !face) {
                return;
            }
            SkFont font(face, size);
            font.setSubpixel(true);
            font.setEdging(SkFont::Edging::kAntiAlias);

            const auto byteLength = (end - start) * sizeof(wchar_t);
            SkRect runBounds = SkRect::MakeEmpty();
            const float runAdvance = font.measureText(
                text.data() + start, byteLength, SkTextEncoding::kUTF16, &runBounds);
            runBounds.offset(entry.advanceWidth, 0.0f);
            if (!runBounds.isEmpty()) {
                if (hasBounds) {
                    entry.bounds.join(runBounds);
                } else {
                    entry.bounds = runBounds;
                    hasBounds = true;
                }
            }

            SkFontMetrics runMetrics{};
            font.getMetrics(&runMetrics);
            if (!hasMetrics) {
                entry.metrics = runMetrics;
                hasMetrics = true;
            } else {
                entry.metrics.fTop = std::min(entry.metrics.fTop, runMetrics.fTop);
                entry.metrics.fAscent = std::min(entry.metrics.fAscent, runMetrics.fAscent);
                entry.metrics.fDescent = std::max(entry.metrics.fDescent, runMetrics.fDescent);
                entry.metrics.fBottom = std::max(entry.metrics.fBottom, runMetrics.fBottom);
                entry.metrics.fLeading = std::max(entry.metrics.fLeading, runMetrics.fLeading);
            }

            entry.runs.push_back({
                SkTextBlob::MakeFromText(
                    text.data() + start, byteLength, font, SkTextEncoding::kUTF16),
                entry.advanceWidth});
            entry.advanceWidth += runAdvance;
        };

        for (std::size_t offset = 0; offset < text.size();) {
            const CodepointSpan span = codepointAt(text, offset);
            const auto face = typefaceForCodepoint(
                family, weight, familyName, span.codepoint, primary);
            if (!runTypeface) {
                runTypeface = face;
                runStart = offset;
            } else if (!face || face->uniqueID() != runTypeface->uniqueID()) {
                appendRun(runStart, offset, runTypeface);
                runStart = offset;
                runTypeface = face ? face : primary;
            }
            offset += std::max<std::size_t>(span.length, 1);
        }
        appendRun(runStart, text.size(), runTypeface ? runTypeface : primary);

        const auto [it, _] = cache.emplace(key, std::move(entry));
        return it->second;
    }

    static sk_sp<SkMaskFilter> shadowMaskFilter(float blurRadius) {
        const float sigma = std::max(0.0f, blurRadius * 0.5f);
        if (sigma <= 0.0f) {
            return nullptr;
        }

        const int key = static_cast<int>(std::round(sigma * 100.0f));
        static std::map<int, sk_sp<SkMaskFilter>> cache;
        if (auto cached = cache.find(key); cached != cache.end()) {
            return cached->second;
        }

        auto filter = SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma, false);
        cache[key] = filter;
        return filter;
    }

    static ShadowImageEntry shadowImage(float width, float height, float radius, float blurRadius, float spreadRadius, Color color) {
        if (width <= 0.0f || height <= 0.0f || blurRadius <= 0.0f || color.a == 0) {
            return {};
        }

        const ShadowImageKey key{
            static_cast<int>(std::ceil(width)),
            static_cast<int>(std::ceil(height)),
            static_cast<int>(std::round(radius * 10.0f)),
            static_cast<int>(std::round(blurRadius * 10.0f)),
            static_cast<int>(std::round(spreadRadius * 10.0f)),
            toSkColor(color)};

        static std::map<ShadowImageKey, ShadowImageEntry> cache;
        if (auto cached = cache.find(key); cached != cache.end()) {
            return cached->second;
        }
        if (cache.size() > 512) {
            cache.clear();
        }

        const int pad = std::max(1, static_cast<int>(std::ceil(blurRadius * 2.0f)));
        const int imageWidth = key.width + pad * 2;
        const int imageHeight = key.height + pad * 2;
        if (imageWidth <= 0 || imageHeight <= 0 || imageWidth > 8192 || imageHeight > 8192) {
            return {};
        }

        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(imageWidth, imageHeight));
        if (!surface) {
            return {};
        }

        SkCanvas* shadowCanvas = surface->getCanvas();
        shadowCanvas->clear(SK_ColorTRANSPARENT);

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));
        paint.setMaskFilter(shadowMaskFilter(blurRadius));
        const float shadowRadius = std::max(0.0f, radius);
        shadowCanvas->drawRRect(
            SkRRect::MakeRectXY(SkRect::MakeXYWH(static_cast<float>(pad), static_cast<float>(pad), width, height), shadowRadius, shadowRadius),
            paint);

        ShadowImageEntry entry{surface->makeImageSnapshot(), pad};
        cache[key] = entry;
        return entry;
    }

    static sk_sp<SkShader> linearGradientShader(Rect rect, Color start, Color end, float angleDegrees) {
        const GradientShaderKey key{
            static_cast<int>(std::round(rect.x * 10.0f)),
            static_cast<int>(std::round(rect.y * 10.0f)),
            static_cast<int>(std::round(rect.width * 10.0f)),
            static_cast<int>(std::round(rect.height * 10.0f)),
            static_cast<int>(std::round(angleDegrees * 10.0f)),
            toSkColor(start),
            toSkColor(end)};

        static std::map<GradientShaderKey, sk_sp<SkShader>> cache;
        if (auto cached = cache.find(key); cached != cache.end()) {
            return cached->second;
        }
        if (cache.size() > 512) {
            cache.clear();
        }

        constexpr float pi = 3.14159265358979323846f;
        const float radians = (angleDegrees - 90.0f) * pi / 180.0f;
        const float dx = std::cos(radians);
        const float dy = std::sin(radians);
        const float half = std::sqrt(rect.width * rect.width + rect.height * rect.height) * 0.5f;
        const SkPoint points[2] = {
            SkPoint::Make(rect.x + rect.width * 0.5f - dx * half, rect.y + rect.height * 0.5f - dy * half),
            SkPoint::Make(rect.x + rect.width * 0.5f + dx * half, rect.y + rect.height * 0.5f + dy * half),
        };
        const SkColor4f colors[2] = {
            SkColor4f::FromColor(toSkColor(start)),
            SkColor4f::FromColor(toSkColor(end)),
        };
        const SkGradient gradient(SkGradient::Colors(colors, SkTileMode::kClamp), {});
        auto shader = SkShaders::LinearGradient(points, gradient);
        cache[key] = shader;
        return shader;
    }

    static sk_sp<SkImage> gradientImage(float width, float height, float radius, Color start, Color end, float angleDegrees) {
        if (width <= 0.0f || height <= 0.0f) {
            return nullptr;
        }

        const GradientImageKey key{
            static_cast<int>(std::ceil(width)),
            static_cast<int>(std::ceil(height)),
            static_cast<int>(std::round(radius * 10.0f)),
            static_cast<int>(std::round(angleDegrees * 10.0f)),
            toSkColor(start),
            toSkColor(end)};

        static std::map<GradientImageKey, sk_sp<SkImage>> cache;
        if (auto cached = cache.find(key); cached != cache.end()) {
            return cached->second;
        }
        if (cache.size() > 512) {
            cache.clear();
        }
        if (key.width <= 0 || key.height <= 0 || key.width > 8192 || key.height > 8192) {
            return nullptr;
        }

        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(key.width, key.height));
        if (!surface) {
            return nullptr;
        }

        SkCanvas* gradientCanvas = surface->getCanvas();
        gradientCanvas->clear(SK_ColorTRANSPARENT);

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setShader(linearGradientShader(Rect{0.0f, 0.0f, width, height}, start, end, angleDegrees));
        gradientCanvas->drawRRect(
            SkRRect::MakeRectXY(SkRect::MakeXYWH(0.0f, 0.0f, width, height), radius, radius),
            paint);

        auto image = surface->makeImageSnapshot();
        cache[key] = image;
        return image;
    }

    SkCanvas& canvas_;
    const std::wstring* defaultFontFamily_ = nullptr;
    std::optional<Rect> viewportBounds_;
    std::optional<Rect> clipBounds_;
    std::vector<std::optional<Rect>> clipStack_;
};

class Win32Window final : public Window {
public:
    explicit Win32Window(WindowOptions options)
        : options_(std::move(options))
        , dpiScale_(dpiScaleForWindowHandle(nullptr))
        , renderTraceEnabled_(renderTraceEnabled())
        , renderTraceFilePath_(renderTraceFilePath()) {
        animationFrameTimer_ = CreateWaitableTimerExW(
            nullptr,
            nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS);
        if (!animationFrameTimer_) {
            animationFrameTimer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        }
    }

    ~Win32Window() override {
        acceptingPostedCallbacks_.store(false, std::memory_order_release);
        discardPostedCallbacks();
        if (content_) {
            content_->detachFromOwner(this);
        }
        if (hwnd_) {
            DestroyWindow(hwnd_);
        }
        if (animationFrameTimer_) {
            CancelWaitableTimer(animationFrameTimer_);
            CloseHandle(animationFrameTimer_);
            animationFrameTimer_ = nullptr;
        }
    }

    void setContent(std::shared_ptr<Widget> widget) override {
        if (content_) {
            content_->detachFromOwner(this);
        }
        content_ = std::move(widget);
        if (content_) {
            content_->attachToOwner(
                this,
                [this] { requestRedraw(); },
                [this](Rect rect) { requestRedrawRect(rect); },
                [this] { scheduleContentAnimationFrame(); });
        }
        requestRedraw();
    }

    bool requestFocus(Widget* widget, bool focusVisible) override {
        if (!content_ || !widget) {
            return false;
        }
        bool focused = false;
        if (content_.get() == widget) {
            focused = widget->isFocusable();
            if (focused) {
                widget->onFocusChanged(true);
                widget->setFocusVisible(focusVisible);
            }
        } else if (auto* root = dynamic_cast<View*>(content_.get())) {
            focused = root->requestFocus(widget, focusVisible);
        }
        if (!focused) {
            return false;
        }
        ensureCreated();
        if (hwnd_) {
            SetFocus(hwnd_);
            updateImePosition();
        }
        requestRedraw();
        return true;
    }

    void setRawKeyHandler(RawKeyHandler handler) override {
        rawKeyHandler_ = std::move(handler);
    }

    void setClientSizeChangedHandler(ClientSizeChangedHandler handler) override {
        clientSizeChangedHandler_ = std::move(handler);
        scheduleClientSizeChanged();
    }

    void setDefaultFontFamily(std::wstring family) override {
        defaultFontFamily_ = std::move(family);
        requestRedraw();
    }

    void initialize() override {
        ensureCreated();
    }

    void show() override {
        ensureCreated();
        initGPU();
        showWithPlacementState(false);
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
    }

    void activate() override {
        ensureCreated();
        if (!hwnd_) {
            return;
        }
        initGPU();
        showWithPlacementState(true);
        SetForegroundWindow(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
    }

    int run() override {
        ensureCreated();
        MSG message{};
        while (true) {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    return static_cast<int>(message.wParam);
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            const DWORD handleCount = animationFrameTimer_ ? 1u : 0u;
            HANDLE handles[1]{animationFrameTimer_};
            const DWORD waitResult = MsgWaitForMultipleObjectsEx(
                handleCount,
                handleCount > 0 ? handles : nullptr,
                INFINITE,
                QS_ALLINPUT,
                MWMO_INPUTAVAILABLE);
            if (handleCount > 0 && waitResult == WAIT_OBJECT_0) {
                runAnimationFrameCallbacks();
                continue;
            }
            if (waitResult == WAIT_OBJECT_0 + handleCount) {
                continue;
            }
            return -1;
        }
    }

    void close() override {
        if (!hwnd_) {
            return;
        }
        // 开启“关闭到托盘”时，点击关闭仅隐藏窗口，程序继续在托盘后台运行。
        if (closeToTray_) {
            ShowWindow(hwnd_, SW_HIDE);
            return;
        }
        DestroyWindow(hwnd_);
    }

    void setCloseToTray(bool closeToTray) override {
        closeToTray_ = closeToTray;
    }

    void restoreFromTray() {
        if (!hwnd_) {
            return;
        }
        ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
        SetForegroundWindow(hwnd_);
    }

    void showTrayMenu() {
        if (!hwnd_) {
            return;
        }
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            return;
        }
        AppendMenuW(menu, MF_STRING, oneui::kTrayCommandShow, L"显示主界面");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, oneui::kTrayCommandExit, L"退出");
        POINT pt{};
        GetCursorPos(&pt);
        // 经典托盘菜单收尾：置前台并在弹出后补一条空消息，避免菜单不消失。
        SetForegroundWindow(hwnd_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
        PostMessageW(hwnd_, WM_NULL, 0, 0);
        DestroyMenu(menu);
    }

    void minimize() override {
        ensureCreated();
        if (hwnd_) {
            ShowWindow(hwnd_, SW_MINIMIZE);
        }
    }

    void requestRedraw() override {
        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    void prepareLayoutSnapshot() override {
        if (!hwnd_ || !content_ || !content_->visible()) {
            return;
        }
        const Size logical = clientSize();
        const Size physical = clientPixelSize();
        const int width = std::max(1, static_cast<int>(std::ceil(physical.width)));
        const int height = std::max(1, static_cast<int>(std::ceil(physical.height)));
        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
        if (!surface) {
            return;
        }
        SkCanvas* skCanvas = surface->getCanvas();
        skCanvas->scale(normalizedDpiScale(), normalizedDpiScale());
        SkiaCanvas canvas(
            *skCanvas,
            &defaultFontFamily_,
            Rect{0.0f, 0.0f, logical.width, logical.height});
        content_->setFrame(Rect{0.0f, 0.0f, logical.width, logical.height});
        content_->paint(canvas);
        paintTooltip(canvas);
    }

    bool post(std::function<void()> callback) override {
        if (!callback || !acceptingPostedCallbacks_.load(std::memory_order_acquire)) {
            return false;
        }

        ensureCreated();
        if (!hwnd_ || !acceptingPostedCallbacks_.load(std::memory_order_acquire)) {
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            if (!acceptingPostedCallbacks_.load(std::memory_order_relaxed)) {
                return false;
            }
            postedCallbacks_.push(std::move(callback));
        }
        if (!PostMessageW(hwnd_, kOneUiRunPostedCallbacks, 0, 0)) {
            acceptingPostedCallbacks_.store(false, std::memory_order_release);
            discardPostedCallbacks();
            return false;
        }
        return true;
    }

    void requestAnimationFrame(std::function<void(double)> callback) override {
        if (!callback) {
            return;
        }

        ensureCreated();
        {
            std::lock_guard<std::mutex> lock(animationCallbacksMutex_);
            animationCallbacks_.push(std::move(callback));
        }
        if (animationFramePending_) {
            return;
        }

        animationFramePending_ = true;
        if (armAnimationFrameTimer()) {
            return;
        }
        if (SetTimer(hwnd_, kOneUiAnimationTimer, kOneUiAnimationFrameIntervalMs, nullptr)) {
            animationTimerActive_ = true;
        } else {
            animationFramePending_ = false;
        }
    }

    NativeWindowHandle nativeHandle() const override {
        return hwnd_;
    }

    Size clientSize() const override {
        if (!hwnd_) {
            return Size{static_cast<float>(options_.width), static_cast<float>(options_.height)};
        }

        RECT rect{};
        if (!GetClientRect(hwnd_, &rect)) {
            return {};
        }
        const float scale = normalizedDpiScale();
        return Size{
            static_cast<float>(rect.right - rect.left) / scale,
            static_cast<float>(rect.bottom - rect.top) / scale};
    }

    Size clientPixelSize() const override {
        if (!hwnd_) {
            return Size{
                static_cast<float>(logicalToPhysicalCeil(static_cast<float>(options_.width))),
                static_cast<float>(logicalToPhysicalCeil(static_cast<float>(options_.height)))};
        }

        RECT rect{};
        if (!GetClientRect(hwnd_, &rect)) {
            return {};
        }
        return Size{static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top)};
    }

    float dpiScale() const override {
        return normalizedDpiScale();
    }

    void setTitle(std::wstring title) override {
        options_.title = std::move(title);
        if (hwnd_) {
            SetWindowTextW(hwnd_, options_.title.c_str());
        }
    }

    void setFullscreen(bool enabled) override {
        if (options_.fullscreen == enabled) {
            return;
        }

        ensureCreated();
        options_.fullscreen = enabled;
        applyWindowState();
    }

    void setBorderless(bool enabled) override {
        if (options_.borderless == enabled) {
            return;
        }

        ensureCreated();
        options_.borderless = enabled;
        if (!options_.fullscreen) {
            applyWindowState();
        }
        applyBorderlessShadow();
    }

    void setTitleBarDragMetrics(float titleBarHeight, float reservedButtonWidth) override {
        if (titleBarHeight > 0.0f) {
            titleBarHeightLogical_ = titleBarHeight;
        }
        if (reservedButtonWidth >= 0.0f) {
            titleButtonReservedWidthLogical_ = reservedButtonWidth;
        }
    }

    void setTitleBarInteractiveInsets(float leadingWidth, float trailingWidth) override {
        if (leadingWidth < 0.0f || trailingWidth < 0.0f) {
            titleBarInteractiveLeadingWidthLogical_ = -1.0f;
            titleBarInteractiveTrailingWidthLogical_ = -1.0f;
            return;
        }
        titleBarInteractiveLeadingWidthLogical_ = leadingWidth;
        titleBarInteractiveTrailingWidthLogical_ = trailingWidth;
    }

    void toggleMaximize() override {
        ensureCreated();
        if (!hwnd_ || options_.fullscreen) {
            return;
        }

        if (!options_.borderless) {
            ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
            return;
        }

        applyBorderlessMaximize(!borderlessMaximized_);
    }

    bool getWindowPlacement(WindowPlacement& placement) const override {
        if (!hasNormalPlacement_) {
            return false;
        }
        placement.x = lastNormalRect_.left;
        placement.y = lastNormalRect_.top;
        placement.width = lastNormalRect_.right - lastNormalRect_.left;
        placement.height = lastNormalRect_.bottom - lastNormalRect_.top;
        placement.maximized = placementStatePending_ ? pendingMaximized_ : lastKnownMaximized_;
        return placement.width > 0 && placement.height > 0;
    }

    bool setWindowPlacement(const WindowPlacement& placement) override {
        if (placement.width <= 0 || placement.height <= 0) {
            return false;
        }
        ensureCreated();
        if (!hwnd_) {
            return false;
        }

        const auto toLong = [](std::int64_t value) {
            return static_cast<LONG>(std::clamp<std::int64_t>(value, LONG_MIN, LONG_MAX));
        };
        RECT requested{
            toLong(placement.x),
            toLong(placement.y),
            toLong(static_cast<std::int64_t>(placement.x) + placement.width),
            toLong(static_cast<std::int64_t>(placement.y) + placement.height)};
        const HMONITOR monitor = MonitorFromRect(&requested, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo)) {
            return false;
        }

        const RECT& work = monitorInfo.rcWork;
        const int workWidth = static_cast<int>(std::max(1L, work.right - work.left));
        const int workHeight = static_cast<int>(std::max(1L, work.bottom - work.top));
        const int width = std::clamp(placement.width, 1, workWidth);
        const int height = std::clamp(placement.height, 1, workHeight);
        const int x = std::clamp(
            placement.x,
            static_cast<int>(work.left),
            static_cast<int>(work.right) - width);
        const int y = std::clamp(
            placement.y,
            static_cast<int>(work.top),
            static_cast<int>(work.bottom) - height);

        const bool visible = IsWindowVisible(hwnd_) != FALSE;
        if (borderlessMaximized_) {
            applyBorderlessMaximize(false);
        } else if (visible && IsZoomed(hwnd_)) {
            ShowWindow(hwnd_, SW_RESTORE);
        }

        if (!SetWindowPos(
            hwnd_,
            options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
            x,
            y,
            width,
            height,
            SWP_NOOWNERZORDER | SWP_NOACTIVATE)) {
            return false;
        }
        lastNormalRect_ = RECT{x, y, x + width, y + height};
        hasNormalPlacement_ = true;
        placementStatePending_ = !visible;
        pendingMaximized_ = placement.maximized;
        lastKnownMaximized_ = placement.maximized;

        if (visible && placement.maximized) {
            if (options_.borderless) {
                applyBorderlessMaximize(true);
            } else {
                ShowWindow(hwnd_, SW_MAXIMIZE);
            }
        }
        return true;
    }

private:
    void showWithPlacementState(bool restoreIconic) {
        if (!placementStatePending_) {
            ShowWindow(hwnd_, restoreIconic && IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
            return;
        }

        if (pendingMaximized_) {
            if (options_.borderless) {
                applyBorderlessMaximize(true);
                ShowWindow(hwnd_, SW_SHOW);
            } else {
                ShowWindow(hwnd_, SW_MAXIMIZE);
            }
        } else {
            if (borderlessMaximized_) {
                applyBorderlessMaximize(false);
            }
            ShowWindow(hwnd_, !options_.borderless && IsZoomed(hwnd_) ? SW_RESTORE : SW_SHOW);
        }
        placementStatePending_ = false;
        pendingMaximized_ = false;
    }

    void captureNormalPlacement() {
        if (!hwnd_ || options_.fullscreen || borderlessMaximized_ || IsIconic(hwnd_) || IsZoomed(hwnd_)) {
            return;
        }
        RECT rect{};
        if (!GetWindowRect(hwnd_, &rect) || rect.right <= rect.left || rect.bottom <= rect.top) {
            return;
        }
        lastNormalRect_ = rect;
        hasNormalPlacement_ = true;
    }

    // 无边框(WS_POPUP)窗口默认没有 DWM 投影，会像一张贴在桌面上的平面图。
    // 向客户区扩 1px glass 边即可启用系统标准窗口阴影（内容不透出、不影响命中）。
    // GetSystemMetricsForDpi 在 Win10 运行时存在，但 mingw-w64 头文件未声明，动态解析；
    // 解析不到(理论上不会)退回 GetSystemMetrics(仅系统 DPI 值)。
    static int frameMetricForDpi(int index, UINT dpi) {
        using Fn = int(WINAPI*)(int, UINT);
        static Fn fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi")));
        return fn ? fn(index, dpi) : GetSystemMetrics(index);
    }

    void applyBorderlessShadow() {
        if (!hwnd_ || !options_.borderless || options_.fullscreen) {
            return;
        }
        const MARGINS margins{0, 0, 0, 1};
        DwmExtendFrameIntoClientArea(hwnd_, &margins);
    }

    // 整体圆角：Win11 优先用 DWM 圆角首选项（抗锯齿 + 保留投影）；Win10 无该属性，
    // 退回 SetWindowRgn 裁一个圆角矩形区域。最大化/全屏时取消圆角（方正铺满）。
    void setCornerRadius(float radiusLogical) override {
        cornerRadiusLogical_ = radiusLogical > 0.0f ? radiusLogical : 0.0f;
        applyRoundedCorners();
    }

    void applyRoundedCorners() {
        if (!hwnd_) {
            return;
        }
        // Win11：DWMWA_WINDOW_CORNER_PREFERENCE = 33，DWMWCP_ROUND = 2 / DWMWCP_DONOTROUND = 1。
        const DWORD attr = 33;
        DWORD pref = (cornerRadiusLogical_ > 0.0f) ? 2 : 1;
        const bool dwmRounded = SUCCEEDED(DwmSetWindowAttribute(hwnd_, attr, &pref, sizeof(pref)));

        const bool square = cornerRadiusLogical_ <= 0.0f || borderlessMaximized_ || options_.fullscreen;
        if (dwmRounded) {
            // Win11 由 DWM 负责圆角（抗锯齿 + 自带柔和投影），清掉可能残留的区域与伴随投影。
            SetWindowRgn(hwnd_, nullptr, TRUE);
            shadowActive_ = false;
            updateShadowWindow();
            return;
        }
        // Win10 回退：用圆角矩形区域裁剪窗口。
        if (square) {
            SetWindowRgn(hwnd_, nullptr, TRUE);
            shadowActive_ = false;
            updateShadowWindow();
            return;
        }
        RECT rc{};
        if (!GetWindowRect(hwnd_, &rc)) {
            return;
        }
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) {
            return;
        }
        const int d = logicalToPhysicalCeil(cornerRadiusLogical_ * 2.0f);
        HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, d, d);
        SetWindowRgn(hwnd_, rgn, TRUE); // 系统接管 rgn 生命周期，不需手动 DeleteObject
        // 区域裁剪丢掉了 DWM 投影，启用伴随分层窗口补一层柔光投影。
        shadowActive_ = true;
        updateShadowWindow();
    }

    // ——— Win10 圆角柔光投影（分层伴随窗口）———
    // GL SwapBuffers present 是不透明的，主窗本身无法做逐像素透明；因此用一个独立的
    // WS_EX_LAYERED 窗口叠在主窗正下方，用有向距离场画出主窗圆角轮廓外的抗锯齿柔光。
    static constexpr float kShadowMarginLogical = 18.0f; // 投影外扩/羽化半径（逻辑像素）
    static constexpr int kShadowMaxAlpha = 66;           // 贴边最深处透明度（0-255，克制些更高级）
    static constexpr float kShadowDropLogical = 3.0f;    // 轮廓下移量，模拟顶部来光的下坠投影

    void ensureShadowWindow() {
        if (shadowHwnd_ || !hwnd_) {
            return;
        }
        static const wchar_t* kShadowClass = L"OneUIShadowWindow";
        static bool registered = false;
        HINSTANCE instance = GetModuleHandleW(nullptr);
        if (!registered) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = instance;
            wc.lpszClassName = kShadowClass;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassW(&wc);
            registered = true;
        }
        // 不设 owner：被 owner 的窗口恒在 owner 之上，会盖住内容；改为独立弹窗 + TOOLWINDOW
        // （不进任务栏/Alt-Tab），z 序由我们手动压到主窗正下方，并随主窗一起显隐/销毁。
        shadowHwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            kShadowClass, L"", WS_POPUP,
            0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
        shadowBuiltW_ = shadowBuiltH_ = 0;
    }

    void destroyShadowWindow() {
        if (shadowHwnd_) {
            DestroyWindow(shadowHwnd_);
            shadowHwnd_ = nullptr;
        }
        shadowActive_ = false;
        shadowBuiltW_ = shadowBuiltH_ = 0;
    }

    // 根据主窗当前状态刷新伴随投影：Win10 圆角且正常显示时展示，最大化/最小化/隐藏/
    // 非圆角时隐藏。尺寸未变的纯移动只重定位，避免拖动时反复重绘位图。
    void updateShadowWindow() {
        if (!shadowActive_) {
            if (shadowHwnd_ && IsWindowVisible(shadowHwnd_)) {
                ShowWindow(shadowHwnd_, SW_HIDE);
            }
            return;
        }
        if (!hwnd_ || !IsWindowVisible(hwnd_) || IsIconic(hwnd_) ||
            borderlessMaximized_ || options_.fullscreen || cornerRadiusLogical_ <= 0.0f) {
            if (shadowHwnd_) {
                ShowWindow(shadowHwnd_, SW_HIDE);
            }
            return;
        }
        ensureShadowWindow();
        if (!shadowHwnd_) {
            return;
        }
        RECT rc{};
        if (!GetWindowRect(hwnd_, &rc)) {
            return;
        }
        const int mw = rc.right - rc.left;
        const int mh = rc.bottom - rc.top;
        if (mw <= 0 || mh <= 0) {
            ShowWindow(shadowHwnd_, SW_HIDE);
            return;
        }
        const int margin = logicalToPhysicalCeil(kShadowMarginLogical);
        const int W = mw + margin * 2;
        const int H = mh + margin * 2;
        const POINT ptDst{rc.left - margin, rc.top - margin};

        if (mw == shadowBuiltW_ && mh == shadowBuiltH_) {
            // 尺寸没变，只需把投影窗挪到新位置并保持在主窗正下方。
            SetWindowPos(shadowHwnd_, hwnd_, ptDst.x, ptDst.y, W, H,
                         SWP_NOACTIVATE | SWP_NOREDRAW | SWP_SHOWWINDOW);
            return;
        }

        // 构建逐像素预乘 BGRA 位图：主窗圆角轮廓外用有向距离场做柔光衰减。
        const float radius = static_cast<float>(logicalToPhysicalCeil(cornerRadiusLogical_));
        const float cx = W * 0.5f;
        const float cy = H * 0.5f + kShadowDropLogical * normalizedDpiScale();
        const float halfW = mw * 0.5f;
        const float halfH = mh * 0.5f;
        const float rInner = std::max(0.0f, std::min(radius, std::min(halfW, halfH)));
        const float spread = static_cast<float>(margin);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = W;
        bmi.bmiHeader.biHeight = -H; // 自上而下
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC screenDC = GetDC(nullptr);
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib || !bits) {
            if (dib) DeleteObject(dib);
            ReleaseDC(nullptr, screenDC);
            return;
        }
        auto* px = static_cast<uint32_t*>(bits);
        for (int y = 0; y < H; ++y) {
            const float fy = static_cast<float>(y) + 0.5f;
            const float qy0 = std::fabs(fy - cy) - (halfH - rInner);
            for (int x = 0; x < W; ++x) {
                const float fx = static_cast<float>(x) + 0.5f;
                const float qx0 = std::fabs(fx - cx) - (halfW - rInner);
                const float ax = std::max(qx0, 0.0f);
                const float ay = std::max(qy0, 0.0f);
                const float d = std::sqrt(ax * ax + ay * ay) + std::min(std::max(qx0, qy0), 0.0f) - rInner;
                int a;
                if (d <= 0.0f) {
                    // 轮廓内部由主窗覆盖：画全透明而非黑，避免主窗某帧未及时覆盖时露出黑块。
                    a = 0;
                } else if (d >= spread) {
                    a = 0;
                } else {
                    float t = d / spread;   // 0..1
                    float f = 1.0f - t;
                    f = f * f;              // 二次衰减，边缘更柔
                    a = static_cast<int>(kShadowMaxAlpha * f + 0.5f);
                }
                // 预乘黑色：RGB=0，仅 alpha 生效。
                px[static_cast<size_t>(y) * W + x] = static_cast<uint32_t>(a) << 24;
            }
        }

        HDC memDC = CreateCompatibleDC(screenDC);
        HGDIOBJ oldBmp = SelectObject(memDC, dib);
        SIZE sz{W, H};
        POINT ptSrc{0, 0};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        POINT dst = ptDst;
        UpdateLayeredWindow(shadowHwnd_, screenDC, &dst, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
        DeleteObject(dib);
        ReleaseDC(nullptr, screenDC);

        shadowBuiltW_ = mw;
        shadowBuiltH_ = mh;
        // 压到主窗正下方并显示。
        SetWindowPos(shadowHwnd_, hwnd_, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    float normalizedDpiScale() const {
        return dpiScale_ > 0.0f ? dpiScale_ : 1.0f;
    }

    int logicalToPhysicalCeil(float value) const {
        return std::max(1, static_cast<int>(std::ceil(value * normalizedDpiScale())));
    }

    Point logicalPointFromClientPixels(int x, int y) const {
        const float scale = normalizedDpiScale();
        return Point{static_cast<float>(x) / scale, static_cast<float>(y) / scale};
    }

    Point logicalPointFromLParam(LPARAM lParam) const {
        return logicalPointFromClientPixels(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    }

    void ensureCreated() {
        if (hwnd_ || !acceptingPostedCallbacks_.load(std::memory_order_acquire)) {
            return;
        }

        ensureProcessDpiAwareness();
        dpiScale_ = dpiScaleForWindowHandle(nullptr);

        HINSTANCE instance = GetModuleHandleW(nullptr);
        const wchar_t* className = L"OneUIWindow";

        WNDCLASSW windowClass{};
        windowClass.style = CS_DBLCLKS;
        windowClass.lpfnWndProc = &Win32Window::windowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = nullptr;
        RegisterClassW(&windowClass);

        DWORD style = windowStyle();
        DWORD exStyle = windowExStyle();
        RECT rect{0, 0, logicalToPhysicalCeil(static_cast<float>(options_.width)), logicalToPhysicalCeil(static_cast<float>(options_.height))};
        // 无边框窗口的 WM_NCCALCSIZE 会把客户区铺满整个窗口，所以窗口尺寸就等于期望的
        // 客户区尺寸，不能再用 AdjustWindowRectEx 按 WS_THICKFRAME 外扩(否则会大出一圈边框)。
        if (!options_.borderless) {
            AdjustWindowRectEx(&rect, style, FALSE, exStyle);
        }

        hwnd_ = CreateWindowExW(
            exStyle,
            className,
            options_.title.c_str(),
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            instance,
            this);

        if (hwnd_) {
            dpiScale_ = dpiScaleForWindowHandle(hwnd_);
            applyBorderlessShadow();
            applyRoundedCorners();
            captureNormalPlacement();
            if (options_.visible) {
                initGPU();
            }
        }

        if (hwnd_ && options_.fullscreen) {
            applyWindowState();
        }
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            window = reinterpret_cast<Win32Window*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            window->hwnd_ = hwnd;
            ++g_liveWindowCount;
        }

        if (!window) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        return window->handleMessage(message, wParam, lParam);
    }

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint();
            return 0;
        case oneui::kTrayCallbackMessage:
            switch (LOWORD(lParam)) {
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
                restoreFromTray();
                break;
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                showTrayMenu();
                break;
            }
            return 0;
        case WM_COMMAND:
            if (HIWORD(wParam) == 0) { // 菜单命令
                switch (LOWORD(wParam)) {
                case oneui::kTrayCommandShow:
                    restoreFromTray();
                    return 0;
                case oneui::kTrayCommandExit:
                    closeToTray_ = false; // 强制真正退出，绕过“关闭到托盘”
                    if (hwnd_) {
                        DestroyWindow(hwnd_);
                    }
                    return 0;
                }
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case kOneUiRunPostedCallbacks:
            runPostedCallbacks();
            return 0;
        case kOneUiFinishWindowStatePaint:
            flushWindowStatePaint();
            return 0;
        case kOneUiDeferredFullPaint:
            flushDeferredFullPaint();
            return 0;
        case WM_TIMER:
            if (wParam == kOneUiAnimationTimer) {
                runAnimationFrameCallbacks();
                return 0;
            }
            if (wParam == kOneUiInteractivePaintTimer) {
                runInteractivePaintFrame();
                return 0;
            }
            if (wParam == kOneUiTooltipTimer) {
                KillTimer(hwnd_, kOneUiTooltipTimer);
                if (!hoveredTooltip_.empty()) {
                    tooltipVisible_ = true;
                    requestRedraw();
                }
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_MOUSEMOVE:
        {
            const Point point = logicalPointFromLParam(lParam);
            dispatchMouseMove(point);
            updateCursor(point);
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT point{};
                if (GetCursorPos(&point) && ScreenToClient(hwnd_, &point)) {
                    updateCursor(logicalPointFromClientPixels(point.x, point.y), true);
                } else {
                    SetCursor(cursorForKind(CursorKind::Default));
                }
                return TRUE;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_MOUSELEAVE:
            mouseLeaveTracking_ = false;
            lastCursorKind_ = CursorKind::Default;
            hasLastCursorPoint_ = false;
            SetCursor(cursorForKind(CursorKind::Default));
            clearTooltip();
            if (content_) {
                if (content_->clearInteractionState()) {
                    requestInteractiveRedraw();
                }
            }
            return 0;
        case WM_ENTERSIZEMOVE:
            interactiveResizeActive_ = true;
            return 0;
        case WM_EXITSIZEMOVE:
            interactiveResizeActive_ = false;
            requestDeferredFullPaint();
            printRenderTrace(true);
            return 0;
        case WM_MOUSEWHEEL:
            dispatchMouseWheel(wParam, lParam);
            return 0;
        case WM_LBUTTONDOWN:
            SetFocus(hwnd_);
            SetCapture(hwnd_);
            dispatchMouseDown(lParam, MouseButton::Left);
            return 0;
        case WM_LBUTTONDBLCLK:
            SetFocus(hwnd_);
            SetCapture(hwnd_);
            dispatchMouseDown(lParam, MouseButton::Left, 2);
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            dispatchMouseUp(lParam, MouseButton::Left);
            return 0;
        case WM_RBUTTONDOWN:
            dispatchMouseDown(lParam, MouseButton::Right);
            return 0;
        case WM_RBUTTONUP:
            dispatchMouseUp(lParam, MouseButton::Right);
            return 0;
        case WM_MBUTTONDOWN:
            dispatchMouseDown(lParam, MouseButton::Middle);
            return 0;
        case WM_MBUTTONUP:
            dispatchMouseUp(lParam, MouseButton::Middle);
            return 0;
        case WM_WINDOWPOSCHANGED:
        {
            // 位置/尺寸/Z 序变化（含失焦→再激活）后，重定位伴随投影并把它重新压到主窗正
            // 下方——修复“失焦后投影消失、要移动一下才回来”。必须先走默认处理，好派生出
            // WM_MOVE/WM_SIZE（区域圆角重算依赖 WM_SIZE）。
            const LRESULT r = DefWindowProcW(hwnd_, message, wParam, lParam);
            captureNormalPlacement();
            updateShadowWindow();
            return r;
        }
        case WM_DISPLAYCHANGE:
            animationFrameMonitor_ = nullptr;
            animationFrameIntervalMs_ = 1000.0 / 60.0;
            return 0;
        case WM_SHOWWINDOW:
            updateShadowWindow(); // 显隐（含托盘还原/隐藏）时同步投影窗显隐
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_SIZE:
            recordResizeMessage(wParam);
            if (wParam == SIZE_MAXIMIZED) {
                lastKnownMaximized_ = true;
            } else if (wParam == SIZE_RESTORED) {
                lastKnownMaximized_ = borderlessMaximized_;
            }
            if (wParam == SIZE_MINIMIZED && shadowHwnd_) {
                ShowWindow(shadowHwnd_, SW_HIDE); // 最小化立刻收起投影
            }
            if (wParam != SIZE_MINIMIZED) {
                applyRoundedCorners(); // 尺寸变化后重算圆角区域，避免拉伸/露白
                scheduleClientSizeChanged();
            }
            if (applyingWindowState_) {
                windowStatePaintPending_ = true;
                return 0;
            }
            if (wParam != SIZE_MINIMIZED) {
                if (interactiveResizeActive_) {
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    flushInteractivePaint();
                } else {
                    requestDeferredFullPaint();
                }
            }
            return 0;
        case kOneUiDpiChanged:
            handleDpiChanged(wParam, lParam);
            return 0;
        case WM_NCCALCSIZE:
            // 无边框窗口带着 WS_THICKFRAME(为拿 DWM 投影)，这里把非客户区尺寸清零，
            // 使客户区铺满整个窗口——投影保留、可见边框消失。常规最大化走自定义逻辑
            // (applyBorderlessMaximize，摆到工作区，IsZoomed 恒 false)，此时直接铺满。
            if (wParam == TRUE && options_.borderless && !options_.fullscreen) {
                if (IsZoomed(hwnd_)) {
                    // 兜底：若经由 Aero Snap 贴顶等路径进入真·系统最大化，系统会把窗口
                    // 摆成工作区外溢一圈边框。此时必须按边框厚度内缩，否则内容被屏幕边缘
                    // 裁掉、标题栏按钮跑到屏幕外。
                    auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    const UINT dpi = static_cast<UINT>(std::lround(normalizedDpiScale() * kDefaultDpi));
                    const int fx = frameMetricForDpi(SM_CXSIZEFRAME, dpi) + frameMetricForDpi(SM_CXPADDEDBORDER, dpi);
                    const int fy = frameMetricForDpi(SM_CYSIZEFRAME, dpi) + frameMetricForDpi(SM_CXPADDEDBORDER, dpi);
                    p->rgrc[0].left += fx;
                    p->rgrc[0].right -= fx;
                    p->rgrc[0].top += fy;
                    p->rgrc[0].bottom -= fy;
                }
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_NCHITTEST:
            if (const auto hit = hitTestBorderlessWindow(lParam); hit != HTNOWHERE) {
                return hit;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_NCLBUTTONDBLCLK:
            // A borderless window has no native caption style for DefWindowProc
            // to maximize. Preserve the standard title-bar contract explicitly
            // for drag regions while leaving interactive accessory content alone.
            if (options_.borderless && wParam == HTCAPTION && !options_.fullscreen) {
                toggleMaximize();
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_SETFOCUS:
            dispatchFocusChanged(true);
            return 0;
        case WM_KILLFOCUS:
            pendingHighSurrogate_ = 0;
            resetTrackedKeyState();
            dispatchFocusChanged(false);
            return 0;
        case WM_KEYDOWN:
            dispatchKeyDown(wParam, lParam);
            return 0;
        case WM_KEYUP:
            dispatchKeyUp(wParam, lParam);
            return 0;
        case WM_SYSKEYDOWN:
            if (dispatchKeyDown(wParam, lParam)) {
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_SYSKEYUP:
            if (dispatchKeyUp(wParam, lParam)) {
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_CHAR:
            dispatchTextInput(wParam);
            return 0;
        case WM_UNICHAR:
            if (wParam == UNICODE_NOCHAR) {
                return TRUE;
            }
            dispatchUnicodeScalar(static_cast<std::uint32_t>(wParam));
            return 0;
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_COMPOSITION:
            updateImePosition();
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_DESTROY:
            return 0;
        case WM_NCDESTROY:
        {
            captureNormalPlacement();
            acceptingPostedCallbacks_.store(false, std::memory_order_release);
            discardPostedCallbacks();
            if (animationFrameTimer_) {
                CancelWaitableTimer(animationFrameTimer_);
            }
            shutdownGPU();
            destroyShadowWindow(); // 主窗销毁时一并销毁独立的伴随投影窗
            HWND destroyedHwnd = hwnd_;
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
            hwnd_ = nullptr;
            if (--g_liveWindowCount == 0) {
                PostQuitMessage(0);
            }
            return DefWindowProcW(destroyedHwnd, message, wParam, lParam);
        }
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        }
    }

    DWORD windowStyle() const {
        if (options_.fullscreen) {
            return WS_POPUP;
        }
        if (options_.borderless) {
            // 纯 WS_POPUP 没有 DWM 投影，浅色桌面上窗口边缘看不清。加 WS_THICKFRAME
            // 让系统按标准窗口绘制投影；可见的非客户区边框随后由 WM_NCCALCSIZE 抹掉，
            // 视觉仍是无边框。最大化走自定义逻辑(applyBorderlessMaximize)，故不加
            // WS_MAXIMIZEBOX，避免系统 SW_MAXIMIZE 与自定义状态冲突。
            return WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX;
        }

        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (options_.resizable) {
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        }
        return style;
    }

    DWORD windowExStyle() const {
        DWORD exStyle = WS_EX_APPWINDOW;
        if (options_.topmost) {
            exStyle |= WS_EX_TOPMOST;
        }
        return exStyle;
    }

    LRESULT hitTestBorderlessWindow(LPARAM lParam) const {
        if (!hwnd_ || !options_.borderless || options_.fullscreen) {
            return HTNOWHERE;
        }

        RECT windowRect{};
        if (!GetWindowRect(hwnd_, &windowRect)) {
            return HTNOWHERE;
        }

        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int localX = x - windowRect.left;
        const int localY = y - windowRect.top;
        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        const int resizeBorder = logicalToPhysicalCeil(6.0f);
        const int titleBarHeight = logicalToPhysicalCeil(titleBarHeightLogical_);
        const int titleButtonReservedWidth = logicalToPhysicalCeil(titleButtonReservedWidthLogical_);

        if (options_.resizable) {
            const bool left = localX >= 0 && localX < resizeBorder;
            const bool right = localX <= width && localX >= width - resizeBorder;
            const bool top = localY >= 0 && localY < resizeBorder;
            const bool bottom = localY <= height && localY >= height - resizeBorder;

            if (top && left) {
                return HTTOPLEFT;
            }
            if (top && right) {
                return HTTOPRIGHT;
            }
            if (bottom && left) {
                return HTBOTTOMLEFT;
            }
            if (bottom && right) {
                return HTBOTTOMRIGHT;
            }
            if (left) {
                return HTLEFT;
            }
            if (right) {
                return HTRIGHT;
            }
            if (top) {
                return HTTOP;
            }
            if (bottom) {
                return HTBOTTOM;
            }
        }

        const bool inTitleBar = localY >= 0 && localY < titleBarHeight;
        const bool hasInteractiveTitleBarRegion =
            titleBarInteractiveLeadingWidthLogical_ >= 0.0f
            && titleBarInteractiveTrailingWidthLogical_ >= 0.0f;
        if (inTitleBar && hasInteractiveTitleBarRegion) {
            const int interactiveLeft = logicalToPhysicalCeil(titleBarInteractiveLeadingWidthLogical_);
            const int interactiveRight =
                width - logicalToPhysicalCeil(titleBarInteractiveTrailingWidthLogical_);
            if (localX >= interactiveLeft && localX < interactiveRight) {
                return HTNOWHERE;
            }
        }
        const bool overWindowButtons = localX >= width - titleButtonReservedWidth;
        if (inTitleBar && !overWindowButtons) {
            return HTCAPTION;
        }

        return HTNOWHERE;
    }

    void applyWindowState() {
        if (!hwnd_) {
            return;
        }

        beginWindowStateChange();
        if (options_.borderless) {
            if (options_.fullscreen) {
                if (!fullscreenApplied_) {
                    GetWindowRect(hwnd_, &savedFullscreenRect_);
                    fullscreenApplied_ = true;
                }

                HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                MONITORINFO monitorInfo{};
                monitorInfo.cbSize = sizeof(monitorInfo);
                GetMonitorInfoW(monitor, &monitorInfo);
                const RECT& bounds = monitorInfo.rcMonitor;
                SetWindowPos(
                    hwnd_,
                    options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                    bounds.left,
                    bounds.top,
                    bounds.right - bounds.left,
                    bounds.bottom - bounds.top,
                    SWP_NOOWNERZORDER | SWP_NOACTIVATE);
                finishWindowStateChange();
                return;
            }

            if (fullscreenApplied_) {
                fullscreenApplied_ = false;
                SetWindowPos(
                    hwnd_,
                    options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                    savedFullscreenRect_.left,
                    savedFullscreenRect_.top,
                    savedFullscreenRect_.right - savedFullscreenRect_.left,
                    savedFullscreenRect_.bottom - savedFullscreenRect_.top,
                    SWP_NOOWNERZORDER | SWP_NOACTIVATE);
                finishWindowStateChange();
                return;
            }

            finishWindowStateChange();
            return;
        }

        if (options_.fullscreen) {
            if (!fullscreenApplied_) {
                savedStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
                savedExStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE));
                savedPlacement_.length = sizeof(savedPlacement_);
                GetWindowPlacement(hwnd_, &savedPlacement_);
                fullscreenApplied_ = true;
            }

            HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            GetMonitorInfoW(monitor, &monitorInfo);

            SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP);
            SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, windowExStyle());
            SetWindowPos(
                hwnd_,
                options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
            finishWindowStateChange();
            return;
        }

        const DWORD style = fullscreenApplied_ ? savedStyle_ : windowStyle();
        const DWORD exStyle = fullscreenApplied_ ? savedExStyle_ : windowExStyle();
        SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle);

        if (fullscreenApplied_) {
            SetWindowPlacement(hwnd_, &savedPlacement_);
            fullscreenApplied_ = false;
        }

        SetWindowPos(
            hwnd_,
            options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        finishWindowStateChange();
    }

    void beginWindowStateChange() {
        applyingWindowState_ = true;
        windowStatePaintPending_ = true;
    }

    void applyBorderlessMaximize(bool enabled) {
        if (!hwnd_ || borderlessMaximized_ == enabled) {
            return;
        }

        beginWindowStateChange();
        if (enabled) {
            GetWindowRect(hwnd_, &savedBorderlessRect_);
            HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            GetMonitorInfoW(monitor, &monitorInfo);
            const RECT& work = monitorInfo.rcWork;
            borderlessMaximized_ = true;
            SetWindowPos(
                hwnd_,
                options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                work.left,
                work.top,
                work.right - work.left,
                work.bottom - work.top,
                SWP_NOOWNERZORDER | SWP_NOACTIVATE);
            finishWindowStateChange();
            return;
        }

        borderlessMaximized_ = false;
        SetWindowPos(
            hwnd_,
            options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
            savedBorderlessRect_.left,
            savedBorderlessRect_.top,
            savedBorderlessRect_.right - savedBorderlessRect_.left,
            savedBorderlessRect_.bottom - savedBorderlessRect_.top,
            SWP_NOOWNERZORDER | SWP_NOACTIVATE);
        finishWindowStateChange();
    }

    void finishWindowStateChange() {
        if (!hwnd_) {
            applyingWindowState_ = false;
            windowStatePaintPending_ = false;
            return;
        }
        PostMessageW(hwnd_, kOneUiFinishWindowStatePaint, 0, 0);
    }

    void flushWindowStatePaint() {
        if (!hwnd_) {
            applyingWindowState_ = false;
            windowStatePaintPending_ = false;
            return;
        }

        applyingWindowState_ = false;
        if (!windowStatePaintPending_) {
            return;
        }

        windowStatePaintPending_ = false;
        InvalidateRect(hwnd_, nullptr, FALSE);
        flushInteractivePaint();
        printRenderTrace(true);
    }

    void requestDeferredFullPaint() {
        if (!hwnd_) {
            return;
        }

        InvalidateRect(hwnd_, nullptr, FALSE);
        scheduleInteractivePaint();
    }

    void flushDeferredFullPaint() {
        if (!hwnd_) {
            deferredFullPaintPending_ = false;
            return;
        }

        deferredFullPaintPending_ = false;
        flushPendingPaint();
    }

    void handleDpiChanged(WPARAM wParam, LPARAM lParam) {
        const UINT dpiX = LOWORD(wParam);
        const float nextScale = scaleFromDpi(dpiX != 0 ? dpiX : static_cast<UINT>(std::round(dpiScale_ * kDefaultDpi)));
        if (nextScale <= 0.0f) {
            return;
        }

        const bool changed = std::abs(nextScale - dpiScale_) > 0.001f;
        dpiScale_ = nextScale;
        if (changed) {
            paintSurface_.reset();
            paintSurfaceWidth_ = 0;
            paintSurfaceHeight_ = 0;
        }

        if (lParam && !options_.fullscreen && !borderlessMaximized_) {
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(
                hwnd_,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }

        requestDeferredFullPaint();
    }

    void runPostedCallbacks() {
        std::queue<std::function<void()>> callbacks;
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            callbacks.swap(postedCallbacks_);
        }

        while (!callbacks.empty()) {
            auto callback = std::move(callbacks.front());
            callbacks.pop();
            callback();
        }
    }

    void runAnimationFrameCallbacks() {
        const bool traceScroll = internal::scrollTraceEnabled();
        const double frameStartMs = traceScroll ? internal::scrollTraceNowMs() : 0.0;
        const double frameIntervalMs = traceScroll && scrollTraceLastAnimationFrameMs_ > 0.0
            ? frameStartMs - scrollTraceLastAnimationFrameMs_
            : 0.0;
        if (traceScroll) {
            scrollTraceLastAnimationFrameMs_ = frameStartMs;
        }
        if (hwnd_ && animationTimerActive_) {
            KillTimer(hwnd_, kOneUiAnimationTimer);
            animationTimerActive_ = false;
        }
        animationFramePending_ = false;

        std::queue<std::function<void(double)>> callbacks;
        {
            std::lock_guard<std::mutex> lock(animationCallbacksMutex_);
            callbacks.swap(animationCallbacks_);
        }
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "animation_frame_begin", reinterpret_cast<std::uintptr_t>(hwnd_),
                0.0, 0.0, 0.0, 0.0, frameIntervalMs, 0.0, 0.0,
                static_cast<double>(callbacks.size()), swapIntervalEnabled_ ? 1.0 : 0.0});
        }

        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const double nowMs = std::chrono::duration<double, std::milli>(now).count();
        lastAnimationFrameMs_ = nowMs;
        while (!callbacks.empty()) {
            auto callback = std::move(callbacks.front());
            callbacks.pop();
            callback(nowMs);
        }
        flushPendingPaint();
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "animation_frame_presented", reinterpret_cast<std::uintptr_t>(hwnd_),
                0.0, 0.0, 0.0, 0.0, frameIntervalMs, 0.0,
                internal::scrollTraceNowMs() - frameStartMs, 0.0,
                swapIntervalEnabled_ ? 1.0 : 0.0});
        }
    }

    bool armAnimationFrameTimer() {
        if (!animationFrameTimer_) {
            return false;
        }

        const double nowMs = currentTimeMs();
        const double refreshIntervalMs = animationFrameIntervalMs();
        const double scheduledMs = lastAnimationFrameMs_ > 0.0
            ? lastAnimationFrameMs_ + refreshIntervalMs
            : nowMs;
        const double delayMs = std::max(0.5, scheduledMs - nowMs);
        LARGE_INTEGER dueTime{};
        dueTime.QuadPart = -std::max<LONGLONG>(
            1,
            static_cast<LONGLONG>(std::llround(delayMs * 10000.0)));
        return SetWaitableTimer(
            animationFrameTimer_,
            &dueTime,
            0,
            nullptr,
            nullptr,
            FALSE) == TRUE;
    }

    double animationFrameIntervalMs() {
        const HMONITOR monitor = hwnd_
            ? MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST)
            : nullptr;
        if (monitor && monitor != animationFrameMonitor_) {
            animationFrameMonitor_ = monitor;
            animationFrameIntervalMs_ = 1000.0 / 60.0;

            MONITORINFOEXW monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            DEVMODEW displayMode{};
            displayMode.dmSize = sizeof(displayMode);
            if (GetMonitorInfoW(monitor, &monitorInfo)
                && EnumDisplaySettingsW(
                    monitorInfo.szDevice,
                    ENUM_CURRENT_SETTINGS,
                    &displayMode)
                && displayMode.dmDisplayFrequency > 1) {
                animationFrameIntervalMs_ = 1000.0
                    / static_cast<double>(displayMode.dmDisplayFrequency);
                return animationFrameIntervalMs_;
            }

            DWM_TIMING_INFO timingInfo{};
            timingInfo.cbSize = sizeof(timingInfo);
            if (SUCCEEDED(DwmGetCompositionTimingInfo(hwnd_, &timingInfo))
                && timingInfo.rateRefresh.uiNumerator > 0
                && timingInfo.rateRefresh.uiDenominator > 0) {
                const double intervalMs = 1000.0
                    * static_cast<double>(timingInfo.rateRefresh.uiDenominator)
                    / static_cast<double>(timingInfo.rateRefresh.uiNumerator);
                if (intervalMs >= 2.0 && intervalMs <= 100.0) {
                    animationFrameIntervalMs_ = intervalMs;
                }
            }
        }
        return animationFrameIntervalMs_;
    }

    void discardPostedCallbacks() {
        std::queue<std::function<void()>> callbacks;
        std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
        callbacks.swap(postedCallbacks_);
    }

    void runInteractivePaintFrame() {
        if (hwnd_ && interactivePaintTimerActive_) {
            KillTimer(hwnd_, kOneUiInteractivePaintTimer);
            interactivePaintTimerActive_ = false;
        }
        flushPendingPaint();
    }

    void scheduleContentAnimationFrame() {
        if (contentAnimationFramePending_) {
            return;
        }
        contentAnimationFramePending_ = true;
        requestAnimationFrame([this](double nowMs) {
            contentAnimationFramePending_ = false;
            if (!content_ || !content_->visible()) {
                return;
            }

            if (content_->tickAnimations(nowMs)) {
                scheduleContentAnimationFrame();
            }
        });
    }

    RECT invalidationRectFor(Rect rect) const {
        RECT clientRect{};
        if (!hwnd_ || !GetClientRect(hwnd_, &clientRect)) {
            return RECT{0, 0, 0, 0};
        }

        const float scale = normalizedDpiScale();
        const int margin = logicalToPhysicalCeil(48.0f);
        const int left = static_cast<int>(std::floor(rect.x * scale)) - margin;
        const int top = static_cast<int>(std::floor(rect.y * scale)) - margin;
        const int right = static_cast<int>(std::ceil((rect.x + rect.width) * scale)) + margin;
        const int bottom = static_cast<int>(std::ceil((rect.y + rect.height) * scale)) + margin;
        const int clientLeft = static_cast<int>(clientRect.left);
        const int clientTop = static_cast<int>(clientRect.top);
        const int clientRight = static_cast<int>(clientRect.right);
        const int clientBottom = static_cast<int>(clientRect.bottom);
        return RECT{
            std::clamp(left, clientLeft, clientRight),
            std::clamp(top, clientTop, clientBottom),
            std::clamp(right, clientLeft, clientRight),
            std::clamp(bottom, clientTop, clientBottom)};
    }

    void requestRedrawRect(Rect rect) {
        if (!hwnd_ || rect.width <= 0.0f || rect.height <= 0.0f) {
            requestRedraw();
            return;
        }

        const RECT dirty = invalidationRectFor(rect);

        if (dirty.right <= dirty.left || dirty.bottom <= dirty.top) {
            return;
        }

        InvalidateRect(hwnd_, &dirty, FALSE);
    }

    void requestInteractiveRedraw() {
        if (!hwnd_) {
            return;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        flushInteractivePaint();
    }

    void paint() {
        const double paintStartMs = currentTimeMs();
        RECT clientRect{};
        GetClientRect(hwnd_, &clientRect);

        const bool useGPU = gpuAvailable_ && glContext_ && glDC_;
        if (useGPU) {
            wglMakeCurrent(glDC_, glContext_);
            glViewport(0, 0,
                static_cast<GLsizei>(clientRect.right - clientRect.left),
                static_cast<GLsizei>(clientRect.bottom - clientRect.top));
        }
        PAINTSTRUCT paintStruct{};
        HDC paintDc = BeginPaint(hwnd_, &paintStruct);
        HDC dc = useGPU ? glDC_ : paintDc;
        RECT dirtyRect = paintStruct.rcPaint;
        const int width = std::max<LONG>(1, clientRect.right - clientRect.left);
        const int height = std::max<LONG>(1, clientRect.bottom - clientRect.top);
        const float scale = normalizedDpiScale();
        const float logicalWidth = static_cast<float>(width) / scale;
        const float logicalHeight = static_cast<float>(height) / scale;

        const double surfaceStartMs = currentTimeMs();
        const bool allocatedSurface = ensurePaintSurfaceCapacity(width, height);
        const double surfaceMs = currentTimeMs() - surfaceStartMs;

        if (!paintSurface_) {
            EndPaint(hwnd_, &paintStruct);
            return;
        }
        if (allocatedSurface) {
            dirtyRect = clientRect;
        }
        const int dirtyX = std::clamp<LONG>(dirtyRect.left, 0, width);
        const int dirtyY = std::clamp<LONG>(dirtyRect.top, 0, height);
        const int dirtyRight = std::clamp<LONG>(dirtyRect.right, dirtyX, width);
        const int dirtyBottom = std::clamp<LONG>(dirtyRect.bottom, dirtyY, height);
        const int dirtyWidth = dirtyRight - dirtyX;
        const int dirtyHeight = dirtyBottom - dirtyY;
        if (dirtyWidth <= 0 || dirtyHeight <= 0) {
            EndPaint(hwnd_, &paintStruct);
            return;
        }

        SkCanvas* skCanvas = paintSurface_->getCanvas();
        SkiaCanvas rawCanvas(
            *skCanvas,
            nullptr,
            Rect{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
        const bool fullPaint = dirtyX == 0 && dirtyY == 0 && dirtyWidth == width && dirtyHeight == height;
        if (fullPaint) {
            rawCanvas.clear(colors::Surface);
        }

        skCanvas->save();
        skCanvas->scale(scale, scale);
        SkiaCanvas canvas(
            *skCanvas,
            &defaultFontFamily_,
            Rect{0.0f, 0.0f, logicalWidth, logicalHeight});
        if (!fullPaint) {
            const Rect dirtyCanvasRect{
                static_cast<float>(dirtyX) / scale,
                static_cast<float>(dirtyY) / scale,
                static_cast<float>(dirtyWidth) / scale,
                static_cast<float>(dirtyHeight) / scale};
            canvas.save();
            canvas.clipRect(dirtyCanvasRect);
            canvas.fillRect(dirtyCanvasRect, colors::Surface, 0.0f);
        }

        if (content_ && content_->visible()) {
            const double contentStartMs = currentTimeMs();
            g_primitivePaintTrace = PrimitivePaintTrace{};
            content_->setFrame(Rect{0.0f, 0.0f, logicalWidth, logicalHeight});
            content_->paint(canvas);
            paintTooltip(canvas);
            recordContentPaint(currentTimeMs() - contentStartMs);
            recordPrimitivePaint(g_primitivePaintTrace);
        }

        if (!fullPaint) {
            canvas.restore();
        }
        skCanvas->restore();

        if (gpuAvailable_ && grContext_ && windowSurface_) {
            SkCanvas* presentCanvas = windowSurface_->getCanvas();
            presentCanvas->clear(SK_ColorBLACK);
            paintSurface_->draw(presentCanvas, 0.0f, 0.0f);
            grContext_->flushAndSubmit();
            if (SwapBuffers(glDC_)) {
                EndPaint(hwnd_, &paintStruct);
                recordPaint(width, height, fullPaint, allocatedSurface, surfaceMs, currentTimeMs() - paintStartMs);
                return;
            }
            gpuAvailable_ = false;
            swapIntervalEnabled_ = false;
            windowSurface_.reset();
            paintSurface_.reset();
            paintSurfaceWidth_ = 0;
            paintSurfaceHeight_ = 0;
            InvalidateRect(hwnd_, nullptr, FALSE);
            EndPaint(hwnd_, &paintStruct);
            return;
        }

        SkPixmap pixmap;
        if (paintSurface_->peekPixels(&pixmap)) {
            const double blitStartMs = currentTimeMs();
            if (fullPaint) {
                BITMAPINFO bitmapInfo{};
                bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bitmapInfo.bmiHeader.biWidth = paintSurfaceWidth_;
                bitmapInfo.bmiHeader.biHeight = -paintSurfaceHeight_;
                bitmapInfo.bmiHeader.biPlanes = 1;
                bitmapInfo.bmiHeader.biBitCount = 32;
                bitmapInfo.bmiHeader.biCompression = BI_RGB;

                StretchDIBits(
                    dc,
                    0,
                    0,
                    width,
                    height,
                    0,
                    0,
                    width,
                    height,
                    pixmap.addr(),
                    &bitmapInfo,
                    DIB_RGB_COLORS,
                    SRCCOPY);
            } else {
                const std::size_t pixelCount = static_cast<std::size_t>(dirtyWidth) * static_cast<std::size_t>(dirtyHeight);
                blitScratch_.resize(pixelCount);
                const std::size_t rowBytes = static_cast<std::size_t>(dirtyWidth) * 4;
                for (int row = 0; row < dirtyHeight; ++row) {
                    const void* src = pixmap.addr(dirtyX, dirtyY + row);
                    std::memcpy(
                        reinterpret_cast<unsigned char*>(blitScratch_.data()) + static_cast<std::size_t>(row) * rowBytes,
                        src,
                        rowBytes);
                }

                BITMAPINFO bitmapInfo{};
                bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bitmapInfo.bmiHeader.biWidth = dirtyWidth;
                bitmapInfo.bmiHeader.biHeight = -dirtyHeight;
                bitmapInfo.bmiHeader.biPlanes = 1;
                bitmapInfo.bmiHeader.biBitCount = 32;
                bitmapInfo.bmiHeader.biCompression = BI_RGB;

                SetDIBitsToDevice(
                    dc,
                    dirtyX,
                    dirtyY,
                    dirtyWidth,
                    dirtyHeight,
                    0,
                    0,
                    0,
                    dirtyHeight,
                    blitScratch_.data(),
                    &bitmapInfo,
                    DIB_RGB_COLORS);
            }
            recordBlit(currentTimeMs() - blitStartMs);
        }

        EndPaint(hwnd_, &paintStruct);
        recordPaint(width, height, fullPaint, allocatedSurface, surfaceMs, currentTimeMs() - paintStartMs);
    }

    static int alignedPaintSurfaceSize(int requested) {
        const int padded = std::max(1, requested + kPaintSurfaceGrowthPadding);
        return ((padded + kPaintSurfaceAlignment - 1) / kPaintSurfaceAlignment) * kPaintSurfaceAlignment;
    }

    bool ensurePaintSurfaceCapacity(int width, int height) {
        if (paintSurface_) {
            if (gpuAvailable_ && windowSurface_ &&
                paintSurfaceWidth_ == width && paintSurfaceHeight_ == height) {
                return false;
            }
            if (!gpuAvailable_ && paintSurfaceWidth_ >= width && paintSurfaceHeight_ >= height) {
                return false;
            }
        }

        int surfaceWidth = alignedPaintSurfaceSize(width);
        int surfaceHeight = alignedPaintSurfaceSize(height);
        SkImageInfo imageInfo = SkImageInfo::Make(surfaceWidth, surfaceHeight, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        if (gpuAvailable_ && grContext_) {
            surfaceWidth = width;
            surfaceHeight = height;
            imageInfo = SkImageInfo::Make(surfaceWidth, surfaceHeight, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
            auto retainedSurface = SkSurfaces::RenderTarget(
                grContext_.get(),
                skgpu::Budgeted::kYes,
                imageInfo,
                0,
                kTopLeft_GrSurfaceOrigin,
                nullptr);
            GrGLFramebufferInfo fbInfo;
            fbInfo.fFBOID = 0;
            fbInfo.fFormat = GL_RGBA8;
            auto backendRT = GrBackendRenderTargets::MakeGL(surfaceWidth, surfaceHeight, 0, 0, fbInfo);
            auto windowSurface = SkSurfaces::WrapBackendRenderTarget(
                grContext_.get(), backendRT, kBottomLeft_GrSurfaceOrigin,
                kBGRA_8888_SkColorType, nullptr, nullptr);
            if (retainedSurface && windowSurface) {
                paintSurface_ = std::move(retainedSurface);
                windowSurface_ = std::move(windowSurface);
                paintSurfaceWidth_ = surfaceWidth;
                paintSurfaceHeight_ = surfaceHeight;
                return true;
            }
            paintSurface_.reset();
            windowSurface_.reset();
            gpuAvailable_ = false;
            swapIntervalEnabled_ = false;
        }
        // 走到这里说明无表面或容量不足，必须重建光栅表面。
        // 旧逻辑仅在表面为空时新建：容量不足时沿用小表面、记账尺寸却改成大的，
        // 后续 blit 按大尺寸读小缓冲越界崩溃（窗口最大化时交互重绘先于延迟全绘触发，必现）。
        paintSurface_ = SkSurfaces::Raster(imageInfo);
        paintSurfaceWidth_ = paintSurface_ ? surfaceWidth : 0;
        paintSurfaceHeight_ = paintSurface_ ? surfaceHeight : 0;
        return true;
    }

    void recordResizeMessage(WPARAM sizeType) {
        if (!renderTraceEnabled_) {
            return;
        }
        ++traceResizeMessages_;
        if (interactiveResizeActive_) {
            ++traceInteractiveResizeMessages_;
        }
        if (sizeType == SIZE_MAXIMIZED) {
            ++traceMaximizeMessages_;
        } else if (sizeType == SIZE_RESTORED) {
            ++traceRestoreMessages_;
        }
    }

    void scheduleClientSizeChanged() {
        if (!clientSizeChangedHandler_ || clientSizeChangedFramePending_ || !hwnd_) {
            return;
        }
        clientSizeChangedFramePending_ = true;
        requestAnimationFrame([this](double) {
            clientSizeChangedFramePending_ = false;
            if (clientSizeChangedHandler_) {
                clientSizeChangedHandler_(clientSize());
            }
        });
    }

    void recordContentPaint(double elapsedMs) {
        if (renderTraceEnabled_) {
            traceContentPaintMs_ += elapsedMs;
        }
    }

    void recordPrimitivePaint(const PrimitivePaintTrace& trace) {
        if (!renderTraceEnabled_) {
            return;
        }
        traceTextCalls_ += trace.textCalls;
        traceTextMeasureCalls_ += trace.textMeasureCalls;
        traceShadowCalls_ += trace.shadowCalls;
        traceGradientCalls_ += trace.gradientCalls;
        traceTextMs_ += trace.textMs;
        traceTextMeasureMs_ += trace.textMeasureMs;
        traceShadowMs_ += trace.shadowMs;
        traceGradientMs_ += trace.gradientMs;
    }

    void recordBlit(double elapsedMs) {
        if (renderTraceEnabled_) {
            traceBlitMs_ += elapsedMs;
        }
    }

    void recordPaint(int width, int height, bool fullPaint, bool allocatedSurface, double surfaceMs, double elapsedMs) {
        const double nowMs = currentTimeMs();
        double frameIntervalMs = 0.0;
        if (internal::scrollTraceEnabled()) {
            const double traceNowMs = internal::scrollTraceNowMs();
            frameIntervalMs = scrollTraceLastPaintMs_ > 0.0
                ? traceNowMs - scrollTraceLastPaintMs_
                : 0.0;
            scrollTraceLastPaintMs_ = traceNowMs;
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "paint_presented", reinterpret_cast<std::uintptr_t>(hwnd_),
                0.0, 0.0, 0.0, 0.0, frameIntervalMs, 0.0, elapsedMs,
                fullPaint ? 1.0 : 0.0,
                static_cast<double>(width) * static_cast<double>(height)});
        }
        if (!renderTraceEnabled_) {
            return;
        }
        if (traceLastPaintMs_ > 0.0) {
            const double renderFrameIntervalMs = nowMs - traceLastPaintMs_;
            if (renderFrameIntervalMs > 0.0 && renderFrameIntervalMs < 250.0) {
                traceFrameIntervalMs_ += renderFrameIntervalMs;
                ++traceFrameIntervals_;
                if (traceMinFrameIntervalMs_ == 0.0 || renderFrameIntervalMs < traceMinFrameIntervalMs_) {
                    traceMinFrameIntervalMs_ = renderFrameIntervalMs;
                }
                traceMaxFrameIntervalMs_ = std::max(traceMaxFrameIntervalMs_, renderFrameIntervalMs);
            }
        }
        traceLastPaintMs_ = nowMs;
        ++tracePaints_;
        if (fullPaint) {
            ++traceFullPaints_;
        } else {
            ++tracePartialPaints_;
        }
        if (allocatedSurface) {
            ++traceSurfaceAllocations_;
            traceSurfaceAllocMs_ += surfaceMs;
        }
        tracePaintMs_ += elapsedMs;
        traceLastWidth_ = width;
        traceLastHeight_ = height;
        printRenderTrace(false);
    }

    void printRenderTrace(bool force) {
        if (!renderTraceEnabled_) {
            return;
        }
        const double nowMs = currentTimeMs();
        if (traceLastPrintMs_ == 0.0) {
            traceLastPrintMs_ = nowMs;
            if (!force) {
                return;
            }
        }
        if (!force && nowMs - traceLastPrintMs_ < 1000.0) {
            return;
        }
        if (traceResizeMessages_ == 0 && tracePaints_ == 0 && traceSurfaceAllocations_ == 0) {
            return;
        }

        const double paintAvg = tracePaints_ > 0 ? tracePaintMs_ / static_cast<double>(tracePaints_) : 0.0;
        const double contentAvg = tracePaints_ > 0 ? traceContentPaintMs_ / static_cast<double>(tracePaints_) : 0.0;
        const double blitAvg = tracePaints_ > 0 ? traceBlitMs_ / static_cast<double>(tracePaints_) : 0.0;
        const double frameIntervalAvg = traceFrameIntervals_ > 0
            ? traceFrameIntervalMs_ / static_cast<double>(traceFrameIntervals_)
            : 0.0;
        const double presentedFps = frameIntervalAvg > 0.0 ? 1000.0 / frameIntervalAvg : 0.0;
        char line[1536]{};
        std::snprintf(
            line,
            sizeof(line),
            "[oneui-render] size=%dx%d wm_size=%llu interactive_size=%llu max=%llu restore=%llu paints=%llu full=%llu partial=%llu frame=%.2fms/%.1ffps min=%.2fms max=%.2fms surface_alloc=%llu avg_paint=%.2fms avg_content=%.2fms avg_blit=%.2fms surface_alloc_ms=%.2f text=%llu/%.2fms measure=%llu/%.2fms shadow=%llu/%.2fms gradient=%llu/%.2fms\n",
            traceLastWidth_,
            traceLastHeight_,
            static_cast<unsigned long long>(traceResizeMessages_),
            static_cast<unsigned long long>(traceInteractiveResizeMessages_),
            static_cast<unsigned long long>(traceMaximizeMessages_),
            static_cast<unsigned long long>(traceRestoreMessages_),
            static_cast<unsigned long long>(tracePaints_),
            static_cast<unsigned long long>(traceFullPaints_),
            static_cast<unsigned long long>(tracePartialPaints_),
            frameIntervalAvg,
            presentedFps,
            traceMinFrameIntervalMs_,
            traceMaxFrameIntervalMs_,
            static_cast<unsigned long long>(traceSurfaceAllocations_),
            paintAvg,
            contentAvg,
            blitAvg,
            traceSurfaceAllocMs_,
            static_cast<unsigned long long>(traceTextCalls_),
            traceTextMs_,
            static_cast<unsigned long long>(traceTextMeasureCalls_),
            traceTextMeasureMs_,
            static_cast<unsigned long long>(traceShadowCalls_),
            traceShadowMs_,
            static_cast<unsigned long long>(traceGradientCalls_),
            traceGradientMs_);
        if (!renderTraceFilePath_.empty()) {
            if (FILE* file = _wfopen(renderTraceFilePath_.c_str(), L"ab")) {
                std::fputs(line, file);
                std::fclose(file);
            }
        } else {
            std::fputs(line, stderr);
            std::fflush(stderr);
        }

        traceLastPrintMs_ = nowMs;
        traceResizeMessages_ = 0;
        traceInteractiveResizeMessages_ = 0;
        traceMaximizeMessages_ = 0;
        traceRestoreMessages_ = 0;
        tracePaints_ = 0;
        traceFullPaints_ = 0;
        tracePartialPaints_ = 0;
        traceFrameIntervals_ = 0;
        traceFrameIntervalMs_ = 0.0;
        traceMinFrameIntervalMs_ = 0.0;
        traceMaxFrameIntervalMs_ = 0.0;
        traceSurfaceAllocations_ = 0;
        tracePaintMs_ = 0.0;
        traceContentPaintMs_ = 0.0;
        traceBlitMs_ = 0.0;
        traceSurfaceAllocMs_ = 0.0;
        traceTextCalls_ = 0;
        traceTextMeasureCalls_ = 0;
        traceShadowCalls_ = 0;
        traceGradientCalls_ = 0;
        traceTextMs_ = 0.0;
        traceTextMeasureMs_ = 0.0;
        traceShadowMs_ = 0.0;
        traceGradientMs_ = 0.0;
    }

    void updateCursor(Point point, bool force = false) {
        if (force && hasLastCursorPoint_ && point.x == lastCursorPoint_.x && point.y == lastCursorPoint_.y) {
            SetCursor(cursorForKind(lastCursorKind_));
            return;
        }

        CursorKind next = CursorKind::Default;
        if (content_ && content_->visible()) {
            next = content_->cursor(point);
        }

        if (!force && next == lastCursorKind_) {
            lastCursorPoint_ = point;
            hasLastCursorPoint_ = true;
            return;
        }

        lastCursorKind_ = next;
        lastCursorPoint_ = point;
        hasLastCursorPoint_ = true;
        SetCursor(cursorForKind(next));
    }

    void dispatchMouseMove(Point point) {
        trackMouseLeave();
        if (!content_ || !content_->visible()) {
            clearTooltip();
            return;
        }

        updateTooltip(point);

        MouseEvent event{point};
        const bool changed = content_->onMouseMove(event);
        if (changed) {
            flushInteractivePaint();
        }
    }

    void updateTooltip(Point point) {
        const std::wstring* next = content_ ? content_->tooltipAt(point) : nullptr;
        const std::wstring nextText = next ? *next : std::wstring{};
        tooltipPoint_ = point;
        if (nextText == hoveredTooltip_) {
            return;
        }
        if (hwnd_) {
            KillTimer(hwnd_, kOneUiTooltipTimer);
        }
        hoveredTooltip_ = nextText;
        tooltipVisible_ = false;
        if (!hoveredTooltip_.empty() && hwnd_) {
            SetTimer(hwnd_, kOneUiTooltipTimer, kOneUiTooltipDelayMs, nullptr);
        }
        requestRedraw();
    }

    void clearTooltip() {
        if (hwnd_) {
            KillTimer(hwnd_, kOneUiTooltipTimer);
        }
        if (hoveredTooltip_.empty() && !tooltipVisible_) {
            return;
        }
        hoveredTooltip_.clear();
        tooltipVisible_ = false;
        requestRedraw();
    }

    void paintTooltip(Canvas& canvas) const {
        if (!tooltipVisible_ || hoveredTooltip_.empty()) {
            return;
        }
        const auto viewport = canvas.viewportBounds();
        if (!viewport) {
            return;
        }
        constexpr float fontSize = 12.0f;
        constexpr float horizontalPadding = 10.0f;
        constexpr float height = 30.0f;
        const float width = std::clamp(
            canvas.measureTextWidth(hoveredTooltip_, fontSize, 500) + horizontalPadding * 2.0f,
            44.0f,
            360.0f);
        float x = tooltipPoint_.x + 12.0f;
        float y = tooltipPoint_.y + 18.0f;
        x = std::clamp(x, viewport->x + 6.0f, viewport->x + viewport->width - width - 6.0f);
        if (y + height > viewport->y + viewport->height - 6.0f) {
            y = tooltipPoint_.y - height - 10.0f;
        }
        y = std::max(viewport->y + 6.0f, y);
        const Rect frame{x, y, width, height};
        canvas.drawBoxShadow(frame, BoxShadow{Color{0, 0, 0, 92}, Point{0.0f, 4.0f}, 12.0f, 0.0f}, 5.0f);
        canvas.fillRect(frame, Color{37, 39, 49}, 5.0f);
        canvas.strokeRect(frame, Color{78, 82, 101}, 5.0f, 1.0f);
        canvas.drawTextStyledEllipsized(
            hoveredTooltip_,
            Rect{x + horizontalPadding, y, width - horizontalPadding * 2.0f, height},
            Color{235, 237, 245},
            fontSize,
            TextAlign::Left,
            500);
    }

    void flushInteractivePaint() {
        if (!hwnd_) {
            return;
        }

        // User input and live resize feedback should be rendered in the same message turn.
        // Timer coalescing remains available for animation callbacks and non-interactive work,
        // but pointer hover/focus/pressed states need immediate feedback to feel attached.
        const bool traceScroll = internal::scrollTraceEnabled();
        const double startMs = traceScroll ? internal::scrollTraceNowMs() : 0.0;
        flushPendingPaint();
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "interactive_flush", reinterpret_cast<std::uintptr_t>(hwnd_),
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                internal::scrollTraceNowMs() - startMs, 0.0, 0.0});
        }
    }

    void scheduleInteractivePaint() {
        if (!hwnd_) {
            return;
        }

        if (!interactivePaintTimerActive_) {
            SetTimer(hwnd_, kOneUiInteractivePaintTimer, kOneUiInteractivePaintIntervalMs, nullptr);
            interactivePaintTimerActive_ = true;
        }
    }

    void flushPendingPaint() {
        if (!hwnd_) {
            return;
        }
        RECT updateRect{};
        if (GetUpdateRect(hwnd_, &updateRect, FALSE)) {
            UpdateWindow(hwnd_);
        }
    }

    void dispatchMouseDown(
        LPARAM lParam,
        MouseButton button = MouseButton::Left,
        int clickCount = 1) {
        dispatchMouse(lParam, button, clickCount, "pointer_down", [](Widget& widget, const MouseEvent& event) {
            return widget.onMouseDown(event);
        });
    }

    void dispatchMouseUp(LPARAM lParam, MouseButton button = MouseButton::Left) {
        dispatchMouse(lParam, button, 1, "pointer_up", [](Widget& widget, const MouseEvent& event) {
            return widget.onMouseUp(event);
        });
    }

    void dispatchMouseWheel(WPARAM wParam, LPARAM lParam) {
        if (!content_ || !content_->visible()) {
            return;
        }

        const bool traceScroll = internal::scrollTraceEnabled();
        const double inputStartMs = traceScroll ? internal::scrollTraceNowMs() : 0.0;
        const double inputIntervalMs = traceScroll && scrollTraceLastWheelMessageMs_ > 0.0
            ? inputStartMs - scrollTraceLastWheelMessageMs_
            : 0.0;
        if (traceScroll) {
            scrollTraceLastWheelMessageMs_ = inputStartMs;
        }

        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd_, &point);
        const short rawDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        const float delta = static_cast<float>(rawDelta) / static_cast<float>(WHEEL_DELTA);
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "wheel_received", reinterpret_cast<std::uintptr_t>(hwnd_),
                delta, 0.0, 0.0, 0.0, inputIntervalMs, 0.0, 0.0,
                static_cast<double>(rawDelta), static_cast<double>(WHEEL_DELTA)});
        }
        MouseWheelEvent event{
            logicalPointFromClientPixels(point.x, point.y),
            delta,
            (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) != 0,
            (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0,
            (GetKeyState(VK_MENU) & 0x8000) != 0,
            currentTimeMs(),
        };
        const bool handled = content_->onMouseWheel(event);
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "wheel_dispatched", reinterpret_cast<std::uintptr_t>(hwnd_),
                delta, 0.0, 0.0, 0.0, inputIntervalMs, 0.0,
                internal::scrollTraceNowMs() - inputStartMs,
                handled ? 1.0 : 0.0, 0.0});
        }
        // Scroll animations are sampled on the display-synchronized frame.
        // A synchronous paint here would present an unchanged frame, block input
        // dispatch, and shift the first useful sample away from vsync.
    }

    void dispatchFocusChanged(bool focused) {
        if (content_ && content_->onFocusChanged(focused)) {
            requestRedraw();
        }
    }

    void updateTrackedKeyState(WPARAM virtualKey, bool pressed) {
        if (virtualKey < trackedKeyState_.size()) {
            trackedKeyState_[static_cast<std::size_t>(virtualKey)] = pressed;
        }
    }

    bool trackedModifierDown(int genericKey, int leftKey, int rightKey) const {
        const auto down = [this](int virtualKey) {
            return virtualKey >= 0 &&
                   static_cast<std::size_t>(virtualKey) < trackedKeyState_.size() &&
                   trackedKeyState_[static_cast<std::size_t>(virtualKey)];
        };
        return down(genericKey) || down(leftKey) || down(rightKey);
    }

    void resetTrackedKeyState() {
        trackedKeyState_.fill(false);
    }

    KeyEvent makeKeyEvent(WPARAM wParam, LPARAM lParam, bool pressed) const {
        Key key = Key::Other;
        if (wParam == VK_TAB) {
            key = Key::Tab;
        } else if (wParam == VK_RETURN) {
            key = Key::Enter;
        } else if (wParam == VK_SPACE) {
            key = Key::Space;
        } else if (wParam == VK_BACK) {
            key = Key::Backspace;
        } else if (wParam == VK_LEFT) {
            key = Key::Left;
        } else if (wParam == VK_RIGHT) {
            key = Key::Right;
        } else if (wParam == VK_UP) {
            key = Key::Up;
        } else if (wParam == VK_DOWN) {
            key = Key::Down;
        } else if (wParam == VK_ESCAPE) {
            key = Key::Escape;
        } else if (wParam == VK_HOME) {
            key = Key::Home;
        } else if (wParam == VK_END) {
            key = Key::End;
        } else if (wParam == VK_PRIOR) {
            key = Key::PageUp;
        } else if (wParam == VK_NEXT) {
            key = Key::PageDown;
        } else if (wParam == VK_DELETE) {
            key = Key::Delete;
        } else if (wParam == VK_F2) {
            key = Key::F2;
        } else if (wParam == 'A') {
            key = Key::A;
        } else if (wParam == 'C') {
            key = Key::C;
        } else if (wParam == 'V') {
            key = Key::V;
        } else if (wParam == 'X') {
            key = Key::X;
        }

        KeyEvent event;
        event.key = key;
        event.shift = trackedModifierDown(VK_SHIFT, VK_LSHIFT, VK_RSHIFT) ||
                      (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        event.control = trackedModifierDown(VK_CONTROL, VK_LCONTROL, VK_RCONTROL) ||
                        (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        event.virtualKey = static_cast<unsigned int>(wParam);
        event.scanCode = static_cast<unsigned int>((lParam >> 16) & 0xff);
        event.pressed = pressed;
        event.repeat = pressed && ((lParam & (1LL << 30)) != 0);
        event.extended = (lParam & (1LL << 24)) != 0;
        event.alt = trackedModifierDown(VK_MENU, VK_LMENU, VK_RMENU) ||
                    (GetKeyState(VK_MENU) & 0x8000) != 0;
        event.win = trackedModifierDown(VK_LWIN, VK_LWIN, VK_RWIN) ||
                    (GetKeyState(VK_LWIN) & 0x8000) != 0 ||
                    (GetKeyState(VK_RWIN) & 0x8000) != 0;
        return event;
    }

    bool dispatchKeyDown(WPARAM wParam, LPARAM lParam) {
        updateTrackedKeyState(wParam, true);
        KeyEvent event = makeKeyEvent(wParam, lParam, true);
        if (rawKeyHandler_ && rawKeyHandler_(event)) {
            requestInteractiveRedraw();
            return true;
        }
        if (!content_ || !content_->visible()) {
            return false;
        }
        if (content_->onKeyDown(event)) {
            requestInteractiveRedraw();
            return true;
        }
        return false;
    }

    bool dispatchKeyUp(WPARAM wParam, LPARAM lParam) {
        updateTrackedKeyState(wParam, false);
        KeyEvent event = makeKeyEvent(wParam, lParam, false);
        if (rawKeyHandler_ && rawKeyHandler_(event)) {
            requestInteractiveRedraw();
            return true;
        }
        if (!content_ || !content_->visible()) {
            return false;
        }
        if (content_->onKeyUp(event)) {
            requestInteractiveRedraw();
            return true;
        }
        return false;
    }

    void dispatchTextInput(WPARAM wParam) {
        if (!content_ || !content_->visible()) {
            return;
        }

        const wchar_t character = static_cast<wchar_t>(wParam);
        if (character >= 0xD800 && character <= 0xDBFF) {
            pendingHighSurrogate_ = character;
            return;
        }

        std::wstring text;
        if (character >= 0xDC00 && character <= 0xDFFF) {
            if (pendingHighSurrogate_ != 0) {
                text.push_back(pendingHighSurrogate_);
                text.push_back(character);
                pendingHighSurrogate_ = 0;
            } else {
                text.push_back(L'\uFFFD');
            }
        } else {
            if (pendingHighSurrogate_ != 0) {
                text.push_back(L'\uFFFD');
                pendingHighSurrogate_ = 0;
            }
            if (character >= 32) {
                text.push_back(character);
            }
        }
        dispatchCommittedText(text);
    }

    void dispatchUnicodeScalar(std::uint32_t scalar) {
        if (scalar < 32 || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF)) {
            return;
        }
        std::wstring text;
        if (scalar <= 0xFFFF) {
            text.push_back(static_cast<wchar_t>(scalar));
        } else {
            scalar -= 0x10000;
            text.push_back(static_cast<wchar_t>(0xD800 + (scalar >> 10)));
            text.push_back(static_cast<wchar_t>(0xDC00 + (scalar & 0x3FF)));
        }
        dispatchCommittedText(text);
    }

    void dispatchCommittedText(const std::wstring& text) {
        if (text.empty() || !content_ || !content_->visible()) {
            return;
        }
        if (content_->onTextInputText(text)) {
            requestInteractiveRedraw();
        }
    }

    void updateImePosition() {
        if (!hwnd_ || !content_ || !content_->visible()) {
            return;
        }

        const Rect logical = content_->textInputCaretRect();
        const float scale = normalizedDpiScale();
        const LONG left = static_cast<LONG>(std::lround(logical.x * scale));
        const LONG top = static_cast<LONG>(std::lround(logical.y * scale));
        const LONG right = static_cast<LONG>(std::lround((logical.x + logical.width) * scale));
        const LONG bottom = static_cast<LONG>(std::lround((logical.y + logical.height) * scale));

        HIMC context = ImmGetContext(hwnd_);
        if (!context) {
            return;
        }

        COMPOSITIONFORM composition{};
        composition.dwStyle = CFS_POINT;
        composition.ptCurrentPos = POINT{left, bottom};
        ImmSetCompositionWindow(context, &composition);

        CANDIDATEFORM candidate{};
        candidate.dwIndex = 0;
        candidate.dwStyle = CFS_EXCLUDE;
        candidate.ptCurrentPos = POINT{left, bottom};
        candidate.rcArea = RECT{left, top, std::max(left + 1, right), std::max(top + 1, bottom)};
        ImmSetCandidateWindow(context, &candidate);
        ImmReleaseContext(hwnd_, context);
    }

    template <typename Handler>
    void dispatchMouse(
        LPARAM lParam,
        MouseButton button,
        int clickCount,
        const char* phase,
        Handler handler) {
        if (!content_ || !content_->visible()) {
            return;
        }

        MouseEvent event{logicalPointFromLParam(lParam)};
        event.button = button;
        event.shift = trackedModifierDown(VK_SHIFT, VK_LSHIFT, VK_RSHIFT) ||
                      (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        event.control = trackedModifierDown(VK_CONTROL, VK_LCONTROL, VK_RCONTROL) ||
                        (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        event.alt = trackedModifierDown(VK_MENU, VK_LMENU, VK_RMENU) ||
                    (GetKeyState(VK_MENU) & 0x8000) != 0;
        event.clickCount = clickCount;
        if (internal::scrollTraceEnabled()) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", phase, reinterpret_cast<std::uintptr_t>(hwnd_),
                static_cast<double>(static_cast<int>(button)), 0.0,
                static_cast<double>(clickCount), 0.0, 0.0, 0.0, 0.0,
                static_cast<double>(event.position.x),
                static_cast<double>(event.position.y)});
        }
        const bool handled = handler(*content_, event);
        if (internal::scrollTraceEnabled()) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", handled ? "pointer_handled" : "pointer_unhandled",
                reinterpret_cast<std::uintptr_t>(hwnd_),
                static_cast<double>(static_cast<int>(button)), 0.0,
                static_cast<double>(clickCount), 0.0, 0.0, 0.0, 0.0,
                static_cast<double>(event.position.x),
                static_cast<double>(event.position.y)});
        }
        if (handled) {
            requestInteractiveRedraw();
        }
    }

    void trackMouseLeave() {
        if (mouseLeaveTracking_ || !hwnd_) {
            return;
        }
        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(TRACKMOUSEEVENT);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = hwnd_;
        if (TrackMouseEvent(&event)) {
            mouseLeaveTracking_ = true;
        }
    }

    void initGPU() {
        if (gpuInitializationAttempted_ || !hwnd_) {
            return;
        }
        gpuInitializationAttempted_ = true;
        if (!gpuRenderingEnabled()) {
            return;
        }

        glDC_ = GetDC(hwnd_);
        if (!glDC_) return;

        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 0;
        pfd.cStencilBits = 0;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pixelFormat = ChoosePixelFormat(glDC_, &pfd);
        if (!pixelFormat || !SetPixelFormat(glDC_, pixelFormat, &pfd)) {
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        HGLRC tempContext = wglCreateContext(glDC_);
        if (!tempContext) {
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        if (!wglMakeCurrent(glDC_, tempContext)) {
            wglDeleteContext(tempContext);
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        grContext_ = GrDirectContexts::MakeGL();
        if (!grContext_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempContext);
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        const auto swapInterval = reinterpret_cast<WglSwapIntervalExtProc>(
            wglGetProcAddress("wglSwapIntervalEXT"));
        swapIntervalEnabled_ = swapInterval && swapInterval(1) == TRUE;

        glContext_ = tempContext;
        gpuAvailable_ = true;
        std::fprintf(
            stderr,
            "OneUI GPU rendering enabled (OpenGL+Skia Ganesh, vsync=%s)\n",
            swapIntervalEnabled_ ? "on" : "dwm-fallback");
    }

    void shutdownGPU() {
        windowSurface_.reset();
        paintSurface_.reset();
        grContext_.reset();
        if (glContext_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(glContext_);
            glContext_ = nullptr;
        }
        if (glDC_) {
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
        }
        gpuAvailable_ = false;
        swapIntervalEnabled_ = false;
    }

    HWND hwnd_ = nullptr;
    HANDLE animationFrameTimer_ = nullptr;
    WindowOptions options_;
    float dpiScale_ = 1.0f;
    // 无边框窗口的拖拽命中区（逻辑像素）：标题栏高度内、且不在右侧预留区，视为可拖拽 caption。
    // 客户端可按自身标题栏与窗口按钮/账号按钮的实际布局配置。
    float titleBarHeightLogical_ = 34.0f;
    float titleButtonReservedWidthLogical_ = 132.0f;
    float titleBarInteractiveLeadingWidthLogical_ = -1.0f;
    float titleBarInteractiveTrailingWidthLogical_ = -1.0f;
    ClientSizeChangedHandler clientSizeChangedHandler_;
    bool clientSizeChangedFramePending_ = false;
    HGLRC glContext_ = nullptr;
    HDC glDC_ = nullptr;
    sk_sp<GrDirectContext> grContext_;
    bool gpuInitializationAttempted_ = false;
    bool gpuAvailable_ = false;
    bool swapIntervalEnabled_ = false;
    std::shared_ptr<Widget> content_;
    RawKeyHandler rawKeyHandler_;
    std::wstring defaultFontFamily_;
    wchar_t pendingHighSurrogate_ = 0;
    std::array<bool, 256> trackedKeyState_{};
    std::atomic_bool acceptingPostedCallbacks_{true};
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
    // Win10 圆角回退（SetWindowRgn）会丢掉 DWM 柔和投影，用一个分层伴随窗口在主窗
    // 圆角轮廓外画一圈抗锯齿柔光投影补回来。Win11 走 DWM 圆角自带投影，不用它。
    HWND shadowHwnd_ = nullptr;
    bool shadowActive_ = false; // 当前是否处于“需要伴随投影”的状态（Win10 圆角且未最大化）
    int shadowBuiltW_ = 0;      // 已构建投影位图对应的主窗尺寸，尺寸不变则移动时只重定位不重绘
    int shadowBuiltH_ = 0;
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
    std::vector<std::uint32_t> blitScratch_;
};

} // namespace

void SystemClipboard::setText(std::wstring text) {
    ClipboardGuard guard;
    if (!guard.isOpen()) {
        return;
    }

    if (!EmptyClipboard()) {
        return;
    }

    const SIZE_T byteCount = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, byteCount);
    if (!memory) {
        return;
    }

    void* locked = GlobalLock(memory);
    if (!locked) {
        GlobalFree(memory);
        return;
    }

    std::memcpy(locked, text.c_str(), byteCount);
    GlobalUnlock(memory);

    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        return;
    }
}

std::wstring SystemClipboard::text() const {
    ClipboardGuard guard;
    if (!guard.isOpen()) {
        return {};
    }

    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) {
        return {};
    }

    const wchar_t* locked = static_cast<const wchar_t*>(GlobalLock(handle));
    if (!locked) {
        return {};
    }

    std::wstring result(locked);
    GlobalUnlock(handle);
    return result;
}

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
