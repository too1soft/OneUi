#include "glyph_metrics.h"

#include "platform/shared/skia_canvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"

#if defined(_WIN32)
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurfaceProps.h"
#include "src/core/SkStrike.h"
#include "src/core/SkStrikeSpec.h"
#include "src/text/StrikeForGPU.h"
#include <array>
#include <cstdlib>
#endif

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace oneui::text {
#if defined(_WIN32)
namespace {
void warmDrawMetrics(const SkFont& font, SkSpan<const SkGlyphID> glyphs) {
    const char* enabled = std::getenv("ONEUI_QA_WARM_DRAW_GLYPH_METRICS");
    const std::string_view version = enabled ? enabled : "";
    if (version != "1" && version != "2") return;
    const bool warmMasks = version == "2";

    // Trial for the unit-scale WGL retained surface. Device paint uses separate
    // luminance and subpixel phases from the canonical shaping strike.
    constexpr std::array<SkColor, 4> colors{0xFF171C26, 0xFF637083, 0xFFE8EAEE, 0xFF9AA2AF};
    SkPaint paint;
    paint.setAntiAlias(true);
    for (SkColor color : colors) {
        paint.setColor(color);
        auto strike = SkStrikeSpec::MakeMask(font, paint, SkSurfaceProps(),
            SkScalerContextFlags::kFakeGammaAndBoostContrast, SkMatrix::I()).findOrCreateStrike();
        const auto mask = strike->roundingSpec().ignorePositionFieldMask;
        const int phases = mask.x() ? 4 : 1;
        for (const auto glyph : glyphs) {
            sktext::StrikeMutationMonitor mutation(strike.get());
            for (int phase = 0; phase < phases; ++phase) {
                const SkPackedGlyphID packed(glyph, SkPoint::Make(phase * 0.25f, 0.0f), mask);
                const auto digest = strike->digestFor(skglyph::kDirectMask, packed);
                if (warmMasks && digest.actionFor(skglyph::kDirectMask) == skglyph::GlyphAction::kAccept) {
                    strike->prepareForImage(strike->glyph(digest));
                }
            }
        }
    }
}
} // namespace
#endif

bool warmRegisteredGlyphMetrics(std::string_view text, std::string_view family, float size, int weight) {
    auto manager = rendering::makeEmbeddedFontManager();
    const std::string name(family);
    const auto style = SkFontStyle(weight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant);
    auto face = manager->matchFamilyStyle(name.c_str(), style);
    if (!face) return false;

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
    if (count <= 0) return false;
    std::vector<SkGlyphID> glyphs(count);
    font.textToGlyphs(text.data(), text.size(), SkTextEncoding::kUTF8, glyphs);
    // Skip the missing glyph: this hint warms the registered face and never
    // resolves fallback. SkParagraph still shapes the real text at paint time.
    glyphs.erase(std::remove(glyphs.begin(), glyphs.end(), 0), glyphs.end());
    if (glyphs.empty()) return false;
    std::vector<SkScalar> widths(glyphs.size());
    font.getWidths(glyphs, widths);
#if defined(_WIN32)
    warmDrawMetrics(font, glyphs);
#endif
    return true;
}
} // namespace oneui::text
