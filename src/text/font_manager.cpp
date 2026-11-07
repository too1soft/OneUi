#include "platform/shared/skia_canvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"

#include "oneui/fonts.h"
#include <algorithm>
#include <stdexcept>
#include <array>
#include <mutex>
#include <string>
#include <unordered_set>
#if defined(_WIN32)
#include "include/ports/SkTypeface_win.h"
#elif defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(__ANDROID__)
#include "include/ports/SkFontMgr_android.h"
#include "include/ports/SkFontScanner_FreeType.h"
#elif defined(ONEUI_FONTMGR_DIRECTORY)
#include "include/ports/SkFontMgr_directory.h"
#include "include/core/SkStream.h"
#include <cstring>
#else
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
#endif
#if defined(ONEUI_PORTABLE_FONT_LOADER)
#include "include/ports/SkFontMgr_empty.h"
#endif

namespace oneui::rendering {
namespace {
namespace paragraph = skia::textlayout;

#if defined(ONEUI_FONTMGR_DIRECTORY)
class HarmonyFontManager final : public SkFontMgr {
public:
    HarmonyFontManager()
        : directory_(SkFontMgr_New_Custom_Directory("/system/fonts")),
          preferred_{
              directory_->makeFromFile("/system/fonts/HarmonyOS_Sans.ttf"),
              directory_->makeFromFile("/system/fonts/HarmonyOS_Sans_SC.ttf"),
              directory_->makeFromFile("/system/fonts/HarmonyOS_Sans_TC.ttf"),
              directory_->makeFromFile("/system/fonts/HMOSColorEmojiCompat.ttf")} {
        if (preferred_.front()) preferred_.front()->getFamilyName(&defaultFamily_);
    }

protected:
    int onCountFamilies() const override { return directory_->countFamilies(); }
    void onGetFamilyName(int index, SkString* family) const override { directory_->getFamilyName(index, family); }
    sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override { return directory_->createStyleSet(index); }
    sk_sp<SkFontStyleSet> onMatchFamily(const char* family) const override {
        return directory_->matchFamily(resolve(family));
    }
    sk_sp<SkTypeface> onMatchFamilyStyle(const char* family, const SkFontStyle& style) const override {
        return directory_->matchFamilyStyle(resolve(family), style);
    }
    sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char* family, const SkFontStyle& style,
                                                  const char*[], int, SkUnichar character) const override {
        auto face = onMatchFamilyStyle(family, style);
        if (face && face->unicharToGlyph(character)) return face;
        for (const auto& fallback : preferred_) {
            if (!fallback || !fallback->unicharToGlyph(character)) continue;
            SkString name;
            fallback->getFamilyName(&name);
            auto styled = directory_->matchFamilyStyle(name.c_str(), style);
            return styled && styled->unicharToGlyph(character) ? std::move(styled) : fallback;
        }
        for (int i = 0; i < directory_->countFamilies(); ++i) {
            face = directory_->createStyleSet(i)->matchStyle(style);
            if (face && face->unicharToGlyph(character)) return face;
        }
        return nullptr;
    }
    sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int index) const override {
        return directory_->makeFromData(std::move(data), index);
    }
    sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream, int index) const override {
        return directory_->makeFromStream(std::move(stream), index);
    }
    sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream, const SkFontArguments& args) const override {
        return directory_->makeFromStream(std::move(stream), args);
    }
    sk_sp<SkTypeface> onMakeFromFile(const char* path, int index) const override {
        return directory_->makeFromFile(path, index);
    }
    sk_sp<SkTypeface> onLegacyMakeTypeface(const char* family, SkFontStyle style) const override {
        return directory_->legacyMakeTypeface(resolve(family), style);
    }

private:
    const char* resolve(const char* family) const {
        if (!defaultFamily_.isEmpty() && (!family || std::strcmp(family, "sans-serif") == 0))
            return defaultFamily_.c_str();
        return family;
    }

    sk_sp<SkFontMgr> directory_;
    std::array<sk_sp<SkTypeface>, 4> preferred_;
    SkString defaultFamily_;
};
#endif

sk_sp<paragraph::TypefaceFontProvider> embeddedProvider() {
    static auto provider = sk_make_sp<paragraph::TypefaceFontProvider>();
    return provider;
}

