#include "style_css_internal.h"
#include "style_layout_internal.h"
#include "oneui/layout/container_query.h"
#include "../internal/view_mount_style_cache.h"
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
    const size_t maxEntries, maxSourceBytes, maxRules;
    std::list<Entry> entries;
    size_t sourceBytes = 0, ruleCount = 0;

    ParsedCssCache(size_t capacity, size_t bytes, size_t rules)
        : maxEntries(capacity), maxSourceBytes(bytes), maxRules(rules) {}

    const StyleSheet* find(const std::string& source) {
        auto found = std::find_if(entries.begin(), entries.end(),
            [&](const Entry& entry) { return entry.source == source; });
        if (found == entries.end()) return nullptr;
        entries.splice(entries.begin(), entries, found);
        return &entries.front().sheet;
    }

    const Entry* findPrefix(const std::string& source) {
        auto best = entries.end();
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            const auto length = it->source.size();
            if (length == 0 || length >= source.size() ||
                (it->source.back() != '\n' && source[length] != '\n') ||
                (best != entries.end() && length <= best->source.size())) continue;
            if (source.compare(0, length, it->source) == 0) best = it;
        }
        if (best == entries.end()) return nullptr;
        entries.splice(entries.begin(), entries, best);
        return &entries.front();
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
    thread_local ParsedCssCache cache(8, 2 * 1024 * 1024, 8192);
    return cache;
}
ParsedCssCache& viewMountStyles() {
    // Independent template sheets should not evict the shared application bases.
    thread_local ParsedCssCache cache(4, 512 * 1024, 3072);
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
void internal::retainViewMountStyles(const std::string& css) {
    auto& bases = viewMountStyles();
    if (bases.find(css)) return;
    if (const auto* compiled = parsedCssCache().find(css)) {
        try { bases.remember(css, *compiled); }
        catch (const std::bad_alloc&) { /* Parsing already succeeded. */ }
    }
}

bool StyleSheet::addRulesFromCss(const std::string& css,std::string* error) {
    try {
        const bool independent = rules().empty() && customProperties().empty();
        if (independent) {
            const auto* cached = viewMountStyles().find(css);
            if (!cached) cached = parsedCssCache().find(css);
            if (cached) {
                auto candidate = *cached;
                candidate.setTransientStyleSheet(transientStyleSheet());
                *this = std::move(candidate);
                return true;
            }
        }
        // View mounts validate their base theme before appending scoped CSS on
        // a new line. Lowered CSS can already end with that line break.
        // Start after the cached complete sheet and keep the original source
        // for diagnostics in the suffix.
        const auto* prefix = independent ? parsedCssCache().findPrefix(css) : nullptr;
        const auto* base = independent ? viewMountStyles().findPrefix(css) : nullptr;
        if (base && (!prefix || base->source.size() > prefix->source.size())) prefix = base;
        const auto startOfRules = prefix ? prefix->source.size() : 0;
        auto candidate = prefix ? prefix->sheet : *this;
        if (prefix) candidate.setTransientStyleSheet(transientStyleSheet());
        auto text=css;
        for(size_t start=startOfRules;(start=text.find("/*",start))!=std::string::npos;) {
            auto end=text.find("*/",start+2);
            if(end==std::string::npos)throw std::invalid_argument("CSS: unclosed comment");
            for(;start<end+2;++start)if(text[start]!='\n')text[start]=' ';
        }
        // Parsing is transactional, including root tokens and query rules.
        parseRules(candidate,text,startOfRules,text.size());
        *this=std::move(candidate);
        if (independent) {
            try { parsedCssCache().remember(css, *this); }
            catch (const std::bad_alloc&) { /* Parse succeeded; caching is optional. */ }
        }
        return true;
    }catch(const std::exception& failure){if(error)*error=failure.what();return false;}
}
StyleSheet StyleSheet::forContainers(const std::map<std::string,Size>& sizes) const {
    if (std::none_of(rules().begin(), rules().end(), [](const auto& rule) { return bool(rule.query); }))
        return *this;
    StyleSheet result;result.customProperties_=customProperties_;result.transient_=transient_;
    result.ruleData_ = std::make_shared<RuleData>();
    result.ruleData_->rules.reserve(rules().size());
    result.ruleData_->selectors.reserve(rules().size());
    for(std::size_t index = 0; index < rules().size(); ++index) {
        auto rule = rules()[index];
        if(rule.query) {
            auto size=sizes.find(rule.query->name);
            if(size==sizes.end()||!rule.query->matches(size->second))continue;
            clearRuleQuery(rule);
        }
        // Filtering changes query eligibility, never selector syntax. Reuse
        // the compiled selector instead of parsing every theme rule again on
        // each container layout. Tokens and resolution caches remain local.
        const auto& selector = ruleData_->selectors[index];
        result.ruleData_->rules.push_back(std::move(rule));
        result.ruleData_->selectors.push_back(selector);
        result.ruleData_->indexLastRule();
        ++result.version_;
    }
    return result;
}
} // namespace oneui
