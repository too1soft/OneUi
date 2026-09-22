#include "text/text_layout.h"
#include "internal/unicode.h"
#include "test_asset_path.h"
#include "platform/shared/skia_canvas.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkData.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using oneui::text::Layout;
using oneui::text::LayoutOptions;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
#if defined(_WIN32)
        // Exercise the real system fallback BEFORE installing test fonts.
        // A Chinese Windows 7 installation contains Microsoft YaHei/SimSun;
        // Segoe UI alone must not turn these characters into empty boxes.
        auto systemChinese = Layout::make(L"\u4e2d\u6587", LayoutOptions{});
        if (systemChinese->unresolvedGlyphs() != 0) {
            auto manager = oneui::rendering::makePlatformFontManager();
            std::cerr << "System font families: " << manager->countFamilies() << '\n';
            for (const char* locale : {"", "und", "zh-CN"}) {
                auto face = manager->matchFamilyStyleCharacter("Segoe UI", SkFontStyle{}, &locale, *locale ? 1 : 0, 0x4E2D);
                std::cerr << "Fallback locale '" << locale << "': " << (face ? face->unicharToGlyph(0x4E2D) : 0) << '\n';
            }
        }
        require(systemChinese->unresolvedGlyphs() == 0, "system font fallback must resolve Chinese glyphs");
