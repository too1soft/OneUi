#pragma once
#include "internal/unicode.h"
#include <string_view>
#include <vector>
namespace oneui::platform {
// text/uri-list only carries local absolute file URLs. Never interpret remote
// URLs, embedded NULs, or malformed escapes as filesystem paths.
inline std::vector<std::wstring> filePathsFromUriList(std::string_view list) {
    std::vector<std::wstring> paths;
    if (list.size() > 4u * 1024u * 1024u) return paths;
    auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
    while (!list.empty() && paths.size() < 4096) {
        const auto end = list.find('\n');
        auto line = list.substr(0, end);
        list = end == std::string_view::npos ? std::string_view{} : list.substr(end + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.substr(0, 5) != "file:") continue;
        line.remove_prefix(5);
        if (line.substr(0, 2) == "//") {
            line.remove_prefix(2);
            const auto slash = line.find('/');
            if (slash == std::string_view::npos) continue;
            const auto host = line.substr(0, slash);
            if (!host.empty() && host != "localhost") continue;
            line.remove_prefix(slash);
        }
        if (line.empty() || line.front() != '/' || line.find_first_of("?#") != std::string_view::npos) continue;
        std::string decoded;
        bool valid = true;
        for (std::size_t i = 0; i < line.size(); ++i) {
            char byte = line[i];
            if (byte == '%') {
                if (i + 2 >= line.size() || hex(line[i+1]) < 0 || hex(line[i+2]) < 0) { valid = false; break; }
                byte = static_cast<char>(hex(line[i+1]) * 16 + hex(line[i+2])); i += 2;
            }
            if (!byte) { valid = false; break; }
            decoded += byte;
        }
        if (valid) {
            auto wide = unicode::fromUtf8(decoded);
            if (unicode::toUtf8(wide) == decoded) paths.push_back(std::move(wide));
        }
    }
    return paths;
}
} // namespace oneui::platform