std::mutex& embeddedFontMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_set<std::string>& embeddedFontFamilies() {
    static std::unordered_set<std::string> families;
    return families;
}
} // namespace

sk_sp<SkFontMgr> makePlatformFontManager() {
#if defined(_WIN32)
    auto manager = SkFontMgr_New_DirectWrite();
    return manager ? manager : SkFontMgr_New_GDI();
#elif defined(__APPLE__)
    return SkFontMgr_New_CoreText(nullptr);
#elif defined(__ANDROID__)
    return SkFontMgr_New_Android(nullptr, SkFontScanner_Make_FreeType());
#elif defined(ONEUI_FONTMGR_DIRECTORY)
    static auto manager = sk_make_sp<HarmonyFontManager>();
    return manager;
#else
    return SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
#endif
}

sk_sp<SkFontMgr> makeEmbeddedFontManager() {
    return embeddedProvider();
}

sk_sp<SkTypeface> makeTypefaceFromData(sk_sp<SkData> data) {
    if (!data) return nullptr;
    auto platform = makePlatformFontManager();
    auto face = platform ? platform->makeFromData(data) : nullptr;
#if defined(ONEUI_PORTABLE_FONT_LOADER)
    // Some native backends cannot open application fonts; Win7 DirectWrite
    // can also open a variable font while ignoring all of its axes, leaving
    // every requested weight at the default (often 100). Prefer the existing
    // FreeType loader only for missing faces or missing variation support.
    if (!face || face->getVariationDesignParameters({}) <= 0) {
        auto portable = SkFontMgr_New_Custom_Empty();
        auto portableFace = portable ? portable->makeFromData(std::move(data)) : nullptr;
        if (portableFace && (!face || portableFace->getVariationDesignParameters({}) > 0)) {
            face = std::move(portableFace);
        }
    }
#endif
    return face;
}

bool registerFontFromMemory(const void* data, std::size_t size, const std::string& familyAlias) {
    if (!data || size == 0 || familyAlias.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(embeddedFontMutex());
    if (embeddedFontFamilies().find(familyAlias) != embeddedFontFamilies().end()) {
        return true;
    }

    auto fontData = SkData::MakeWithCopy(data, size);
    auto baseTypeface = makeTypefaceFromData(std::move(fontData));
    if (!baseTypeface) {
        return false;
    }

    const SkString alias(familyAlias.c_str());
    constexpr std::array<int, 9> weights{100, 200, 300, 400, 500, 600, 700, 800, 900};
    std::size_t registered = 0;
    for (const int weight : weights) {
        const SkFontArguments::VariationPosition::Coordinate coordinate{
            SkFontArguments::VariationPosition::Coordinate::wght,
            static_cast<float>(weight)};
        SkFontArguments arguments;
        arguments.setVariationDesignPosition({&coordinate, 1});
        auto instance = baseTypeface->makeClone(arguments);
        if (instance) {
            embeddedProvider()->registerTypeface(std::move(instance), alias);
            ++registered;
        }
    }
    if (registered == 0) {
        embeddedProvider()->registerTypeface(std::move(baseTypeface), alias);
    }
    embeddedFontFamilies().insert(familyAlias);
    return true;
}
} // namespace oneui::rendering

namespace oneui {
std::vector<FontFamilyInfo> enumerateSystemFonts() {
    auto manager = rendering::makePlatformFontManager();
    if (!manager) throw std::runtime_error("System font manager unavailable");
    std::vector<FontFamilyInfo> result;
    const int count = manager->countFamilies();
    result.reserve(static_cast<std::size_t>(std::max(count, 0)));
    for (int i = 0; i < count; ++i) {
        SkString name;
        manager->getFamilyName(i, &name);
        if (name.isEmpty()) continue;
        auto styles = manager->createStyleSet(i);
        auto face = styles ? styles->matchStyle(SkFontStyle::Normal()) : nullptr;
        result.push_back({std::string(name.c_str(), name.size()), face && face->isFixedPitch()});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.family < b.family;
    });
    result.erase(std::unique(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.family == b.family;
    }), result.end());
    return result;
}
} // namespace oneui
