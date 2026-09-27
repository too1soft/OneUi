#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace oneui::ui::syntax {
inline std::string trim(std::string s) {
    const auto a = s.find_first_not_of(" \t\r\n"); if (a == s.npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
inline const std::map<std::string, std::string>& components() {
    static const std::map<std::string, std::string> value = {
        {"Page","stack"},{"Header","stack"},{"Toolbar","stack"},{"Section","stack"},
        {"SettingsPage","stack"},{"ListPage","stack"},{"DetailPage","stack"},{"ActionBar","stack"},{"LoadingState","stack"},{"Status","label"},
        {"Column","stack"},{"Row","stack"},{"Content","stack"},{"FormRow","stack"},{"FormGrid","stack"},{"EmptyState","stack"},
        {"Scroll","scroll-view"},{"Text","label"},{"ValidationMessage","label"},
        {"Input","input"},{"SearchInput","input"},{"Switch","switch"},{"Select","select"},
        {"Button","button"},{"DataTable","table"}};
    return value;
}
inline bool pagePattern(const std::string& tag) { return tag=="SettingsPage" || tag=="ListPage" || tag=="DetailPage"; }
inline bool property(const std::string& tag, const std::string& key) {
    if (key == "min-column-width") return tag == "FormGrid";
    if (key == "tone") return tag == "Status";
    if (key == "error") return tag == "FormRow";
    if (key == "subtitle") return pagePattern(tag) || tag=="EmptyState" || tag=="LoadingState";
    if (key == "max-width" || key == "align") return tag == "Content";
    if (key == "class" || key == "grow" || key == "basis" || key == "min" || key == "max" || key == "shrink" || key == "visible" || key == "disabled" || key == "name" || key == "description") return true;
    if (key == "text") return tag == "Text" || tag == "Status" || tag == "ValidationMessage" || tag == "Button" || tag == "Switch" || tag == "Input" || tag == "SearchInput";
    if (key == "title") return pagePattern(tag) || tag == "Page" || tag == "Header" || tag == "Section" || tag == "EmptyState" || tag == "LoadingState";
    if (key == "label" || key == "hint") return tag == "FormRow";
    if (key == "placeholder") return tag == "Input" || tag == "SearchInput";
    if (key == "variant") return tag == "Button";
    if (key == "items") return tag == "Select" || tag == "DataTable";
    if (key == "columns" || key == "selectedKey" || key == "item-key") return tag == "DataTable";
    if (key == "checked") return tag == "Switch";
    if (key == "selectedIndex") return tag == "Select";
    return false;
}
inline bool layoutNumber(const std::string& key) {
    return key == "grow" || key == "shrink" || key == "basis" || key == "min" || key == "max" || key == "max-width" || key == "min-column-width";
}
// Shared by the template compiler and native adapter. Reject values that Yoga
// would otherwise clamp or reinterpret, while preserving legacy Builder APIs.
inline void validateLayoutNumber(const std::string& key, float value) {
    if (!std::isfinite(value) || value < 0 || ((key == "max-width" || key=="min-column-width") && value == 0))
        throw std::invalid_argument(key + ((key == "max-width" || key=="min-column-width") ? " must be a finite positive number" : " must be a finite nonnegative number"));
}
inline std::set<std::string> cssProperties(const std::string& tag) {
    if (tag == "label") return {"color","font-size","font-weight"};
    if (tag == "stack") return {"background-color","border-color","border-width","border-radius","padding","gap"};
    if (tag == "scroll-view") return {"background-color","border-color","border-width","border-radius","padding"};
    std::set<std::string> keys = {"color","background-color","border-color","border-width","border-radius","outline-color","outline-width","outline-offset"};
    if (tag != "switch") keys.insert("padding");
    if (tag == "button" || tag == "select" || tag == "table") keys.insert("font-size");
    if (tag == "button") keys.insert("font-weight");
    if (tag == "button") { keys.insert("transition-duration"); keys.insert("transition-timing-function"); }
    if (tag == "input") { keys.insert("placeholder-color"); keys.insert("caret-color"); keys.insert("selection-color"); }
    if (tag == "switch" || tag == "select" || tag == "table") keys.insert("content-background-color");
    if (tag == "table") { keys.insert("scrollbar-color"); keys.insert("scrollbar-width"); keys.insert("placeholder-color"); keys.erase("outline-color"); keys.erase("outline-width"); keys.erase("outline-offset"); }
    return keys;
}
inline std::string stripComments(std::string source) {
    std::size_t at = 0;
    while ((at = source.find("/*", at)) != source.npos) {
        const auto end = source.find("*/", at + 2);
        if (end == source.npos) throw std::runtime_error("Unclosed CSS comment");
        for (; at < end + 2; ++at) if (source[at] != '\n') source[at] = ' ';
    }
    return source;
}
// Deliberately small CSS dialect: typed simple selectors, states and variables.
// Unknown/unconsumed properties are errors rather than successful no-ops.
inline std::string css(std::string source, const std::string& scope = {}, const std::string& file = "<style>", int startLine = 1) {
    source = stripComments(std::move(source));
    std::string output; std::size_t pos = 0;
    const std::regex selectorPattern(R"(^([A-Za-z][A-Za-z0-9-]*)(\.[A-Za-z_][A-Za-z0-9_-]*)*(:(hover|pressed|focus|disabled|selected|checked|read-only))?$)");
    while (pos < source.size()) {
        if (trim(source.substr(pos)).empty()) break;
        const int line = startLine + int(std::count(source.begin(), source.begin() + pos, '\n'));
        auto fail = [&](const std::string& message) { throw std::runtime_error(file + ":" + std::to_string(line) + ":1: " + message); };
        const auto open = source.find('{', pos), close = source.find('}', pos);
        if (open == source.npos || close == source.npos || close < open) fail("Malformed CSS rule");
        const auto selector = trim(source.substr(pos, open - pos));
        const auto body = source.substr(open + 1, close - open - 1);
        if (body.find('{') != body.npos) fail("Nested CSS rules are unsupported");
        std::string mapped = selector, tag, pseudo;
        if (selector != ":root") {
            if (!std::regex_match(selector, selectorPattern)) fail("Use one typed simple selector, without descendants or commas: " + selector);
            const auto endTag = selector.find_first_of(".:");
            const auto type = selector.substr(0, endTag);
            auto component = components().find(type);
            if (component != components().end()) {
                tag = component->second; mapped = tag + ".one-" + type + selector.substr(type.size());
            } else {
                tag = type;
                bool found = false; for (auto& c : components()) if (c.second == tag) found = true;
                if (!found) fail("Unknown CSS component: " + type);
            }
            const auto colon=selector.find(':');
            if(colon!=selector.npos) {
                pseudo=selector.substr(colon+1);
                std::set<std::string> states;
                if(tag=="button")states={"hover","pressed","focus","disabled"};
                else if(tag=="input")states={"hover","focus","disabled","read-only"};
                else if(tag=="switch" || tag=="select")states={"hover","pressed","focus","disabled","selected"};
                else if(tag=="table")states={"hover","pressed","selected"};
                if(!states.count(pseudo))fail("State is not consumed by "+tag+": "+pseudo);
            }
            if (!scope.empty()) { const auto pseudo = mapped.find(':'); mapped.insert(pseudo == mapped.npos ? mapped.size() : pseudo, "." + scope); }
        }
        std::istringstream declarations(body); std::string declaration;
        while (std::getline(declarations, declaration, ';')) {
            declaration = trim(declaration); if (declaration.empty()) continue;
            const auto colon = declaration.find(':');
            if (colon == declaration.npos) fail("Expected property: value");
            const auto property = trim(declaration.substr(0, colon));
            if (trim(declaration.substr(colon + 1)).empty()) fail("Empty CSS value");
            if (selector == ":root") {
                if (property.rfind("--", 0) != 0) fail(":root only accepts custom properties");
                if (!scope.empty()) fail("Scoped :root tokens are unsupported; put shared tokens in external CSS");
            } else if (!cssProperties(tag).count(property)) fail("Property is not consumed by " + tag + ": " + property);
            else if(tag=="table" && !pseudo.empty() && property!="background-color") fail("DataTable row states only consume background-color");
        }
        output += mapped + " {" + body + "}\n";
        pos = close + 1;
    }
    return output;
}
} // namespace oneui::ui::syntax
