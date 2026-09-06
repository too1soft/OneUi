#include "skia_canvas.h"
#include "internal/unicode.h"
#include "skia_path.h"
#include "text/text_layout.h"

#include "include/core/SkBlurTypes.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
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

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

namespace oneui::rendering {
thread_local PrimitivePaintTrace g_primitivePaintTrace;
namespace {
SkColor toSkColor(Color c) { return SkColorSetARGB(c.a, c.r, c.g, c.b); }
SkRect toSkRect(Rect r) { return SkRect::MakeXYWH(r.x, r.y, r.width, r.height); }
double currentTimeMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
constexpr SkTextEncoding wideEncoding =
    sizeof(wchar_t) == 2 ? SkTextEncoding::kUTF16 : SkTextEncoding::kUTF32;

struct GradientShaderKey {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int angle = 0;
    SkColor start = SK_ColorTRANSPARENT;
    SkColor end = SK_ColorTRANSPARENT;

    bool operator<(const GradientShaderKey &other) const {
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

    bool operator<(const GradientImageKey &other) const {
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

    bool operator<(const ShadowImageKey &other) const {
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

    bool operator<(const TextBlobKey &other) const {
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

class SkiaCanvasImpl final : public Canvas {
  public:
    explicit SkiaCanvasImpl(SkCanvas &canvas, const std::wstring *defaultFontFamily = nullptr,
                            std::optional<Rect> viewportBounds = std::nullopt)
        : canvas_(canvas), defaultFontFamily_(defaultFontFamily), viewportBounds_(viewportBounds) {}

    void clear(Color color) override { canvas_.clear(toSkColor(color)); }

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

    std::optional<Rect> clipBounds() const override { return clipBounds_; }

    std::optional<Rect> viewportBounds() const override { return viewportBounds_; }

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

    void fillRadialGradient(Rect rect, Color center, Color edge, Point centerNorm, float radiusNorm,
                            float radius) override {
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }
        const float shaderRadius = std::max(rect.width, rect.height) * std::max(0.01f, radiusNorm);
        const SkPoint shaderCenter =
            SkPoint::Make(rect.x + rect.width * centerNorm.x, rect.y + rect.height * centerNorm.y);
        const SkColor4f colors[2] = {
            SkColor4f::FromColor(toSkColor(center)),
            SkColor4f::FromColor(toSkColor(edge)),
        };
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setShader(SkGradientShader::MakeRadial(shaderCenter, shaderRadius, colors, nullptr, nullptr, 2,
                                                     SkTileMode::kClamp, SkGradientShader::Interpolation{},
                                                     nullptr));
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

    void strokePath(const CanvasPath &path, Color color, float width, bool rounded) override {
        if (path.empty()) {
            return;
        }
        SkPath native = toSkPath(path);
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(width);
        paint.setStrokeCap(rounded ? SkPaint::kRound_Cap : SkPaint::kButt_Cap);
        paint.setStrokeJoin(rounded ? SkPaint::kRound_Join : SkPaint::kMiter_Join);
        canvas_.drawPath(native, paint);
    }

    void fillPathLinearGradient(const CanvasPath &path, Rect bounds, Color start, Color end,
                                float angleDegrees) override {
        if (path.empty() || bounds.width <= 0.0f || bounds.height <= 0.0f) {
            return;
        }
        SkPath native = toSkPath(path);
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setShader(linearGradientShader(bounds, start, end, angleDegrees));
        canvas_.drawPath(native, paint);
    }

    void drawBoxShadow(Rect rect, const BoxShadow &shadow, float radius) override {
        if (shadow.color.a == 0 || rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }

        const double traceStartMs = currentTimeMs();
        const float spread = shadow.spreadRadius;
        const Rect shadowRect{rect.x + shadow.offset.x - spread, rect.y + shadow.offset.y - spread,
                              rect.width + spread * 2.0f, rect.height + spread * 2.0f};
        if (shadowRect.width <= 0.0f || shadowRect.height <= 0.0f) {
            return;
        }

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(shadow.color));
        const float shadowRadius = std::max(0.0f, radius + spread);
        if (shadow.blurRadius > 0.0f) {
            const auto cached = shadowImage(shadowRect.width, shadowRect.height, shadowRadius,
                                            shadow.blurRadius, shadow.spreadRadius, shadow.color);
            if (cached.image) {
                canvas_.drawImage(cached.image, shadowRect.x - static_cast<float>(cached.pad),
                                  shadowRect.y - static_cast<float>(cached.pad));
                ++g_primitivePaintTrace.shadowCalls;
                g_primitivePaintTrace.shadowMs += currentTimeMs() - traceStartMs;
                return;
            }
            paint.setMaskFilter(shadowMaskFilter(shadow.blurRadius));
        }

        canvas_.save();
        canvas_.clipRect(toSkRect(
            Rect{shadowRect.x - shadow.blurRadius * 2.0f, shadowRect.y - shadow.blurRadius * 2.0f,
                 shadowRect.width + shadow.blurRadius * 4.0f, shadowRect.height + shadow.blurRadius * 4.0f}));
        canvas_.drawRRect(SkRRect::MakeRectXY(toSkRect(shadowRect), shadowRadius, shadowRadius), paint);
        canvas_.restore();
        ++g_primitivePaintTrace.shadowCalls;
        g_primitivePaintTrace.shadowMs += currentTimeMs() - traceStartMs;
    }

    void drawText(const std::wstring &text, Rect rect, Color color, float size,
                  TextAlign align = TextAlign::Center) override {
        drawTextStyled(text, rect, color, size, align, 400);
    }

    void drawTextStyled(const std::wstring &text, Rect rect, Color color, float size,
                        TextAlign align = TextAlign::Center, int weight = 400) override {
        drawTextStyledWithFont(text, rect, color, size, align, TextFontFamily::Default, weight);
    }

    void drawTextStyledWithFont(const std::wstring &text, Rect rect, Color color, float size, TextAlign align,
                                TextFontFamily family, int weight = 400) override {
        drawTextStyledWithNamedFont(text, rect, color, size, align, {}, family, weight);
    }

    void drawTextStyledWithNamedFont(const std::wstring& value, Rect bounds, Color color, float size,
                                     TextAlign align, const std::wstring& family,
                                     TextFontFamily fallback, int weight = 400) override {
        if (value.empty() || bounds.width <= 0 || bounds.height <= 0) return;
        const double started = currentTimeMs();
        text::LayoutOptions options;
        options.size = size; options.weight = weight; options.fallbackFamily = fallback;
        options.family = family.empty() && fallback == TextFontFamily::Default ? defaultFontFamily() : family;
        auto layout = text::Layout::make(value, options);
        float x = bounds.x;
        if (align == TextAlign::Center) x += (bounds.width - layout->width()) / 2;
        else if (align == TextAlign::Right) x += bounds.width - layout->width();
        canvas_.save();
        canvas_.clipRect(toSkRect(bounds));
        layout->paint(canvas_, {x, bounds.y + (bounds.height - layout->height()) / 2}, color);
        canvas_.restore();
        ++g_primitivePaintTrace.textCalls;
        g_primitivePaintTrace.textMs += currentTimeMs() - started;
    }

    std::wstring defaultFontFamily() const override {
        return defaultFontFamily_ ? *defaultFontFamily_ : std::wstring{};
    }

    void drawTextBlock(const std::wstring& value, Rect bounds, Color color, const TextBlockStyle& style) override {
        text::LayoutOptions options;
        options.text = style.options;
        options.family = style.fontFamily.empty() ? defaultFontFamily() : style.fontFamily;
        options.fallbackFamily = style.fallbackFamily;
        options.size = style.fontSize; options.weight = style.fontWeight;
        options.lineHeight = style.lineHeight; options.width = std::max(0.0f, bounds.width);
        options.align = style.align; options.maxLines = style.maxLines;
        options.ellipsis = style.ellipsis; options.sensitive = style.sensitive;
        options.scale = style.dpiScale;
        const double started = currentTimeMs();
        text::Layout::make(value, options)->paint(canvas_, {bounds.x, bounds.y}, color);
        ++g_primitivePaintTrace.textCalls;
        g_primitivePaintTrace.textMs += currentTimeMs() - started;
    }

    void drawTextCells(const std::wstring &text, Rect rect, Color color, float size,
                                     TextAlign align, const std::wstring &familyName,
                                     TextFontFamily fallbackFamily, int weight = 400) override {
        if (text.empty() || rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }

        const double traceStartMs = currentTimeMs();
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));

        const std::wstring &resolvedFamily = familyName.empty() &&
                                                     fallbackFamily == TextFontFamily::Default &&
                                                     defaultFontFamily_ && !defaultFontFamily_->empty()
                                                 ? *defaultFontFamily_
                                                 : familyName;
        const TextBlobEntry &textBlob = cachedTextBlob(text, size, fallbackFamily, resolvedFamily, weight);
        if (textBlob.runs.empty()) {
            return;
        }

        float x = rect.x;
        if (align == TextAlign::Center) {
            x = rect.x + (rect.width - textBlob.advanceWidth) / 2.0f - textBlob.bounds.left();
        } else if (align == TextAlign::Right) {
            x = rect.x + rect.width - textBlob.advanceWidth - textBlob.bounds.left();
        }

        const float baseline =
            rect.y + (rect.height - textBlob.metrics.fDescent - textBlob.metrics.fAscent) / 2.0f;
        canvas_.save();
        canvas_.clipRect(toSkRect(rect));
        for (const auto &run : textBlob.runs) {
            if (run.blob) {
                canvas_.drawTextBlob(run.blob, x + run.x, baseline, paint);
            }
        }
        canvas_.restore();
        ++g_primitivePaintTrace.textCalls;
        g_primitivePaintTrace.textMs += currentTimeMs() - traceStartMs;
    }

    bool supportsNamedFont(const std::wstring &familyName) const override {
        static std::mutex cacheMutex;
        static thread_local std::map<std::wstring, bool> cache;
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            if (const auto found = cache.find(familyName); found != cache.end()) {
                return found->second;
            }
        }
        const std::string requested = utf8FontFamily(familyName);
        bool available = false;
        if (!requested.empty()) {
            const auto embedded = makeEmbeddedFontManager();
            auto face = embedded ? embedded->matchFamilyStyle(requested.c_str(), SkFontStyle()) : nullptr;
            if (!face) {
                const auto manager = fontManager();
                face = manager ? manager->matchFamilyStyle(requested.c_str(), SkFontStyle()) : nullptr;
            }
            if (face) {
                SkString actual;
                face->getFamilyName(&actual);
                // Embedded fonts may be registered under an application alias,
                // so a successful provider match is authoritative even when the
                // font's legacy family name differs from that alias.
                available = embedded && embedded->matchFamilyStyle(requested.c_str(), SkFontStyle())
                    ? true
                    : actual.equals(requested.c_str());
            }
        }
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            if (cache.size() > 128) {
                cache.clear();
            }
            cache.emplace(familyName, available);
        }
        return available;
    }

