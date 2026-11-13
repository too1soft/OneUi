#include "style_css_internal.h"
#include "style_layout_internal.h"
#include "oneui/layout/container_query.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <list>
namespace oneui {
namespace {
using namespace style_detail;
// Declaration validation and independent view mounts parse the same theme.
// Keep only immutable parse results; every caller receives its own sheet,
// tokens and resolution caches. Existing rules/tokens must still be reparsed
// because an appended declaration can depend on the caller's environment.
struct ParsedCssCache {
    struct Entry {
        std::string source;
        StyleSheet sheet;
    };
    static constexpr size_t maxEntries = 8;
    static constexpr size_t maxSourceBytes = 2 * 1024 * 1024;
    static constexpr size_t maxRules = 8192;
    std::list<Entry> entries;
    size_t sourceBytes = 0, ruleCount = 0;

    const StyleSheet* find(const std::string& source) {
        auto found = std::find_if(entries.begin(), entries.end(),
            [&](const Entry& entry) { return entry.source == source; });
        if (found == entries.end()) return nullptr;
        entries.splice(entries.begin(), entries, found);
        return &entries.front().sheet;
    }
    void remember(const std::string& source, const StyleSheet& sheet) {
        if (source.size() > maxSourceBytes || sheet.rules().size() > maxRules) return;
        // Allocate before evicting or changing counters; a failed optimization
        // must not turn successfully parsed CSS into a failed style update.
        entries.push_front({source, sheet});
        entries.front().sheet.setTransientStyleSheet({});
        sourceBytes += source.size();
        ruleCount += sheet.rules().size();
        while (entries.size() > maxEntries || sourceBytes > maxSourceBytes || ruleCount > maxRules) {
            sourceBytes -= entries.back().source.size();
            ruleCount -= entries.back().sheet.rules().size();
            entries.pop_back();
        }
    }
};
ParsedCssCache& parsedCssCache() {
    thread_local ParsedCssCache cache;
    return cache;
}
void parseRules(StyleSheet& sheet,const std::string& text,size_t begin,size_t end,
                std::shared_ptr<const ContainerQuery> query={}) {
    size_t cursor=begin;
    while(cursor<end) {
        while(cursor<end&&std::isspace(static_cast<unsigned char>(text[cursor])))++cursor;
        if(cursor==end)break;
        auto fail=[&](const std::string& message) {
            auto line=1+std::count(text.begin(),text.begin()+cursor,'\n');
            auto newline=text.rfind('\n',cursor);auto column=newline==std::string::npos?cursor+1:cursor-newline;
            throw std::invalid_argument("CSS:"+std::to_string(line)+":"+std::to_string(column)+": "+message);
        };
        auto open=text.find('{',cursor);
        if(open==std::string::npos||open>=end)fail("Expected rule block");
        size_t close=open+1;unsigned depth=1;char quote=0;
        for(;close<end&&depth;++close) {
            char c=text[close];
            if(quote){if(c=='\\'){++close;continue;}if(c==quote)quote=0;continue;}
            if(c=='\''||c=='"'){quote=c;continue;}
            if(c=='{')++depth;else if(c=='}')--depth;
        }
        if(depth)fail("Unclosed style rule block");
        --close;auto selector=trim(text.substr(cursor,open-cursor));
        if(selector.rfind("@container",0)==0) {
            if(query)fail("CONTAINER_DEPENDENCY: nested query blocks are unsupported");
            try {parseRules(sheet,text,open+1,close,std::make_shared<ContainerQuery>(parseContainerQuery(selector)));}
            catch(const std::exception& error){fail(error.what());}
        } else {
            if(selector.empty()||selector[0]=='@')fail("Unsupported or empty rule selector");
            auto body=text.substr(open+1,close-open-1);
            if(body.find('{')!=std::string::npos)fail("Nested style rule");
            for(const auto& raw:split(selector,',')) {
                StyleRule rule;rule.selector=trim(raw);rule.query=query;
                if(rule.selector!=":root"&&!parseSelector(rule.selector).valid)fail("Invalid selector");
                for(const auto& declaration:splitTopLevel(body,';')) {
                    if(trim(declaration).empty())continue;
                    auto colon=declaration.find(':');if(colon==std::string::npos)fail("Expected property: value");
                    auto property=trim(declaration.substr(0,colon)),value=trim(declaration.substr(colon+1));
                    if(rule.selector==":root") {
                        if(query||!isCustomPropertyName(property))fail("Root only accepts unconditional custom properties");
                        sheet.setCustomProperty(property,value);continue;
                    }
                    if(query&&(property=="container-name"||property=="container-type"))fail("CONTAINER_DEPENDENCY: query cannot establish a container");
                    std::string error;
                    if(!applyDeclaration(rule,property,value,sheet.customProperties(),&error))fail(error);
                    rule.declarations.emplace_back(std::move(property),std::move(value));
                }
                if(rule.selector!=":root")sheet.addRule(std::move(rule));
            }
        }
        cursor=close+1;
    }
}
}
bool StyleSheet::addRulesFromCss(const std::string& css,std::string* error) {
    try {
        const bool independent = rules_.empty() && customProperties_.empty();
        if (independent) {
            if (const auto* cached = parsedCssCache().find(css)) {
                auto candidate = *cached;
                candidate.setTransientStyleSheet(transientStyleSheet());
                *this = std::move(candidate);
                return true;
            }
        }
        auto text=css;
        for(size_t start=0;(start=text.find("/*",start))!=std::string::npos;) {
            auto end=text.find("*/",start+2);
            if(end==std::string::npos)throw std::invalid_argument("CSS: unclosed comment");
            for(;start<end+2;++start)if(text[start]!='\n')text[start]=' ';
        }
        // Parsing is transactional, including root tokens and query rules.
        auto candidate=*this;parseRules(candidate,text,0,text.size());
        *this=std::move(candidate);
        if (independent) {
            try { parsedCssCache().remember(css, *this); }
            catch (const std::bad_alloc&) { /* Parse succeeded; caching is optional. */ }
        }
        return true;
    }catch(const std::exception& failure){if(error)*error=failure.what();return false;}
}
StyleSheet StyleSheet::forContainers(const std::map<std::string,Size>& sizes) const {
    StyleSheet result;result.customProperties_=customProperties_;result.transient_=transient_;
    for(auto rule:rules_) {
        if(rule.query) {
            auto size=sizes.find(rule.query->name);
            if(size==sizes.end()||!rule.query->matches(size->second))continue;
            clearRuleQuery(rule);
        }
        result.addRule(std::move(rule));
    }
    return result;
}
} // namespace oneui
