#pragma once
#include "component_gallery.hpp"
#include "connection_views.hpp"
#include <oneui/ui_dev.h>

namespace connection_demo {
struct StyleSession {
    Mount &mount;
    VM &vm;
    Connections &flow;
    std::filesystem::path external;
    bool dev = false;
    std::vector<std::pair<std::string, std::string>> sourceFiles = [] {
        auto files = sources_Connections(), gallery = sources_Gallery();
        files.insert(files.end(), gallery.begin(), gallery.end());
        return files;
    }();
    std::function<VisualPreset()> visualPreset;
    std::function<std::string()> effectCss;
    std::string validCss;
    std::size_t applied = 0;
    static void appendCss(std::string &output, std::string text, const std::string &scope,
                          const std::string &file, int firstLine = 1) {
        try {
            text = syntax::stripComments(std::move(text));
        } catch (const std::exception &e) {
            throw std::runtime_error(file + ":" + std::to_string(firstLine) + ":1: " + e.what());
        }
        std::size_t cursor = 0;
        while (cursor < text.size()) {
            const auto begin = text.find_first_not_of(" \t\r\n", cursor);
            if (begin == text.npos)
                break;
            const int line = firstLine + int(std::count(text.begin(), text.begin() + begin, '\n'));
            const auto close = text.find('}', begin);
            const auto rule =
                syntax::css(text.substr(begin, close == text.npos ? text.npos : close - begin + 1),
                            scope, file, line);
            StyleSheet check;
            std::string error;
            if (!check.addRulesFromCss(output + rule, &error))
                throw std::runtime_error(file + ":" + std::to_string(line) + ":1: " + error);
            output += rule;
            if (close == text.npos)
                break;
            cursor = close + 1;
        }
    }
    static void appendInline(std::string &output, const std::string &text, const std::string &scope,
                             const std::string &file) {
        std::size_t cursor = 0;
        while ((cursor = text.find("<style", cursor)) != text.npos) {
            const auto begin = text.find('>', cursor), end = text.find("</style>", begin);
            const int line = 1 + int(std::count(text.begin(), text.begin() + cursor, '\n'));
            if (begin == text.npos || end == text.npos)
                throw std::runtime_error(file + ":" + std::to_string(line) +
                                         ":1: unclosed style block");
            if (text.substr(cursor, begin - cursor).find("scoped") == text.npos)
                throw std::runtime_error(file + ":" + std::to_string(line) +
                                         ":1: inline styles require scoped");
            appendCss(output, text.substr(begin + 1, end - begin - 1), scope, file,
                      1 + int(std::count(text.begin(), text.begin() + begin + 1, '\n')));
            cursor = end + 8;
        }
    }
    bool apply() {
        try {
            const auto density = vm.density.get() == 1 ? Density::Compact : Density::Comfortable;
            auto css = declarativeTheme(vm.theme.get() == 1, density,
                                        visualPreset ? visualPreset() : VisualPreset::Standard);
            if (dev)
                for (auto &source : sourceFiles)
                    appendInline(css, readStyleFile(source.first), source.second, source.first);
            else
                css += styles_Connections() + styles_Gallery();
            if (effectCss)
                css += effectCss();
            appendCss(css, readStyleFile(external), {}, external.string());
            mount.styles()->replace(css, density);
            validCss = std::move(css);
            ++applied;
            flow.styleError.set({});
            return true;
        } catch (const std::exception &e) {
            flow.styleError.set(wide(e.what()));
            return false;
        }
    }
    std::vector<std::string> files() const {
        std::vector<std::string> paths{external.string()};
        for (auto &source : sourceFiles)
            paths.push_back(source.first);
        return paths;
    }
};
} // namespace connection_demo
