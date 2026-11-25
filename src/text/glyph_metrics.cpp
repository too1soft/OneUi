#include "glyph_metrics.h"

#include "platform/shared/skia_canvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace oneui::text {
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
    return true;
}
} // namespace oneui::text
