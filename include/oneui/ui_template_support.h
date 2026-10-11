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
#include "oneui/ui_component_schema.h"
#include "oneui/ui_icon_names.h"
#include "oneui/detail/container_query_parse.h"

namespace oneui::ui::syntax {
inline std::string trim(std::string s) {
    const auto a = s.find_first_not_of(" \t\r\n"); if (a == s.npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
inline const std::map<std::string, std::string>& components() {
    static const std::map<std::string, std::string> value = [] {
        std::map<std::string,std::string> result;
        for(const auto& spec:schema::components)result.emplace(spec.name,spec.tag);
        return result;
    }();
    return value;
}
inline bool pagePattern(const std::string& tag) { return tag=="SettingsPage" || tag=="ListPage" || tag=="DetailPage"; }
inline bool property(const std::string& tag, const std::string& key) {
    const auto* spec=schema::component(tag);
    return spec && schema::supports(spec->kind,key);
}
inline bool layoutNumber(const std::string& key) {
    return schema::layoutNumber(key);
}
// Shared by the template compiler and native adapter. Reject values that Yoga
// would otherwise clamp or reinterpret, while preserving legacy Builder APIs.
inline void validateLayoutNumber(const std::string& key, float value) {
    if(key=="ratio" && (!std::isfinite(value) || value<0 || value>1))throw std::invalid_argument("ratio must be between 0 and 1");
    if (!std::isfinite(value) || value < 0 || ((key == "max-width" || key=="min-column-width" || key=="breakpoint" || key=="pane-width") && value == 0))
        throw std::invalid_argument(key + ((key == "max-width" || key=="min-column-width" || key=="breakpoint" || key=="pane-width") ? " must be a finite positive number" : " must be a finite nonnegative number"));
}
inline std::string lowerValue(std::string value) { for(auto& ch:value)ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));return value; }
inline std::set<std::string> componentCssProperties(const std::string& tag) {
    if (tag == "menu") return {"background", "background-color", "color", "placeholder-color", "border-color", "border-width", "border-radius", "padding", "font-size", "font-weight", "box-shadow"};
    if (tag == "reorderable-grid") return {"padding","gap","grid-min-column-width","outline-color","outline-width","border-color"};
    if (tag == "popup") return {"background", "background-color", "color", "border-color", "border-width", "border-radius", "padding", "gap", "box-shadow"};
    if (tag == "dialog") return {"background", "background-color", "color", "border-color", "border-width", "border-radius", "padding", "font-size", "font-weight", "box-shadow"};
    if (tag == "progress"||tag=="slider"||tag=="font-picker"||tag=="media-picker"||tag=="media-preview"||tag=="theme-scope") return {};
    if (tag == "card") return {"background-color","background","border-color","border-width","border-radius","padding","box-shadow","opacity"};
    if (tag == "interactive-surface") return {"background-color","background","border-color","border-width","border-radius","padding","transition-duration","transition-timing-function"};
    if (tag == "icon") return {"color"};
    if (tag == "tree-view") return {"background-color","content-background-color","color","placeholder-color","border-color","border-width","border-radius","content-radius","padding","font-size","font-weight","detail-font-size","detail-font-weight","text-inset","title-offset-y","detail-offset-y","outline-color","outline-width","outline-offset"};
    if (tag=="split-view") return {}; // semantic native theme
    if (tag == "list" || tag == "virtual-list") return {"background-color","content-background-color","color","placeholder-color","border-color","border-width","border-radius","content-radius","padding","font-size","font-weight","detail-font-size","detail-font-weight","text-inset","title-offset-y","detail-offset-y","scrollbar-color","scrollbar-width","outline-color","outline-width","outline-offset"};
    if (tag == "chart") return {"color","border-color","font-size","gap"};
    if (tag == "native-host") return {}; // foreign content owns its styling
    if (tag == "panes") return {"gap","padding","background-color","border-color","border-width","border-radius","box-shadow"};
    if (tag == "reveal") return {"transition-duration","transition-timing-function"};
    if (tag == "label") return {"color","font-size","font-weight","font-family"};
    if (tag == "stack") return {"background","box-shadow","background-color","border-color","border-width","border-radius","padding","gap"};
    if (tag == "scroll-view") return {"background","box-shadow","background-color","border-color","border-width","border-radius","padding","color","scrollbar-color","scrollbar-width"};
    std::set<std::string> keys = {"color","background-color","border-color","border-width","border-radius","outline-color","outline-width","outline-offset"};
    if (tag != "switch" && tag != "checkbox") keys.insert("padding");
    if (tag == "button" || tag == "input" || tag == "tabs" || tag == "select" || tag == "table") keys.insert("font-size");
    if (tag == "button" || tag == "tabs") keys.insert("font-weight");
    if (tag == "button") { keys.insert("icon-size"); keys.insert("gap"); keys.insert("opacity"); }
    if (tag == "tabs") { keys.insert("content-background-color"); keys.insert("content-radius"); }
    if (tag == "button" || tag == "input") { keys.insert("background"); keys.insert("box-shadow"); keys.insert("transition-duration"); keys.insert("transition-timing-function"); }
    if (tag == "input") { keys.insert("placeholder-color"); keys.insert("caret-color"); keys.insert("selection-color"); }
    if (tag == "select") keys.insert("content-radius");
    if (tag == "switch" || tag == "checkbox" || tag == "select" || tag == "table") keys.insert("content-background-color");
    if (tag == "table") { keys.insert("scrollbar-color"); keys.insert("scrollbar-width"); keys.insert("placeholder-color"); keys.erase("outline-color"); keys.erase("outline-width"); keys.erase("outline-offset"); keys.insert("table-column-widths"); keys.insert("table-selection-width"); keys.insert("text-inset"); keys.insert("gap"); keys.insert("detail-font-size"); keys.insert("check-background"); keys.insert("check-border"); keys.insert("check-radius"); }
    return keys;
}
inline std::set<std::string> cssProperties(const std::string& tag) {
    auto keys = componentCssProperties(tag);
    // Material opacity affects only the corresponding fill, never text/children.
    if (keys.count("background") || keys.count("background-color")) keys.insert("background-opacity");
    if (keys.count("content-background-color")) keys.insert("content-background-opacity");
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
inline std::string css(std::string source, const std::string& scope = {}, const std::string& file = "<style>", int startLine = 1, bool insideQuery=false) {
    source = stripComments(std::move(source));
    std::string output; std::size_t pos = 0;
    const std::regex selectorPattern(R"(^([A-Za-z][A-Za-z0-9-]*)(\.[A-Za-z_][A-Za-z0-9_-]*)*(:(hover|pressed|focus|disabled|selected|checked|read-only))?$)");
    while (pos < source.size()) {
        if (trim(source.substr(pos)).empty()) break;
        const int line = startLine + int(std::count(source.begin(), source.begin() + pos, '\n'));
        auto fail = [&](const std::string& message) { throw std::runtime_error(file + ":" + std::to_string(line) + ":1: " + message); };
        const auto open = source.find('{', pos);
        auto close = source.find('}', pos);
        if (open == source.npos || close == source.npos || close < open) fail("Malformed CSS rule");
        const auto selector = trim(source.substr(pos, open - pos));
        if(selector.rfind("@container",0)==0) {
            if(insideQuery)fail("CONTAINER_DEPENDENCY: nested queries are unsupported");
            try {oneui::detail::parseContainerQuery(selector);}catch(const std::exception& error){fail(error.what());}
            unsigned depth=1;close=open+1;
            for(;close<source.size()&&depth;++close){if(source[close]=='{')++depth;else if(source[close]=='}')--depth;}
            if(depth)fail("Unclosed container query");--close;
            output+=selector+" {\n"+css(source.substr(open+1,close-open-1),scope,file,line,true)+"}\n";
            pos=close+1;continue;
        }
        const auto body = source.substr(open + 1, close - open - 1);
        if (body.find('{') != body.npos) fail("Nested CSS rules are unsupported");
        std::string mapped = selector, tag, pseudo;
        if (selector != ":root") {
            const bool classOnly=std::regex_match(selector,std::regex(R"(^\.[A-Za-z_][A-Za-z0-9_-]*(\.[A-Za-z_][A-Za-z0-9_-]*)*$)"));
            if (!classOnly && !std::regex_match(selector, selectorPattern)) fail("Use one simple selector, without descendants or commas: " + selector);
            const auto endTag = selector.find_first_of(".:");
            const auto type = selector.substr(0, endTag);
            auto component = components().find(type);
            if (component != components().end()) {
                tag = component->second; mapped = tag + ".one-" + type + selector.substr(type.size());
            } else if(!classOnly) {
                tag = type;
                bool found = false; for (auto& c : components()) if (c.second == tag) found = true;
                if (!found) fail("Unknown CSS component: " + type);
            }
            const auto colon=selector.find(':');
            if(colon!=selector.npos) {
                pseudo=selector.substr(colon+1);
                std::set<std::string> states;
                if(tag=="button"||tag=="interactive-surface")states={"hover","pressed","focus","disabled"};
                else if(tag=="input")states={"hover","focus","disabled","read-only"};
                else if(tag=="switch" || tag=="checkbox" || tag=="select" || tag=="tabs")states={"hover","pressed","focus","disabled","selected"};
            else if(tag=="table" || tag=="list" || tag=="virtual-list")states={"hover","pressed","selected"};
                else if(tag=="tree-view")states={"hover","pressed","selected","focus","disabled"};
                if(tag=="switch" || tag=="checkbox")states.insert("checked");
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
            const auto cssValue=trim(declaration.substr(colon+1));
            if(property=="container-type"&&cssValue!="size")fail("CONTAINER_SIZE: only size is supported");
            if(property=="display"&&cssValue!="none"&&cssValue!="flex")fail("CSS_LAYOUT: display expects none/flex");
            if(property=="flex-direction"&&cssValue!="row"&&cssValue!="column")fail("CSS_LAYOUT: flex-direction expects row/column");
            if(property=="flex-wrap"&&cssValue!="wrap"&&cssValue!="nowrap")fail("CSS_LAYOUT: flex-wrap expects wrap/nowrap");
            if(std::set<std::string>{"width","height","min-width","min-height"}.count(property) &&
               cssValue.rfind("var(",0)!=0&&!std::regex_match(cssValue,std::regex(R"(^[0-9]+(\.[0-9]+)?px$)")))fail("CSS_LAYOUT: finite nonnegative px required");
            if(property=="flex-basis" && cssValue.rfind("var(",0)!=0 && !std::regex_match(cssValue,std::regex(R"(^[0-9]+(\.[0-9]+)?(px|%)$)")))fail("CSS_LAYOUT: flex-basis requires finite nonnegative px or percentage");
            if(property=="order" && cssValue.rfind("var(",0)!=0 && !std::regex_match(cssValue,std::regex(R"(^-?[0-9]+$)")))fail("CSS_LAYOUT: order requires an integer");
            if(property=="table-column-widths" && cssValue.rfind("var(",0)!=0) {
                std::istringstream tokens(cssValue);std::string token;bool any=false;
                while(tokens>>token) {
                    any=true;if(token=="auto")continue;
                    if(!std::regex_match(token,std::regex(R"(^[0-9]+(\.[0-9]+)?px$)")))fail("CSS_LAYOUT: table columns require auto or nonnegative px");
                    try {if(!std::isfinite(std::stof(token)))fail("CSS_LAYOUT: finite table width required");}
                    catch(const std::out_of_range&){fail("CSS_LAYOUT: table width exceeds finite range");}
                }
                if(!any)fail("CSS_LAYOUT: nonempty table column widths required");
            }
            if(property=="table-selection-width" && cssValue.rfind("var(",0)!=0) {
                if(!std::regex_match(cssValue,std::regex(R"(^[0-9]+(\.[0-9]+)?px$)")))fail("CSS_LAYOUT: table selection width requires px");
                try {const auto n=std::stof(cssValue);if(n<24||n>128)fail("CSS_LAYOUT: table selection width requires 24..128px");}
                catch(const std::out_of_range&){fail("CSS_LAYOUT: table selection width exceeds range");}
            }
            if(property=="order" && cssValue.rfind("var(",0)!=0) {
                try {const auto n=std::stoll(cssValue);if(n < -1000000 || n > 1000000)fail("CSS_LAYOUT: order exceeds range");}
                catch(const std::out_of_range&){fail("CSS_LAYOUT: order exceeds range");}
            }
            if (trim(declaration.substr(colon + 1)).empty()) fail("Empty CSS value");
            if (selector == ":root") {
                if (property.rfind("--", 0) != 0) fail(":root only accepts custom properties");
                if (!scope.empty()) fail("Scoped :root tokens are unsupported; put shared tokens in external CSS");
            } else if(insideQuery && (property=="container-name"||property=="container-type"))fail("CONTAINER_DEPENDENCY: query cannot establish a container");
            else if (!cssProperties(tag).count(property) && !std::set<std::string>{"display","width","height","min-width","min-height","flex-basis","flex-grow","flex-shrink","order"}.count(property) &&
                     !((tag=="stack"||tag.empty())&&std::set<std::string>{"container-name","container-type","flex-direction","flex-wrap","gap"}.count(property))) fail("Property is not consumed by " + tag + ": " + property);
            else if(property=="background-color" && lowerValue(trim(declaration.substr(colon+1))).find("gradient(")!=std::string::npos) fail("Use background for gradients on supported components");
            else if(tag=="table" && !pseudo.empty() && property!="background-color") fail("DataTable row states only consume background-color");
        }
        output += mapped + " {" + body + "}\n";
        pos = close + 1;
    }
    return output;
}
} // namespace oneui::ui::syntax