    float measureTextWidth(const std::wstring &text, float size, int weight = 400) const override {
        return measureTextWidthWithFont(text, size, TextFontFamily::Default, weight);
    }

    std::vector<float> measureTextPrefixWidths(const std::wstring &text, float size,
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
            const auto face =
                typefaceForCodepoint(TextFontFamily::Default, weight, familyName, span.codepoint, primary);
            SkFont font(face ? face : primary, size);
            font.setSubpixel(true);
            font.setEdging(SkFont::Edging::kAntiAlias);
            advance += font.measureText(text.data() + offset, length * sizeof(wchar_t), wideEncoding);
            for (std::size_t index = 1; index <= length; ++index) {
                widths[offset + index] = index == length ? advance : widths[offset];
            }
            offset += length;
        }
        return widths;
    }

    float measureTextWidthWithFont(const std::wstring &text, float size, TextFontFamily family,
                                   int weight = 400) const override {
        return measureTextWidthWithNamedFont(text, size, {}, family, weight);
    }

    float measureTextWidthWithNamedFont(const std::wstring& value, float size,
                                        const std::wstring& family, TextFontFamily fallback,
                                        int weight = 400) const override {
        text::LayoutOptions options;
        options.size = size; options.weight = weight; options.fallbackFamily = fallback;
        options.family = family.empty() && fallback == TextFontFamily::Default ? defaultFontFamily() : family;
        const double started = currentTimeMs();
        const float width = text::Layout::make(value, options)->width();
        ++g_primitivePaintTrace.textMeasureCalls;
        g_primitivePaintTrace.textMeasureMs += currentTimeMs() - started;
        return width;
    }

