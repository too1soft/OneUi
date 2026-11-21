#pragma once

#include "text_layout.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>
#include <tuple>
#include <type_traits>
#include <unordered_map>

namespace oneui::text::detail {

// This cache is thread-local. List nodes own the text and retain stable
// addresses when a hit moves them to the front. The index stores only a hash
// and a list iterator, so it does not duplicate the string or layout options.
class LayoutCache {
    struct Entry {
        std::wstring value;
        LayoutOptions options;
        std::shared_ptr<Layout> layout;
        std::size_t hash;
    };
    using Entries = std::list<Entry>;

    static auto identity(const LayoutOptions& options) {
        return std::tie(options.family, options.fallbackFamily, options.size, options.weight,
                        options.width, options.lineHeight, options.scale, options.align,
                        options.maxLines, options.ellipsis, options.text.direction, options.text.wrap,
                        options.text.locale, options.text.letterSpacing, options.text.tabularNumbers);
    }

    static bool matches(const Entry& entry, const std::wstring& value, const LayoutOptions& options) {
        return entry.value == value && identity(entry.options) == identity(options) &&
               entry.options.spans == options.spans;
    }

    void evictOldest() {
        auto oldest = std::prev(entries_.end());
        const auto range = index_.equal_range(oldest->hash);
        auto indexed = std::find_if(range.first, range.second,
            [oldest](const auto& candidate) { return candidate.second == oldest; });
        index_.erase(indexed);
        textBytes_ -= oldest->value.size() * sizeof(wchar_t);
        entries_.erase(oldest);
    }

    static std::size_t lookupHash(const std::wstring& value, const LayoutOptions& options) {
        std::size_t hash = std::hash<std::wstring>{}(value);
        const auto combine = [&](const auto& field) {
            using Field = std::decay_t<decltype(field)>;
            hash ^= std::hash<Field>{}(field) + std::size_t{0x9e3779b9} + (hash << 6) + (hash >> 2);
        };
        // Width and font variants dominate duplicate strings. Resolve collisions
        // with the complete identity, including direction and styled spans.
        combine(options.width);
        combine(options.family);
        combine(options.size);
        combine(options.weight);
        return hash;
    }

    std::shared_ptr<Layout> promote(Entries::iterator entry) {
        entries_.splice(entries_.begin(), entries_, entry);
        return entry->layout;
    }

public:
    std::shared_ptr<Layout> find(const std::wstring& value, const LayoutOptions& options) {
        // A handful of layouts costs less to compare directly than to hash.
        // The full cache always uses the index.
        constexpr std::size_t directLookupLimit = 8;
        if (entries_.size() <= directLookupLimit) {
            const auto entry = std::find_if(entries_.begin(), entries_.end(),
                [&](const auto& candidate) { return matches(candidate, value, options); });
            return entry == entries_.end() ? nullptr : promote(entry);
        }
        const auto range = index_.equal_range(lookupHash(value, options));
        for (auto current = range.first; current != range.second; ++current) {
            auto entry = current->second;
            if (matches(*entry, value, options)) return promote(entry);
        }
        return {};
    }

    void insert(const std::wstring& value, const LayoutOptions& options,
                const std::shared_ptr<Layout>& layout) {
        constexpr std::size_t maxEntries = 512;
        constexpr std::size_t maxTextBytes = 1024 * 1024;
        if (value.size() > maxTextBytes / 8 / sizeof(wchar_t)) return;
        const auto bytes = value.size() * sizeof(wchar_t);
        while (entries_.size() >= maxEntries || textBytes_ + bytes > maxTextBytes) evictOldest();
        const auto hash = lookupHash(value, options);
        entries_.push_front({value, options, layout, hash});
        try {
            index_.emplace(hash, entries_.begin());
        } catch (...) {
            entries_.pop_front();
            throw;
        }
        textBytes_ += bytes;
    }

    std::size_t size() const { return entries_.size(); }
    std::size_t textBytes() const { return textBytes_; }
    void clear() {
        index_.clear();
        entries_.clear();
        textBytes_ = 0;
    }

private:
    Entries entries_;
    std::unordered_multimap<std::size_t, Entries::iterator> index_;
    std::size_t textBytes_ = 0;
};

} // namespace oneui::text::detail
