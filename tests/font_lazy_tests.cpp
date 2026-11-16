#include "text/embedded_font_provider.h"
#include "platform/shared/skia_canvas.h"
#include "include/core/SkData.h"
#include <stdexcept>
#include <thread>
#include <iostream>

int main(int argc, char** argv) {
    try {
        auto missing = sk_make_sp<oneui::rendering::EmbeddedFontProvider>();
        if (missing->matchFamilyStyle("not-a-registered-family", SkFontStyle::Normal()) ||
            missing->matchFamilyStyle(nullptr, SkFontStyle::Normal()) || missing->createStyleSet(-1)->count())
            throw std::runtime_error("missing aliases must allow platform fallback without dereferencing null");
        auto family = sk_make_sp<oneui::rendering::DeferredFontFamily>();
        family->add(SkTypeface::MakeEmpty(), true);
        if (family->count() != 9 || family->cachedTypefaceCount())
            throw std::runtime_error("enumeration must not instantiate variable weights");
        SkFontStyle style; family->getStyle(5, &style, nullptr);
        if (style.weight() != 600 || family->cachedTypefaceCount())
            throw std::runtime_error("style metadata must remain lazy");
        auto regular = family->matchStyle(SkFontStyle::Normal());
        if (!regular || family->cachedTypefaceCount() != 1)
            throw std::runtime_error("first request must create exactly one weight");
        std::thread worker([&] {
            for (int i = 0; i != 100; ++i) family->matchStyle(SkFontStyle::Bold());
        });
        for (int i = 0; i != 100; ++i) family->matchStyle(SkFontStyle::Normal());
        worker.join();
        if (family->cachedTypefaceCount() != 2)
            throw std::runtime_error("repeated concurrent requests must reuse weights");
        for (int i = 0; i != 9; ++i) if (!family->createTypeface(i))
            throw std::runtime_error("every advertised weight must remain available");
        if (family->cachedTypefaceCount() != 9)
            throw std::runtime_error("all requested weights must be cached");
        if (argc == 2) {
            auto data = SkData::MakeFromFileName(argv[1]);
            auto face = oneui::rendering::makeTypefaceFromData(data);
            if (!face || face->getVariationDesignParameters({}) <= 0)
                throw std::runtime_error("actual variable font fixture required");
            std::vector<SkFontParameters::Variation::Axis> parameters(face->getVariationDesignParameters({}));
            face->getVariationDesignParameters(parameters);
            auto provider = sk_make_sp<oneui::rendering::EmbeddedFontProvider>();
            provider->add(face, SkString("lazy-variable-fixture"));
            if (provider->countFamilies() != 1 || provider->matchFamily("lazy-variable-fixture")->count() != 9)
                throw std::runtime_error("alias enumeration must retain every variable weight");
            if (!provider->legacyMakeTypeface("not-registered", SkFontStyle::Normal()))
                throw std::runtime_error("legacy lookup must preserve the base provider's default family fallback");
            for (int weight = 100; weight <= 900; weight += 100) {
                auto instance = provider->matchFamilyStyle("lazy-variable-fixture", SkFontStyle(weight, 5, SkFontStyle::kUpright_Slant));
                if (!instance || !instance->unicharToGlyph('A'))
                    throw std::runtime_error("requested variable face lost glyph coverage");
                std::vector<SkFontArguments::VariationPosition::Coordinate> axes(instance->getVariationDesignPosition({}));
                instance->getVariationDesignPosition(axes);
                bool matched = false;
                for (const auto& axis : axes)
                    if (axis.axis == SkFontArguments::VariationPosition::Coordinate::wght)
                        for (const auto& parameter : parameters)
                            if (parameter.tag == axis.axis && axis.value == std::clamp(float(weight), parameter.min, parameter.max)) matched = true;
                if (!matched) throw std::runtime_error("requested wght axis was not materialized");
            }
        }
        std::cout << "Deferred font weight lifetime passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
