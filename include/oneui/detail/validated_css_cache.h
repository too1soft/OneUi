#pragma once
#include <cstddef>
#include <new>
#include <set>
#include <string>
#include <vector>

namespace oneui::ui::syntax::detail {
// Only successful, complete validation is remembered. File/line belong to
// diagnostics, so errors are always reparsed with their current source location.
// Every input that changes accepted selectors or output is part of the key.
class ValidatedCssCache {
    struct Entry {
        std::string source, scope, output;
        std::set<std::string> external;
        bool insideQuery = false;
        std::size_t bytes = 0;
    };
    std::vector<Entry> entries_;
    std::size_t bytes_ = 0;
public:
    static constexpr std::size_t maxBytes = 1024 * 1024;
    static constexpr std::size_t maxEntries = 8;
    const std::string* find(const std::string& source, const std::string& scope,
                            bool insideQuery, const std::set<std::string>& external) const {
        for (const auto& entry : entries_)
            if (entry.scope == scope && entry.insideQuery == insideQuery &&
                entry.external == external && entry.source == source) return &entry.output;
        return nullptr;
    }
    void remember(const std::string& source, const std::string& scope, bool insideQuery,
                  const std::set<std::string>& external, const std::string& output) {
        std::size_t bytes = source.size() + scope.size() + output.size();
        for (const auto& name : external) bytes += name.size();
        if (bytes > maxBytes) return;
        // A cache allocation failure must not turn valid CSS into an error.
        try {
            Entry entry{source, scope, output, external, insideQuery, bytes};
            while (!entries_.empty() && (entries_.size() >= maxEntries || bytes_ + bytes > maxBytes)) {
                bytes_ -= entries_.front().bytes;
                entries_.erase(entries_.begin());
            }
            entries_.push_back(std::move(entry));
            bytes_ += bytes;
        } catch (const std::bad_alloc&) { }
    }
};
inline ValidatedCssCache& validatedCssCache() {
    thread_local ValidatedCssCache cache;
    return cache;
}
}