    void drawPixels(Rect rect, const std::uint8_t *pixels, int width, int height, int stride,
                    CanvasPixelFormat format) override {
        if (!pixels || width <= 0 || height <= 0 || stride <= 0 || rect.width <= 0.0f ||
            rect.height <= 0.0f) {
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
        static auto manager = makePlatformFontManager();
        return manager;
    }

    static std::string utf8FontFamily(const std::wstring &familyName) { return unicode::toUtf8(familyName); }

    static sk_sp<SkTypeface> typeface(TextFontFamily family, int weight,
                                      const std::wstring &familyName = {}) {
        const auto fontMgr = fontManager();
        if (!fontMgr) {
            return {};
        }

        const int clampedWeight = std::clamp(weight, 100, 900);
        using TypefaceKey = std::tuple<TextFontFamily, int, std::wstring>;
        static thread_local std::map<TypefaceKey, sk_sp<SkTypeface>> cache;
        const TypefaceKey cacheKey{family, clampedWeight, familyName};
        if (auto cached = cache.find(cacheKey); cached != cache.end()) {
            return cached->second;
        }

        const SkFontStyle style(clampedWeight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant);
        if (const std::string requested = utf8FontFamily(familyName); !requested.empty()) {
            const auto embedded = makeEmbeddedFontManager();
            if (auto face = embedded ? embedded->matchFamilyStyle(requested.c_str(), style) : nullptr;
                face && (family != TextFontFamily::Monospace || face->isFixedPitch())) {
                cache[cacheKey] = face;
                return face;
            }
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
            for (const char *candidate :
                 {"JetBrains Mono", "Cascadia Mono", "Cascadia Code", "Consolas", "Courier New", "NSimSun",
                  "Menlo", "DejaVu Sans Mono", "monospace"}) {
                if (auto face = fontMgr->matchFamilyStyle(candidate, style); face && face->isFixedPitch()) {
                    cache[cacheKey] = face;
                    return face;
                }
            }
        }
#ifndef _WIN32
        if (auto face = fontMgr->matchFamilyStyle(nullptr, style)) {
            cache[cacheKey] = face;
            return face;
        }
#endif
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

    static CodepointSpan codepointAt(const std::wstring &text, std::size_t offset) {
        if (offset >= text.size()) {
            return {};
        }
#if WCHAR_MAX <= 0xFFFF
        const auto first = static_cast<std::uint16_t>(text[offset]);
        if (first >= 0xD800 && first <= 0xDBFF && offset + 1 < text.size()) {
            const auto second = static_cast<std::uint16_t>(text[offset + 1]);
            if (second >= 0xDC00 && second <= 0xDFFF) {
                return {static_cast<SkUnichar>(0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00)),
                        offset, 2};
            }
        }
#endif
        return {static_cast<SkUnichar>(text[offset]), offset, 1};
    }

    static sk_sp<SkTypeface> typefaceForCodepoint(TextFontFamily family, int weight,
                                                  const std::wstring &familyName, SkUnichar codepoint,
                                                  const sk_sp<SkTypeface> &primary) {
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
            for (const char *candidate : {"NSimSun", "Microsoft YaHei Mono"}) {
                if (auto face = fontMgr->matchFamilyStyle(candidate, style);
                    face && face->isFixedPitch() && face->unicharToGlyph(codepoint) != 0) {
                    return face;
                }
            }
        }
        const char *locales[] = {"zh-CN", "en-US"};
        auto fallback = fontMgr->matchFamilyStyleCharacter(nullptr, style, locales,
                                                           static_cast<int>(std::size(locales)), codepoint);
        return fallback ? fallback : primary;
    }

