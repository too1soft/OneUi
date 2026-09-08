#include "text/text_layout.h"
#include "internal/unicode.h"
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
        Layout::installTestFonts({ONEUI_TEXT_ASSETS "/NotoSans-Regular.ttf", ONEUI_TEXT_ASSETS "/NotoNaskhArabic-Regular.ttf",
            ONEUI_TEXT_ASSETS "/NotoSansDevanagari-Regular.ttf", ONEUI_TEXT_ASSETS "/NotoSansHebrew-Regular.ttf",
            ONEUI_TEXT_ASSETS "/NotoSansCJKsc-Regular.otf", ONEUI_TEXT_ASSETS "/NotoColorEmoji.ttf"});
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
