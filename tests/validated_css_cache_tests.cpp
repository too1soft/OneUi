#include "oneui/detail/validated_css_cache.h"
#include <iostream>
#include <stdexcept>
#include <thread>

using oneui::ui::syntax::detail::ValidatedCssCache;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main() {
    try {
        ValidatedCssCache cache;
        cache.remember("source", "scope", false, {"External"}, "output");
        require(cache.find("source", "scope", false, {"External"}), "complete key hits");
        require(!cache.find("changed", "scope", false, {"External"}), "source isolation");
        require(!cache.find("source", "other", false, {"External"}), "scope isolation");
        require(!cache.find("source", "scope", true, {"External"}), "container isolation");
        require(!cache.find("source", "scope", false, {}), "extension isolation");
        for (std::size_t i = 0; i < ValidatedCssCache::maxEntries; ++i)
            cache.remember(std::to_string(i), {}, false, {}, "value");
        require(!cache.find("source", "scope", false, {"External"}), "entry count eviction");
        ValidatedCssCache bytes;
        const std::string payload(ValidatedCssCache::maxBytes / 3, 'a');
        bytes.remember("first", {}, false, {}, payload);
        bytes.remember("second", {}, false, {}, payload);
        bytes.remember("third", {}, false, {}, payload);
        require(!bytes.find("first", {}, false, {}), "payload budget eviction");
        require(bytes.find("third", {}, false, {}), "new entry retained");
        bytes.remember("oversize", {}, false, {}, std::string(ValidatedCssCache::maxBytes, 'b'));
        require(!bytes.find("oversize", {}, false, {}), "oversize not retained");
        auto& local = oneui::ui::syntax::detail::validatedCssCache();
        local.remember("thread", {}, false, {}, "main");
        bool independent = false;
        std::thread worker([&] { independent = !oneui::ui::syntax::detail::validatedCssCache().find("thread", {}, false, {}); });
        worker.join();
        require(independent && *local.find("thread", {}, false, {}) == "main", "independent thread ownership");
        std::cout << "Validated CSS cache contracts passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
