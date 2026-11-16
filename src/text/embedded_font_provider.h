#pragma once
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"
#include <array>
#include <algorithm>
#include <map>
#include <mutex>

namespace oneui::rendering {
// Advertise the same nine variable weights, but materialize only requested ones.
class DeferredFontFamily final : public SkFontStyleSet {
    struct Source {
        sk_sp<SkTypeface> base;
        bool variable;
        std::array<sk_sp<SkTypeface>, 9> weights{};
        int minimum = 100, maximum = 900;
    };
    mutable std::recursive_mutex mutex_;
    std::vector<Source> sources_;
    std::pair<Source*, int> at(int index) {
        if (index < 0) return {nullptr, 0};
        for (auto& source : sources_) {
            const int count = source.variable ? 9 : 1;
            if (index < count) return {&source, index};
            index -= count;
        }
        return {nullptr, 0};
    }
public:
    void add(sk_sp<SkTypeface> face, bool variable) {
        std::lock_guard lock(mutex_);
        Source source{std::move(face), variable};
        if (variable) {
            std::vector<SkFontParameters::Variation::Axis> axes(source.base->getVariationDesignParameters({}));
            source.base->getVariationDesignParameters(axes);
            for (const auto& axis : axes)
                if (axis.tag == SkFontArguments::VariationPosition::Coordinate::wght) {
                    source.minimum = int(axis.min); source.maximum = int(axis.max);
                }
        }
        sources_.push_back(std::move(source));
    }
    int count() override {
        std::lock_guard lock(mutex_);
        int result = 0;
        for (const auto& source : sources_) result += source.variable ? 9 : 1;
        return result;
    }
    void getStyle(int index, SkFontStyle* style, SkString* name) override {
        std::lock_guard lock(mutex_);
        auto [source, position] = at(index);
        if (!source) return;
        const auto base = source->base->fontStyle();
        if (style) *style = source->variable
            ? SkFontStyle(std::clamp((position + 1) * 100, source->minimum, source->maximum), base.width(), base.slant()) : base;
        if (name) name->reset();
    }
    sk_sp<SkTypeface> createTypeface(int index) override {
        std::lock_guard lock(mutex_);
        auto [source, position] = at(index);
        if (!source) return nullptr;
        if (!source->variable) return source->base;
        auto& result = source->weights[position];
        if (!result) {
            const SkFontArguments::VariationPosition::Coordinate coordinate{
                SkFontArguments::VariationPosition::Coordinate::wght, float((position + 1) * 100)};
            SkFontArguments arguments;
            arguments.setVariationDesignPosition({&coordinate, 1});
            result = source->base->makeClone(arguments);
            if (!result) result = source->base;
        }
        return result;
    }
    sk_sp<SkTypeface> matchStyle(const SkFontStyle& style) override {
        std::lock_guard lock(mutex_);
        return matchStyleCSS3(style);
    }
    size_t cachedTypefaceCount() const {
        std::lock_guard lock(mutex_);
        size_t result = 0;
        for (const auto& source : sources_)
            for (const auto& face : source.weights) if (face) ++result;
        return result;
    }
};

class EmbeddedFontProvider final : public skia::textlayout::TypefaceFontProvider {
    using Base = skia::textlayout::TypefaceFontProvider;
    mutable std::recursive_mutex mutex_;
    std::map<std::string, sk_sp<DeferredFontFamily>> families_;
public:
    void add(sk_sp<SkTypeface> face, const SkString& alias) {
        std::lock_guard lock(mutex_);
        auto& family = families_[alias.c_str()];
        if (!family) family = sk_make_sp<DeferredFontFamily>();
        family->add(face, face->getVariationDesignParameters({}) > 0);
        // The base registry owns family enumeration, never eager weight clones.
        Base::registerTypeface(std::move(face), alias);
    }
    int onCountFamilies() const override {
        std::lock_guard lock(mutex_); return Base::onCountFamilies();
    }
    void onGetFamilyName(int index, SkString* name) const override {
        std::lock_guard lock(mutex_); Base::onGetFamilyName(index, name);
    }
    sk_sp<SkFontStyleSet> onMatchFamily(const char name[]) const override {
        std::lock_guard lock(mutex_);
        if (!name) return nullptr;
        if (name) if (auto found = families_.find(name); found != families_.end()) return found->second;
        return Base::onMatchFamily(name);
    }
    sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override {
        std::lock_guard lock(mutex_);
        if (index < 0 || index >= Base::onCountFamilies()) return nullptr;
        SkString name; Base::onGetFamilyName(index, &name);
        return onMatchFamily(name.c_str());
    }
    sk_sp<SkTypeface> onMatchFamilyStyle(const char name[], const SkFontStyle& style) const override {
        auto family = onMatchFamily(name);
        return family ? family->matchStyle(style) : nullptr;
    }
};
} // namespace oneui::rendering
