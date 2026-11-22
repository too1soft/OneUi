#include "platform/shared/skia_canvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"
#include "oneui/fonts.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>

namespace oneui::rendering {
namespace {
constexpr std::array<int, 9> weights{100, 200, 300, 400, 500, 600, 700, 800, 900};

class EmbeddedStyleSet final : public SkFontStyleSet {
    struct Entry {
        SkFontStyle style;
        sk_sp<SkTypeface> base, instance;
        int weight = 0;
    };
    SkString alias_;
    std::mutex mutex_;
    std::vector<Entry> entries_;
public:
    explicit EmbeddedStyleSet(SkString alias) : alias_(std::move(alias)) {}
    void append(sk_sp<SkTypeface> face, int lazyWeight = 0) {
        const auto baseStyle = face->fontStyle();
        const auto style = lazyWeight ? SkFontStyle(lazyWeight, baseStyle.width(), baseStyle.slant()) : baseStyle;
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back({style, face, lazyWeight ? nullptr : face, lazyWeight});
    }
    int count() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(entries_.size());
    }
    void getStyle(int index, SkFontStyle* style, SkString* name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index < 0 || static_cast<size_t>(index) >= entries_.size()) return;
        if (style) *style = entries_[index].style;
        if (name) *name = alias_;
    }
    sk_sp<SkTypeface> createTypeface(int index) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index < 0 || static_cast<size_t>(index) >= entries_.size()) return nullptr;
        auto& entry = entries_[index];
        if (!entry.instance) {
            const SkFontArguments::VariationPosition::Coordinate coordinate{
                SkFontArguments::VariationPosition::Coordinate::wght, static_cast<float>(entry.weight)};
            SkFontArguments arguments;
            arguments.setVariationDesignPosition({&coordinate, 1});
            entry.instance = entry.base->makeClone(arguments);
            if (!entry.instance) entry.instance = entry.base;
        }
        return entry.instance;
    }
    sk_sp<SkTypeface> matchStyle(const SkFontStyle& style) override {
        return matchStyleCSS3(style);
    }
};

class EmbeddedProvider final : public skia::textlayout::TypefaceFontProvider {
    std::map<std::string, sk_sp<EmbeddedStyleSet>> styles_;
public:
    void append(sk_sp<SkTypeface> face, const SkString& alias, int lazyWeight = 0) {
        auto& set = styles_[alias.c_str()];
        if (!set) {
            // The base provider retains family enumeration and lookup order.
            registerTypeface(face, alias);
            set = sk_make_sp<EmbeddedStyleSet>(alias);
        }
        set->append(std::move(face), lazyWeight);
    }
    sk_sp<SkFontStyleSet> onMatchFamily(const char name[]) const override {
        if (!name) return nullptr;
        const auto found = styles_.find(name);
        return found == styles_.end() ? nullptr : found->second;
    }
    sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override {
        if (index < 0 || index >= onCountFamilies()) return nullptr;
        SkString name;
        onGetFamilyName(index, &name);
        return onMatchFamily(name.c_str());
    }
};

sk_sp<EmbeddedProvider> embeddedProvider() {
    static auto provider = sk_make_sp<EmbeddedProvider>();
    return provider;
}
std::mutex& embeddedFontMutex() {
    static std::mutex mutex;
    return mutex;
}
struct RegisteredFont { std::string family; sk_sp<SkData> data; };
std::vector<RegisteredFont>& registeredFonts() {
    static std::vector<RegisteredFont> fonts;
    return fonts;
}
bool canCreateWeightsLazily(SkTypeface& face) {
#if ONEUI_LAZY_EMBEDDED_FONTS && defined(_WIN32)
    // Other axes/ranges retain the established eager path. The nine styles
    // must have exactly the same metadata as their native weight instances.
    SkFontParameters::Variation::Axis axis;
    return face.getVariationDesignParameters({&axis, 1}) == 1 &&
           axis.tag == SkFontArguments::VariationPosition::Coordinate::wght &&
           axis.min == 100 && axis.max == 900;
#else
    return false;
#endif
}
} // namespace

sk_sp<SkFontMgr> makeEmbeddedFontManager() { return embeddedProvider(); }

bool registerFontFromMemory(const void* data, std::size_t size, const std::string& familyAlias) {
    if (!data || !size || familyAlias.empty()) return false;
    std::lock_guard<std::mutex> lock(embeddedFontMutex());
    for (const auto& font : registeredFonts())
        if (font.family == familyAlias && font.data->size() == size &&
            std::memcmp(font.data->data(), data, size) == 0) return true;
    auto fontData = SkData::MakeWithCopy(data, size);
    auto base = makeTypefaceFromData(fontData);
    if (!base) return false;
    const SkString alias(familyAlias.c_str());
    size_t registered = 0;
    if (canCreateWeightsLazily(*base)) {
        for (const int weight : weights) embeddedProvider()->append(base, alias, weight);
        registered = weights.size();
    } else if (base->getVariationDesignParameters({}) > 0) {
        for (const int weight : weights) {
            const SkFontArguments::VariationPosition::Coordinate coordinate{
                SkFontArguments::VariationPosition::Coordinate::wght, static_cast<float>(weight)};
            SkFontArguments arguments;
            arguments.setVariationDesignPosition({&coordinate, 1});
            auto instance = base->makeClone(arguments);
            if (instance) {
                embeddedProvider()->append(std::move(instance), alias);
                ++registered;
            }
        }
    }
    if (!registered) embeddedProvider()->append(std::move(base), alias);
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
