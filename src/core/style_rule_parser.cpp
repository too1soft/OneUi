#include "style_css_internal.h"
#include "oneui/layout/container_query.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>
namespace oneui {
namespace {
using namespace style_detail;
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
        auto text=css;
        for(size_t start=0;(start=text.find("/*",start))!=std::string::npos;) {
            auto end=text.find("*/",start+2);
            if(end==std::string::npos)throw std::invalid_argument("CSS: unclosed comment");
            for(;start<end+2;++start)if(text[start]!='\n')text[start]=' ';
        }
        // Parsing is transactional, including root tokens and query rules.
        auto candidate=*this;parseRules(candidate,text,0,text.size());
        *this=std::move(candidate);return true;
    }catch(const std::exception& failure){if(error)*error=failure.what();return false;}
}
StyleSheet StyleSheet::forContainers(const std::map<std::string,Size>& sizes) const {
    StyleSheet result;result.customProperties_=customProperties_;result.transient_=transient_;
    for(auto rule:rules_) {
        if(rule.query) {
            auto size=sizes.find(rule.query->name);
            if(size==sizes.end()||!rule.query->matches(size->second))continue;
            rule.query.reset();
        }
        result.addRule(std::move(rule));
    }
    return result;
}
} // namespace oneui
