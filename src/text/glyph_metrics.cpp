#include "glyph_metrics.h"

#include "platform/shared/skia_canvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"

#if defined(ONEUI_WIN32_GLYPH_MASK_WARMUP)
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurfaceProps.h"
#include "src/core/SkStrike.h"
#include "src/core/SkStrikeSpec.h"
#include "src/text/StrikeForGPU.h"
#endif

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oneui::text {
namespace {
struct RegisteredGlyphs {
    SkFont font;
    std::vector<SkGlyphID> glyphs;
};

std::optional<RegisteredGlyphs> registeredGlyphs(
    std::string_view text, std::string_view family, float size, int weight) {
    auto manager = rendering::makeEmbeddedFontManager();
    const std::string name(family);
    const auto style = SkFontStyle(weight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant);
    auto face = manager->matchFamilyStyle(name.c_str(), style);
    if (!face) return std::nullopt;

    // Match the strike SkParagraph uses when it asks HarfBuzz for glyph metrics.
    const skia::textlayout::TextStyle defaults;
    SkFont font(std::move(face), size);
    font.setEdging(defaults.getFontEdging());
    font.setHinting(defaults.getFontHinting());
    font.setSubpixel(defaults.getSubpixel());
    // OneLineShaper can synthesize bold if a registered family lacks this weight.
    if (skia::textlayout::ParagraphStyle{}.fakeMissingFontStyles()) {
        font.setEmbolden(weight >= SkFontStyle::kSemiBold_Weight &&
                         weight - font.getTypeface()->fontStyle().weight() >= 200);
    }

    const int count = font.countText(text.data(), text.size(), SkTextEncoding::kUTF8);
    if (count <= 0) return std::nullopt;
    std::vector<SkGlyphID> glyphs(count);
    font.textToGlyphs(text.data(), text.size(), SkTextEncoding::kUTF8, glyphs);
    // Skip the missing glyph: this hint warms the registered face and never
    // resolves fallback. SkParagraph still shapes the real text at paint time.
    glyphs.erase(std::remove(glyphs.begin(), glyphs.end(), 0), glyphs.end());
    if (glyphs.empty()) return std::nullopt;
    return RegisteredGlyphs{std::move(font), std::move(glyphs)};
}
} // namespace

bool warmRegisteredGlyphMetrics(std::string_view text, std::string_view family, float size, int weight) {
    auto run = registeredGlyphs(text, family, size, weight);
    if (!run) return false;
    std::vector<SkScalar> widths(run->glyphs.size());
    run->font.getWidths(run->glyphs, widths);
    return true;
}

#if defined(ONEUI_WIN32_GLYPH_MASK_WARMUP)
bool warmRegisteredGlyphMasks(std::string_view text, std::string_view family, float size, int weight,
                              float scale, const uint32_t* colors, std::size_t colorCount) {
    const auto run = registeredGlyphs(text, family, size, weight);
    if (!run) return false;

    const auto matrix = SkMatrix::Scale(scale, scale);
    SkPaint paint;
    paint.setAntiAlias(true);
    if (SkStrikeSpec::ShouldDrawAsPath(paint, run->font, matrix)) return false;
    bool prepared = false;
    for (std::size_t index = 0; index < colorCount; ++index) {
        paint.setColor(colors[index]);
        auto strike = SkStrikeSpec::MakeMask(run->font, paint, SkSurfaceProps(),
            SkScalerContextFlags::kFakeGammaAndBoostContrast, matrix).findOrCreateStrike();
        // Horizontal text uses up to four device-pixel positions per glyph.
        const auto mask = strike->roundingSpec().ignorePositionFieldMask;
        const int phases = mask.x() ? 4 : 1;
        for (const auto glyph : run->glyphs) {
            sktext::StrikeMutationMonitor mutation(strike.get());
            for (int phase = 0; phase < phases; ++phase) {
                const SkPackedGlyphID packed(glyph, SkPoint::Make(phase * 0.25f, 0.0f), mask);
                const auto digest = strike->digestFor(skglyph::kDirectMask, packed);
                if (digest.actionFor(skglyph::kDirectMask) == skglyph::GlyphAction::kAccept) {
                    prepared |= strike->prepareForImage(strike->glyph(digest));
                }
            }
        }
    }
    return prepared;
}
#else
bool warmRegisteredGlyphMasks(std::string_view, std::string_view, float, int, float,
                              const uint32_t*, std::size_t) {
    return false;
}
#endif
} // namespace oneui::text
