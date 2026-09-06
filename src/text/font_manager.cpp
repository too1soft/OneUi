#include "platform/shared/skia_canvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"

#include <array>
#include <mutex>
#include <string>
#include <unordered_set>
#if defined(_WIN32)
#include "include/ports/SkTypeface_win.h"
#elif defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#else
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
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
#else
    return SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
#endif
}

sk_sp<SkFontMgr> makeEmbeddedFontManager() {
    return embeddedProvider();
}

bool registerFontFromMemory(const void* data, std::size_t size, const std::string& familyAlias) {
    if (!data || size == 0 || familyAlias.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(embeddedFontMutex());
    if (embeddedFontFamilies().find(familyAlias) != embeddedFontFamilies().end()) {
        return true;
    }

    auto loader = makePlatformFontManager();
    if (!loader) {
        return false;
    }
    auto fontData = SkData::MakeWithCopy(data, size);
    auto baseTypeface = loader->makeFromData(std::move(fontData));
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