    static const TextBlobEntry &cachedTextBlob(const std::wstring &text, float size, TextFontFamily family,
                                               const std::wstring &familyName, int weight) {
        static TextBlobEntry empty;
        if (text.empty()) {
            return empty;
        }

        const TextBlobKey key{text, static_cast<int>(std::round(size * 10.0f)), std::clamp(weight, 100, 900),
                              family, familyName};
        static thread_local std::map<TextBlobKey, TextBlobEntry> cache;
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

        auto appendRun = [&](std::size_t start, std::size_t end, const sk_sp<SkTypeface> &face) {
            if (end <= start || !face) {
                return;
            }
            SkFont font(face, size);
            font.setSubpixel(true);
            font.setEdging(SkFont::Edging::kAntiAlias);

            const auto byteLength = (end - start) * sizeof(wchar_t);
            SkRect runBounds = SkRect::MakeEmpty();
            const float runAdvance =
                font.measureText(text.data() + start, byteLength, wideEncoding, &runBounds);
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

            entry.runs.push_back(
                {SkTextBlob::MakeFromText(text.data() + start, byteLength, font, wideEncoding),
                 entry.advanceWidth});
            entry.advanceWidth += runAdvance;
        };

        for (std::size_t offset = 0; offset < text.size();) {
            const CodepointSpan span = codepointAt(text, offset);
            const auto face = typefaceForCodepoint(family, weight, familyName, span.codepoint, primary);
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
        static thread_local std::map<int, sk_sp<SkMaskFilter>> cache;
        if (auto cached = cache.find(key); cached != cache.end()) {
            return cached->second;
        }

        auto filter = SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma, false);
        cache[key] = filter;
        return filter;
    }

