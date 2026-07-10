#include "oneui/platform/window.h"

#include "oneui/color.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <GL/gl.h>
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"

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
#include "include/effects/SkGradientShader.h"
#include "include/effects/SkImageFilters.h"
#include "include/ports/SkTypeface_win.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <functional>
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
constexpr UINT kOneUiInteractivePaintIntervalMs = 8;
constexpr int kPaintSurfaceAlignment = 128;
constexpr int kPaintSurfaceGrowthPadding = 256;
constexpr float kDefaultDpi = 96.0f;
constexpr int kProcessPerMonitorDpiAware = 2;

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

    bool operator<(const TextBlobKey& other) const {
        return std::tie(text, size, weight) < std::tie(other.text, other.size, other.weight);
    }
};

struct TextBlobEntry {
    sk_sp<SkTextBlob> blob;
    SkRect bounds = SkRect::MakeEmpty();
    SkFontMetrics metrics{};
};

bool renderTraceEnabled() {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(L"ONEUI_RENDER_TRACE", value, static_cast<DWORD>(std::size(value)));
    return length > 0 && value[0] != L'0';
}

bool gpuRenderingEnabled() {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(L"ONEUI_ENABLE_GPU", value, static_cast<DWORD>(std::size(value)));
    return length > 0 && value[0] != L'0';
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
    ClipboardGuard() : open_(OpenClipboard(clipboardOwnerWindow()) != FALSE) {}

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
    explicit SkiaCanvas(SkCanvas& canvas) : canvas_(canvas) {}

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
        const SkColor colors[2] = {toSkColor(center), toSkColor(edge)};
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setShader(SkGradientShader::MakeRadial(shaderCenter, shaderRadius, colors, nullptr, 2, SkTileMode::kClamp));
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
        if (text.empty() || rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }

        const double traceStartMs = currentTimeMs();
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));

        const TextBlobEntry& textBlob = cachedTextBlob(text, size, weight);
        if (!textBlob.blob) {
            return;
        }

        float x = rect.x;
        if (align == TextAlign::Center) {
            x = rect.x + (rect.width - textBlob.bounds.width()) / 2.0f - textBlob.bounds.left();
        } else if (align == TextAlign::Right) {
            x = rect.x + rect.width - textBlob.bounds.width() - textBlob.bounds.left();
        }

        const float baseline = rect.y + (rect.height - textBlob.metrics.fDescent - textBlob.metrics.fAscent) / 2.0f;
        canvas_.save();
        canvas_.clipRect(toSkRect(rect));
        canvas_.drawTextBlob(textBlob.blob, x, baseline, paint);
        canvas_.restore();
        ++g_primitivePaintTrace.textCalls;
        g_primitivePaintTrace.textMs += currentTimeMs() - traceStartMs;
    }

    float measureTextWidth(const std::wstring& text, float size, int weight = 400) const override {
        if (text.empty()) {
            return 0.0f;
        }
        const double traceStartMs = currentTimeMs();
        const TextBlobEntry& textBlob = cachedTextBlob(text, size, weight);
        ++g_primitivePaintTrace.textMeasureCalls;
        g_primitivePaintTrace.textMeasureMs += currentTimeMs() - traceStartMs;
        return textBlob.bounds.width();
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
    static sk_sp<SkTypeface> defaultTypeface(int weight) {
        static sk_sp<SkFontMgr> fontMgr = [] {
            auto mgr = SkFontMgr_New_DirectWrite();
            if (!mgr) {
                mgr = SkFontMgr_New_GDI();
            }
            return mgr;
        }();
        if (!fontMgr) {
            return {};
        }

        const int clampedWeight = std::clamp(weight, 100, 900);
        static std::map<int, sk_sp<SkTypeface>> cache;
        if (auto cached = cache.find(clampedWeight); cached != cache.end()) {
            return cached->second;
        }

        const SkFontStyle style(clampedWeight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant);
        if (auto face = fontMgr->legacyMakeTypeface("Microsoft YaHei", style)) {
            cache[clampedWeight] = face;
            return face;
        }
        if (auto face = fontMgr->legacyMakeTypeface("SimSun", style)) {
            cache[clampedWeight] = face;
            return face;
        }
        auto face = fontMgr->legacyMakeTypeface("Segoe UI", style);
        cache[clampedWeight] = face;
        return face;
    }

    static const TextBlobEntry& cachedTextBlob(const std::wstring& text, float size, int weight) {
        static TextBlobEntry empty;
        if (text.empty()) {
            return empty;
        }

        const TextBlobKey key{text, static_cast<int>(std::round(size * 10.0f)), std::clamp(weight, 100, 900)};
        static std::map<TextBlobKey, TextBlobEntry> cache;
        if (auto cached = cache.find(key); cached != cache.end()) {
            return cached->second;
        }
        if (cache.size() > 2048) {
            cache.clear();
        }

        SkFont font(defaultTypeface(weight), size);
        font.setSubpixel(true);
        font.setEdging(SkFont::Edging::kAntiAlias);

        TextBlobEntry entry;
        const auto byteLength = text.size() * sizeof(wchar_t);
        font.measureText(text.data(), byteLength, SkTextEncoding::kUTF16, &entry.bounds);
        font.getMetrics(&entry.metrics);
        entry.blob = SkTextBlob::MakeFromText(text.data(), byteLength, font, SkTextEncoding::kUTF16);

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
        const SkColor colors[2] = {
            toSkColor(start),
            toSkColor(end),
        };
        auto shader = SkGradientShader::MakeLinear(points, colors, nullptr, 2, SkTileMode::kClamp);
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
    std::optional<Rect> clipBounds_;
    std::vector<std::optional<Rect>> clipStack_;
};

class Win32Window final : public Window {
public:
    explicit Win32Window(WindowOptions options)
        : options_(std::move(options))
        , dpiScale_(dpiScaleForWindowHandle(nullptr))
        , renderTraceEnabled_(renderTraceEnabled())
        , renderTraceFilePath_(renderTraceFilePath()) {}

    ~Win32Window() override {
        if (hwnd_) {
            DestroyWindow(hwnd_);
        }
    }

    void setContent(std::shared_ptr<Widget> widget) override {
        content_ = std::move(widget);
        if (content_) {
            content_->setInvalidator([this] {
                requestRedraw();
            });
            content_->setRectInvalidator([this](Rect rect) {
                requestRedrawRect(rect);
            });
            content_->setAnimationScheduler([this] {
                scheduleContentAnimationFrame();
            });
        }
        requestRedraw();
    }

    void show() override {
        ensureCreated();
        ShowWindow(hwnd_, SW_SHOW);
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
    }

    int run() override {
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
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

    void post(std::function<void()> callback) override {
        if (!callback) {
            return;
        }

        ensureCreated();
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            postedCallbacks_.push(std::move(callback));
        }
        PostMessageW(hwnd_, kOneUiRunPostedCallbacks, 0, 0);
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
        if (!animationTimerActive_) {
            SetTimer(hwnd_, kOneUiAnimationTimer, 16, nullptr);
            animationTimerActive_ = true;
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

private:
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
            // Win11 由 DWM 负责圆角，清掉可能残留的区域，避免双重裁剪。
            SetWindowRgn(hwnd_, nullptr, TRUE);
            return;
        }
        // Win10 回退：用圆角矩形区域裁剪窗口。
        if (square) {
            SetWindowRgn(hwnd_, nullptr, TRUE);
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
        if (hwnd_) {
            return;
        }

        ensureProcessDpiAwareness();
        dpiScale_ = dpiScaleForWindowHandle(nullptr);

        HINSTANCE instance = GetModuleHandleW(nullptr);
        const wchar_t* className = L"OneUIWindow";

        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = &Win32Window::windowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = nullptr;
        // CS_DROPSHADOW：SetWindowRgn 裁圆角后 DWM 标准投影会消失，这个类级投影能与
        // 自定义窗口区域共存，为圆角窗口补回一层投影（浅色桌面下看清边缘）。
        windowClass.style = CS_DROPSHADOW;
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
            initGPU();
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
        case WM_SIZE:
            recordResizeMessage(wParam);
            if (wParam != SIZE_MINIMIZED) {
                applyRoundedCorners(); // 尺寸变化后重算圆角区域，避免拉伸/露白
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
        case WM_SETFOCUS:
            dispatchFocusChanged(true);
            return 0;
        case WM_KILLFOCUS:
            dispatchFocusChanged(false);
            return 0;
        case WM_KEYDOWN:
            dispatchKeyDown(wParam, lParam);
            return 0;
        case WM_KEYUP:
            dispatchKeyUp(wParam, lParam);
            return 0;
        case WM_CHAR:
            dispatchTextInput(wParam);
            return 0;
        case WM_DESTROY:
            return 0;
        case WM_NCDESTROY:
        {
            shutdownGPU();
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
        if (hwnd_ && animationTimerActive_) {
            KillTimer(hwnd_, kOneUiAnimationTimer);
            animationTimerActive_ = false;
        }

        std::queue<std::function<void(double)>> callbacks;
        {
            std::lock_guard<std::mutex> lock(animationCallbacksMutex_);
            callbacks.swap(animationCallbacks_);
        }

        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const double nowMs = std::chrono::duration<double, std::milli>(now).count();
        while (!callbacks.empty()) {
            auto callback = std::move(callbacks.front());
            callbacks.pop();
            callback(nowMs);
        }
        flushPendingPaint();
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
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        PAINTSTRUCT paintStruct{};
        HDC dc = nullptr;
        RECT dirtyRect{};
        if (useGPU) {
            dc = glDC_;
            dirtyRect = clientRect;
        } else {
            dc = BeginPaint(hwnd_, &paintStruct);
            dirtyRect = paintStruct.rcPaint;
        }
        const int width = std::max<LONG>(1, clientRect.right - clientRect.left);
        const int height = std::max<LONG>(1, clientRect.bottom - clientRect.top);
        const float scale = normalizedDpiScale();
        const float logicalWidth = static_cast<float>(width) / scale;
        const float logicalHeight = static_cast<float>(height) / scale;

        const double surfaceStartMs = currentTimeMs();
        const bool allocatedSurface = ensurePaintSurfaceCapacity(width, height);
        const double surfaceMs = currentTimeMs() - surfaceStartMs;

        if (!paintSurface_) {
            if (!useGPU) EndPaint(hwnd_, &paintStruct);
            return;
        }
        const int dirtyX = std::clamp<LONG>(dirtyRect.left, 0, width);
        const int dirtyY = std::clamp<LONG>(dirtyRect.top, 0, height);
        const int dirtyRight = std::clamp<LONG>(dirtyRect.right, dirtyX, width);
        const int dirtyBottom = std::clamp<LONG>(dirtyRect.bottom, dirtyY, height);
        const int dirtyWidth = dirtyRight - dirtyX;
        const int dirtyHeight = dirtyBottom - dirtyY;
        if (dirtyWidth <= 0 || dirtyHeight <= 0) {
            if (!useGPU) EndPaint(hwnd_, &paintStruct);
            return;
        }

        SkCanvas* skCanvas = paintSurface_->getCanvas();
        SkiaCanvas rawCanvas(*skCanvas);
        const bool fullPaint = dirtyX == 0 && dirtyY == 0 && dirtyWidth == width && dirtyHeight == height;
        if (fullPaint) {
            rawCanvas.clear(colors::Surface);
        }

        skCanvas->save();
        skCanvas->scale(scale, scale);
        SkiaCanvas canvas(*skCanvas);
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
            recordContentPaint(currentTimeMs() - contentStartMs);
            recordPrimitivePaint(g_primitivePaintTrace);
        }

        if (!fullPaint) {
            canvas.restore();
        }
        skCanvas->restore();

        if (gpuAvailable_ && grContext_) {
            grContext_->flushAndSubmit();
            if (SwapBuffers(glDC_)) {
                return;
            }
            gpuAvailable_ = false;
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

        if (!useGPU) EndPaint(hwnd_, &paintStruct);
        recordPaint(width, height, fullPaint, allocatedSurface, surfaceMs, currentTimeMs() - paintStartMs);
    }

    static int alignedPaintSurfaceSize(int requested) {
        const int padded = std::max(1, requested + kPaintSurfaceGrowthPadding);
        return ((padded + kPaintSurfaceAlignment - 1) / kPaintSurfaceAlignment) * kPaintSurfaceAlignment;
    }

    bool ensurePaintSurfaceCapacity(int width, int height) {
        if (!gpuAvailable_ && paintSurface_ && paintSurfaceWidth_ >= width && paintSurfaceHeight_ >= height) {
            return false;
        }

        int surfaceWidth = alignedPaintSurfaceSize(width);
        int surfaceHeight = alignedPaintSurfaceSize(height);
        SkImageInfo imageInfo = SkImageInfo::Make(surfaceWidth, surfaceHeight, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        if (gpuAvailable_ && grContext_) {
            surfaceWidth = width;
            surfaceHeight = height;
            imageInfo = SkImageInfo::Make(surfaceWidth, surfaceHeight, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
            GrGLFramebufferInfo fbInfo;
            fbInfo.fFBOID = 0;
            fbInfo.fFormat = GL_RGBA8;
            auto backendRT = GrBackendRenderTargets::MakeGL(surfaceWidth, surfaceHeight, 0, 0, fbInfo);
            paintSurface_ = SkSurfaces::WrapBackendRenderTarget(
                grContext_.get(), backendRT, kBottomLeft_GrSurfaceOrigin,
                kBGRA_8888_SkColorType, nullptr, nullptr);
            if (paintSurface_) {
                paintSurfaceWidth_ = surfaceWidth;
                paintSurfaceHeight_ = surfaceHeight;
                return true;
            }
            gpuAvailable_ = false;
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
        if (!renderTraceEnabled_) {
            return;
        }
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
        char line[1536]{};
        std::snprintf(
            line,
            sizeof(line),
            "[oneui-render] size=%dx%d wm_size=%llu interactive_size=%llu max=%llu restore=%llu paints=%llu full=%llu partial=%llu surface_alloc=%llu avg_paint=%.2fms avg_content=%.2fms avg_blit=%.2fms surface_alloc_ms=%.2f text=%llu/%.2fms measure=%llu/%.2fms shadow=%llu/%.2fms gradient=%llu/%.2fms\n",
            traceLastWidth_,
            traceLastHeight_,
            static_cast<unsigned long long>(traceResizeMessages_),
            static_cast<unsigned long long>(traceInteractiveResizeMessages_),
            static_cast<unsigned long long>(traceMaximizeMessages_),
            static_cast<unsigned long long>(traceRestoreMessages_),
            static_cast<unsigned long long>(tracePaints_),
            static_cast<unsigned long long>(traceFullPaints_),
            static_cast<unsigned long long>(tracePartialPaints_),
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
            return;
        }

        MouseEvent event{point};
        const bool changed = content_->onMouseMove(event);
        if (changed) {
            flushInteractivePaint();
        }
    }

    void flushInteractivePaint() {
        if (!hwnd_) {
            return;
        }

        // User input and live resize feedback should be rendered in the same message turn.
        // Timer coalescing remains available for animation callbacks and non-interactive work,
        // but pointer hover/focus/pressed states need immediate feedback to feel attached.
        flushPendingPaint();
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

    void dispatchMouseDown(LPARAM lParam, MouseButton button = MouseButton::Left) {
        dispatchMouse(lParam, button, [](Widget& widget, const MouseEvent& event) {
            return widget.onMouseDown(event);
        });
    }

    void dispatchMouseUp(LPARAM lParam, MouseButton button = MouseButton::Left) {
        dispatchMouse(lParam, button, [](Widget& widget, const MouseEvent& event) {
            return widget.onMouseUp(event);
        });
    }

    void dispatchMouseWheel(WPARAM wParam, LPARAM lParam) {
        if (!content_ || !content_->visible()) {
            return;
        }

        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd_, &point);
        const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
        MouseWheelEvent event{logicalPointFromClientPixels(point.x, point.y), delta};
        if (content_->onMouseWheel(event)) {
            requestInteractiveRedraw();
        }
    }

    void dispatchFocusChanged(bool focused) {
        if (content_ && content_->onFocusChanged(focused)) {
            requestRedraw();
        }
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
        } else if (wParam == VK_DELETE) {
            key = Key::Delete;
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
        event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        event.control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        event.virtualKey = static_cast<unsigned int>(wParam);
        event.scanCode = static_cast<unsigned int>((lParam >> 16) & 0xff);
        event.pressed = pressed;
        event.repeat = pressed && ((lParam & (1LL << 30)) != 0);
        event.extended = (lParam & (1LL << 24)) != 0;
        event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        event.win = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
        return event;
    }

    void dispatchKeyDown(WPARAM wParam, LPARAM lParam) {
        if (!content_ || !content_->visible()) {
            return;
        }

        KeyEvent event = makeKeyEvent(wParam, lParam, true);
        if (content_->onKeyDown(event)) {
            requestInteractiveRedraw();
        }
    }

    void dispatchKeyUp(WPARAM wParam, LPARAM lParam) {
        if (!content_ || !content_->visible()) {
            return;
        }

        KeyEvent event = makeKeyEvent(wParam, lParam, false);
        if (content_->onKeyUp(event)) {
            requestInteractiveRedraw();
        }
    }

    void dispatchTextInput(WPARAM wParam) {
        if (!content_ || !content_->visible()) {
            return;
        }

        const wchar_t character = static_cast<wchar_t>(wParam);
        if (character < 32) {
            return;
        }

        if (content_->onTextInput(character)) {
            requestInteractiveRedraw();
        }
    }

    template <typename Handler>
    void dispatchMouse(LPARAM lParam, MouseButton button, Handler handler) {
        if (!content_ || !content_->visible()) {
            return;
        }

        MouseEvent event{logicalPointFromLParam(lParam)};
        event.button = button;
        if (handler(*content_, event)) {
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

        glContext_ = tempContext;
        gpuAvailable_ = true;
        std::fprintf(stderr, "OneUI GPU rendering enabled (OpenGL+Skia Ganesh)\n");
    }

    void shutdownGPU() {
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
    }

    HWND hwnd_ = nullptr;
    WindowOptions options_;
    float dpiScale_ = 1.0f;
    // 无边框窗口的拖拽命中区（逻辑像素）：标题栏高度内、且不在右侧预留区，视为可拖拽 caption。
    // 客户端可按自身标题栏与窗口按钮/账号按钮的实际布局配置。
    float titleBarHeightLogical_ = 34.0f;
    float titleButtonReservedWidthLogical_ = 132.0f;
    HGLRC glContext_ = nullptr;
    HDC glDC_ = nullptr;
    sk_sp<GrDirectContext> grContext_;
    bool gpuAvailable_ = false;
    std::shared_ptr<Widget> content_;
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
    float cornerRadiusLogical_ = 0.0f;
    bool closeToTray_ = false;
    bool mouseLeaveTracking_ = false;
    bool animationTimerActive_ = false;
    bool contentAnimationFramePending_ = false;
    bool interactivePaintTimerActive_ = false;
    bool applyingWindowState_ = false;
    bool windowStatePaintPending_ = false;
    bool deferredFullPaintPending_ = false;
    bool interactiveResizeActive_ = false;
    bool renderTraceEnabled_ = false;
    std::wstring renderTraceFilePath_;
    CursorKind lastCursorKind_ = CursorKind::Default;
    Point lastCursorPoint_{};
    bool hasLastCursorPoint_ = false;
    sk_sp<SkSurface> paintSurface_;
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
    std::uint64_t traceSurfaceAllocations_ = 0;
    std::uint64_t traceTextCalls_ = 0;
    std::uint64_t traceTextMeasureCalls_ = 0;
    std::uint64_t traceShadowCalls_ = 0;
    std::uint64_t traceGradientCalls_ = 0;
    double traceLastPrintMs_ = 0.0;
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
