#include "platform/shared/skia_canvas.h"
#include "test_asset_path.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
SkBitmap draw(const sk_sp<SkTypeface>& face, float size) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(900, 100);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    SkFont font(face, size);
    font.setEdging(SkFont::Edging::kAntiAlias);
    font.setSubpixel(true);
    SkPaint paint;
    paint.setColor(SK_ColorBLACK);
    const char text[] = u8"安装程序 OneUpdate Aa 0123 中文简繁體 € Ω";
    canvas.drawSimpleText(text, sizeof(text) - 1, SkTextEncoding::kUTF8, 2.25f, 60, font, paint);
    return bitmap;
}
void exercise(const char* path, const std::string& alias) {
    auto data = SkData::MakeFromFileName(path);
    check(data != nullptr, "Font fixture unavailable");
    auto base = oneui::rendering::makeTypefaceFromData(data);
    check(base != nullptr, "Native font loader failed");
    check(oneui::rendering::registerFontFromMemory(data->data(), data->size(), alias), "Registration failed");
    auto manager = oneui::rendering::makeEmbeddedFontManager();
    auto styles = manager->matchFamily(alias.c_str());
    check(styles != nullptr, "Registered family missing");
    const bool variable = base->getVariationDesignParameters({}) > 0;
    check(styles->count() == (variable ? 9 : 1), "Style count changed");
    const auto count = styles->count();
    check(oneui::rendering::registerFontFromMemory(data->data(), data->size(), alias), "Repeated registration failed");
    check(styles->count() == count, "Idempotent registration changed the catalog");
    for (int i = 0; i < count; ++i) {
        auto expected = base;
        if (variable) {
            const SkFontArguments::VariationPosition::Coordinate coordinate{
                SkFontArguments::VariationPosition::Coordinate::wght, float((i + 1) * 100)};
            SkFontArguments arguments;
            arguments.setVariationDesignPosition({&coordinate, 1});
            expected = base->makeClone(arguments);
        }
        check(expected != nullptr, "Native reference clone failed");
        SkFontStyle metadata;
        styles->getStyle(i, &metadata, nullptr);
        check(metadata == expected->fontStyle(), "Style metadata differs from the native instance");
        auto actual = styles->createTypeface(i);
        check(actual != nullptr && actual->fontStyle() == expected->fontStyle(), "Resolved weight changed");
        check(styles->createTypeface(i).get() == actual.get(), "Resolved typeface was not cached");
        auto matched = manager->matchFamilyStyle(alias.c_str(), metadata);
        check(matched && matched->fontStyle() == expected->fontStyle(), "CSS weight matching changed");
        check(actual->countGlyphs() == expected->countGlyphs(), "Glyph coverage changed");
        for (float size : {12.0f, 18.0f, 32.0f}) {
            const auto reference = draw(expected, size), result = draw(actual, size);
            check(reference.computeByteSize() == result.computeByteSize() &&
                      std::memcmp(reference.getPixels(), result.getPixels(), result.computeByteSize()) == 0,
                  "Glyph pixels differ from the native weight instance");
        }
    }
    check(!manager->matchFamilyStyle("__missing_OneUI_family__", SkFontStyle()), "Missing family must not match");
    std::cout << alias << ": " << count << " styles; coverage, matching and pixels exact\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto fixture = oneuiTestAsset("NotoSans-Regular.ttf");
        exercise(fixture.c_str(), "ProfileStatic");
        if (argc > 1) exercise(argv[1], "ProfileVariable");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
