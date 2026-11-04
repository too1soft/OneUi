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
#include <cstring>
#if defined(_WIN32)
#include "include/ports/SkTypeface_win.h"
#elif defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(__ANDROID__)
#include "include/ports/SkFontMgr_android.h"
#include "include/ports/SkFontScanner_FreeType.h"
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

sk_sp<paragraph::TypefaceFontProvider> embeddedProvider() {
    static auto provider = sk_make_sp<paragraph::TypefaceFontProvider>();
    return provider;
}

std::mutex& embeddedFontMutex() {
    static std::mutex mutex;
    return mutex;
}

struct RegisteredFont {
    std::string family;
    sk_sp<SkData> data;
};

std::vector<RegisteredFont>& registeredFonts() {
    static std::vector<RegisteredFont> fonts;
    return fonts;
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
    if (std::any_of(registeredFonts().begin(), registeredFonts().end(), [&](const auto& font) {
            return font.family == familyAlias && font.data->size() == size &&
                std::memcmp(font.data->data(), data, size) == 0;
        })) {
        return true;
    }

    auto fontData = SkData::MakeWithCopy(data, size);
    auto baseTypeface = makeTypefaceFromData(fontData);
    if (!baseTypeface) {
        return false;
    }

    const SkString alias(familyAlias.c_str());
    constexpr std::array<int, 9> weights{100, 200, 300, 400, 500, 600, 700, 800, 900};
    std::size_t registered = 0;
    if (baseTypeface->getVariationDesignParameters({}) > 0) {
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
    }
    if (registered == 0) {
        embeddedProvider()->registerTypeface(std::move(baseTypeface), alias);
    }
    registeredFonts().push_back({familyAlias, std::move(fontData)});
    return true;
}
} // namespace oneui::rendering

namespace oneui {
namespace {
std::vector<FontFamilyInfo> fontCatalog(bool includeRegistered) {
    auto manager = rendering::makePlatformFontManager();
    if (!manager && !includeRegistered) throw std::runtime_error("System font manager unavailable");
    std::vector<FontFamilyInfo> result;
    const auto append = [&](const sk_sp<SkFontMgr>& fonts) {
        if (!fonts) return;
        const int count = fonts->countFamilies();
        for (int i = 0; i < count; ++i) {
            SkString name;
            fonts->getFamilyName(i, &name);
            if (name.isEmpty()) continue;
            auto styles = fonts->createStyleSet(i);
            auto face = styles ? styles->matchStyle(SkFontStyle::Normal()) : nullptr;
            result.push_back({std::string(name.c_str(), name.size()), face && face->isFixedPitch()});
        }
    };
    if (includeRegistered) {
        std::lock_guard<std::mutex> lock(rendering::embeddedFontMutex());
        append(rendering::makeEmbeddedFontManager());
    }
    append(manager);
    if (!manager && result.empty()) throw std::runtime_error("System font manager unavailable");
    // Match painting: application fonts take precedence over installed names.
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.family < b.family;
    });
    result.erase(std::unique(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.family == b.family;
    }), result.end());
    return result;
}
} // namespace

std::vector<FontFamilyInfo> enumerateSystemFonts() { return fontCatalog(false); }
std::vector<FontFamilyInfo> enumerateAvailableFonts() { return fontCatalog(true); }
} // namespace oneui
