#pragma once
#include <string>
#include <unordered_map>

namespace oneui::style_detail {
// Scope classes can contain a new mount identity on every open/close. Keep
// memoized node results bounded independently of parsed CSS and declarations.
template<class Value>
void rememberNodeStyle(std::unordered_map<std::string, Value>& cache,
                       const std::string& key, const Value& value) {
    constexpr std::size_t maximumNodeResults = 1024;
    if (cache.size() >= maximumNodeResults) cache.clear();
    cache.emplace(key, value);
}
}