#endif
        auto emojiData = SkData::MakeFromFileName(oneuiTestAsset("NotoColorEmoji.ttf").c_str());
        require(emojiData && oneui::rendering::registerFontFromMemory(emojiData->data(), emojiData->size(), "OneUI Acceptance Emoji"),
                "application color font registration must work without OS font installation");
        LayoutOptions emojiOptions;
        emojiOptions.family = L"OneUI Acceptance Emoji";
        require(Layout::make(oneui::unicode::fromUtf8(u8"😀"), emojiOptions)->unresolvedGlyphs() == 0,
                "registered application emoji font must resolve glyphs");
        Layout::installTestFonts({oneuiTestAsset("NotoSans-Regular.ttf"), oneuiTestAsset("NotoNaskhArabic-Regular.ttf"),
            oneuiTestAsset("NotoSansDevanagari-Regular.ttf"), oneuiTestAsset("NotoSansHebrew-Regular.ttf"),
            oneuiTestAsset("NotoSansCJKsc-Regular.otf"), oneuiTestAsset("NotoColorEmoji.ttf")});
        const auto family = oneui::unicode::fromUtf8("a\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6" "b");
        auto clusters = Layout::make(family);
        require(clusters->graphemes().size() == 4, "family emoji must form one grapheme");
        require(clusters->previous(family.size() - 1) == 1, "backspace must not split ZWJ sequence");
        require(clusters->next(1) == family.size() - 1, "forward navigation must not split ZWJ sequence");
        auto combining = Layout::make(L"e\u0301x");
        require(combining->next(0) == 2 && combining->floor(1) == 0, "combining mark boundary");
        require(combining->utf8Offset(2) == 3 && combining->wideOffset(3) == 2, "UTF-8 mapping");
        auto crlf = Layout::make(L"a\r\nb\n");
        require(crlf->next(1) == 3, "CRLF must form one grapheme");
        require(crlf->lines().size() == 3 && crlf->height() > 0, "hard paragraphs and trailing empty line");

        auto latin = Layout::make(L"office");
        require(latin->width() > 0 && latin->height() > 0, "layout must work before paint");
        for (const auto offset : latin->graphemes()) {
            const auto caret = latin->caret({offset});
            const auto hit = latin->hitTest({caret.x, caret.y + caret.height / 2});
            require(hit.offset == offset, "Latin caret/hit-test roundtrip");
        }
        auto rtl = Layout::make(L"\u05d0\u05d1\u05d2");
        require(rtl->caret({0}).x > rtl->caret({3}).x, "auto RTL base direction");
        require(rtl->moveVisual({0}, -1).offset == 1, "visual RTL left navigation");
        LayoutOptions limited;
        limited.maxLines = 3;
        auto paragraphs = Layout::make(L"abc\n\u05d0\u05d1\u05d2\nlast\nhidden", limited);
        require(paragraphs->lines().size() == 3, "hard-paragraph line limit");
        require(paragraphs->caret({4}).x > paragraphs->caret({7}).x, "line limits preserve per-paragraph auto direction");
        auto mixed = Layout::make(L"abc \u05d0\u05d1\u05d2 123");
        require(!mixed->selection(2, 7).empty(), "bidi selection geometry");
        for (const auto* sample : {u8"中文日本語한글", u8"नमस्ते हिन्दी", u8"مرحبا 123", u8"עברית", u8"👨‍👩‍👧‍👦🇨🇳", u8"office é"}) {
            auto script = Layout::make(oneui::unicode::fromUtf8(sample));
            require(script->unresolvedGlyphs() == 0, "required scripts must have complete pinned-font coverage");
            require(script->width() > 0, "script shapes to visible glyphs");
        }
        LayoutOptions wrapped;
        wrapped.text.wrap = oneui::TextWrapMode::WordWrap;
        wrapped.width = 35;
        auto lines = Layout::make(L"one two three four", wrapped);
        require(lines->lines().size() > 1, "word wrap must produce visual lines");
        auto down = lines->moveVertical({0}, 1, 0);
        require(down.offset > 0, "vertical navigation uses visual lines");
        wrapped.maxLines = 2; wrapped.ellipsis = true;
        auto ellipsized = Layout::make(L"abc\n\u05d0\u05d1\u05d2\nhidden", wrapped);
        require(ellipsized->lines().size() == 2 && ellipsized->width() <= wrapped.width + 1, "narrow paragraph ellipsis obeys max lines");

        LayoutOptions richOptions;
        richOptions.width = 180;
        richOptions.text.wrap = oneui::TextWrapMode::WordWrap;
        oneui::TextStyleSpan accent;
        accent.start = 5;
        accent.end = 9;
        accent.fontWeight = 700;
        accent.italic = true;
        accent.foreground = oneui::Color{0, 220, 96, 255};
        accent.background = oneui::Color{20, 80, 220, 180};
        richOptions.spans = {accent};
        auto rich = Layout::make(L"base rich next", richOptions);
        require(rich->lines().size() == 1 && rich->width() > 0, "rich spans shape in one paragraph");
        auto richSurface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(220, 48));
        rich->paint(*richSurface->getCanvas(), {0, 0}, {230, 32, 32, 255});
        SkPixmap richPixels;
        require(richSurface->peekPixels(&richPixels), "rich text pixels available");
        int richRed = 0, richGreen = 0, richBlue = 0;
        for (int y = 0; y < richPixels.height(); ++y) for (int x = 0; x < richPixels.width(); ++x) {
            const auto c = richPixels.getColor(x, y);
            if (SkColorGetA(c) < 16) continue;
            if (SkColorGetR(c) > SkColorGetG(c) * 2) ++richRed;
            if (SkColorGetG(c) > SkColorGetR(c) && SkColorGetG(c) > SkColorGetB(c)) ++richGreen;
            if (SkColorGetB(c) > SkColorGetR(c) && SkColorGetB(c) > SkColorGetG(c)) ++richBlue;
        }
        require(richRed > 0 && richGreen > 0 && richBlue > 0,
                "rich paint keeps base color, inline foreground, and inline background");
        LayoutOptions richMultiline = richOptions;
        richMultiline.spans.front().start = 0;
        richMultiline.spans.front().end = 4;
        auto richLines = Layout::make(L"bold\nnext", richMultiline);
        require(richLines->lines().size() == 2, "rich spans preserve hard line breaks");

        Layout::clearCache();
        auto first = Layout::make(L"unchanged\nfirst");
        const auto before = Layout::stats();
        auto second = Layout::make(L"unchanged\nsecond");
        require(Layout::stats().layouts == before.layouts + 1, "unchanged paragraph must reuse shaping");
        auto cached = Layout::make(L"unchanged\nsecond");
        require(Layout::stats().layouts == before.layouts + 1, "identical document reuses every shaped paragraph");
        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 80));
        const auto shapes = Layout::stats().layouts;
        second->paint(*surface->getCanvas(), {0, 0}, {255, 0, 0, 255});
        surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        second->paint(*surface->getCanvas(), {0, 0}, {0, 255, 0, 255});
        SkPixmap pixels;
        require(surface->peekPixels(&pixels), "text color pixels available");
        int greenPixels = 0, redPixels = 0;
        for (int y = 0; y < pixels.height(); ++y) for (int x = 0; x < pixels.width(); ++x) {
            const auto c = pixels.getColor(x, y);
            if (SkColorGetA(c) < 16) continue;
            if (SkColorGetG(c) > SkColorGetR(c)) ++greenPixels;
            if (SkColorGetR(c) > SkColorGetG(c)) ++redPixels;
        }
        require(greenPixels > 0 && redPixels == 0, "cached text must use this draw's color, not its first paint color");
        require(Layout::stats().layouts == shapes, "color changes must not re-shape");
        LayoutOptions sensitive; sensitive.sensitive = true;
        const auto cacheBefore = Layout::stats().cachedTextBytes;
        auto secret = Layout::make(L"test-only secret", sensitive);
        require(Layout::stats().cachedTextBytes == cacheBefore, "sensitive layouts never enter shared cache");
        require(secret->height() > 0, "private layout remains usable");
        std::cout << "Text layout tests passed (Unicode 15.1, real SkParagraph)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