    static ShadowImageEntry shadowImage(float width, float height, float radius, float blurRadius,
                                        float spreadRadius, Color color) {
        if (width <= 0.0f || height <= 0.0f || blurRadius <= 0.0f || color.a == 0) {
            return {};
        }

        const ShadowImageKey key{static_cast<int>(std::ceil(width)),
                                 static_cast<int>(std::ceil(height)),
                                 static_cast<int>(std::round(radius * 10.0f)),
                                 static_cast<int>(std::round(blurRadius * 10.0f)),
                                 static_cast<int>(std::round(spreadRadius * 10.0f)),
                                 toSkColor(color)};

        static thread_local std::map<ShadowImageKey, ShadowImageEntry> cache;
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

        SkCanvas *shadowCanvas = surface->getCanvas();
        shadowCanvas->clear(SK_ColorTRANSPARENT);

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(toSkColor(color));
        paint.setMaskFilter(shadowMaskFilter(blurRadius));
        const float shadowRadius = std::max(0.0f, radius);
        shadowCanvas->drawRRect(SkRRect::MakeRectXY(SkRect::MakeXYWH(static_cast<float>(pad),
                                                                     static_cast<float>(pad), width, height),
                                                    shadowRadius, shadowRadius),
                                paint);

        ShadowImageEntry entry{surface->makeImageSnapshot(), pad};
        cache[key] = entry;
        return entry;
    }

    static sk_sp<SkShader> linearGradientShader(Rect rect, Color start, Color end, float angleDegrees) {
        const GradientShaderKey key{static_cast<int>(std::round(rect.x * 10.0f)),
                                    static_cast<int>(std::round(rect.y * 10.0f)),
                                    static_cast<int>(std::round(rect.width * 10.0f)),
                                    static_cast<int>(std::round(rect.height * 10.0f)),
                                    static_cast<int>(std::round(angleDegrees * 10.0f)),
                                    toSkColor(start),
                                    toSkColor(end)};

        static thread_local std::map<GradientShaderKey, sk_sp<SkShader>> cache;
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
        auto shader = SkGradientShader::MakeLinear(points, colors, nullptr, nullptr, 2, SkTileMode::kClamp,
                                                   SkGradientShader::Interpolation{}, nullptr);
        cache[key] = shader;
        return shader;
    }

    static sk_sp<SkImage> gradientImage(float width, float height, float radius, Color start, Color end,
                                        float angleDegrees) {
        if (width <= 0.0f || height <= 0.0f) {
            return nullptr;
        }

        const GradientImageKey key{static_cast<int>(std::ceil(width)),
                                   static_cast<int>(std::ceil(height)),
                                   static_cast<int>(std::round(radius * 10.0f)),
                                   static_cast<int>(std::round(angleDegrees * 10.0f)),
                                   toSkColor(start),
                                   toSkColor(end)};

        static thread_local std::map<GradientImageKey, sk_sp<SkImage>> cache;
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

        SkCanvas *gradientCanvas = surface->getCanvas();
        gradientCanvas->clear(SK_ColorTRANSPARENT);

        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setShader(linearGradientShader(Rect{0.0f, 0.0f, width, height}, start, end, angleDegrees));
        gradientCanvas->drawRRect(
            SkRRect::MakeRectXY(SkRect::MakeXYWH(0.0f, 0.0f, width, height), radius, radius), paint);

        auto image = surface->makeImageSnapshot();
        cache[key] = image;
        return image;
    }

    SkCanvas &canvas_;
    const std::wstring *defaultFontFamily_ = nullptr;
    std::optional<Rect> viewportBounds_;
    std::optional<Rect> clipBounds_;
    std::vector<std::optional<Rect>> clipStack_;
};

} // namespace

std::unique_ptr<Canvas> makeSkiaCanvas(SkCanvas &canvas, const std::wstring *family,
                                       std::optional<Rect> viewport) {
    return std::make_unique<SkiaCanvasImpl>(canvas, family, viewport);
}
} // namespace oneui::rendering
