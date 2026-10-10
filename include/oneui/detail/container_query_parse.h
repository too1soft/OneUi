#pragma once
#include "oneui/layout/container_query.h"
#include <regex>
#include <stdexcept>
#include <cmath>
namespace oneui::detail {
inline ContainerQuery parseContainerQuery(const std::string& source) {
    static const std::regex head(R"(^\s*@container\s+([A-Za-z_][A-Za-z0-9_-]*)\s+(.+?)\s*$)");
    static const std::regex condition(R"(^\s*\(\s*(width|height|aspect-ratio)\s*(<=|>=|=|<|>)\s*([0-9]+(?:\.[0-9]+)?)(px|\s*/\s*[1-9][0-9]*)\s*\)\s*)");
    std::smatch match;
    if(!std::regex_match(source,match,head))throw std::invalid_argument("CONTAINER_SYNTAX: expected named @container");
    ContainerQuery query;query.name=match[1];std::string rest=match[2];
    while(!rest.empty()) {
        if(!std::regex_search(rest,match,condition))throw std::invalid_argument("CONTAINER_SYNTAX: expected width/height px or aspect-ratio comparison");
        ContainerQueryTerm term;
        term.axis=match[1]=="width"?ContainerQueryTerm::Axis::Width:match[1]=="height"?ContainerQueryTerm::Axis::Height:ContainerQueryTerm::Axis::Aspect;
        const auto op=match[2].str();
        term.compare=op=="<"?ContainerQueryTerm::Compare::Less:op=="<="?ContainerQueryTerm::Compare::LessEqual:op=="="?ContainerQueryTerm::Compare::Equal:op==">="?ContainerQueryTerm::Compare::GreaterEqual:ContainerQueryTerm::Compare::Greater;
        term.numerator=std::stod(match[3]);auto unit=match[4].str();
        if(term.axis==ContainerQueryTerm::Axis::Aspect) {
            if(unit=="px" || term.numerator<=0 || std::floor(term.numerator)!=term.numerator)throw std::invalid_argument("CONTAINER_RATIO: positive integer fraction required");
            term.denominator=std::stod(unit.substr(unit.find('/')+1));
        } else if(unit!="px")throw std::invalid_argument("CONTAINER_UNIT: dimensions require px");
        if(!std::isfinite(term.numerator)||!std::isfinite(term.denominator))throw std::invalid_argument("CONTAINER_NUMBER: finite value required");
        query.terms.push_back(term);rest=match.suffix();
        if(rest.empty())break;
        if(rest.rfind("and ",0)!=0)throw std::invalid_argument("CONTAINER_SYNTAX: only and is supported");
        rest.erase(0,4);if(rest.empty())throw std::invalid_argument("CONTAINER_SYNTAX: missing condition");
    }
    if(query.terms.empty())throw std::invalid_argument("CONTAINER_SYNTAX: empty query");
    return query;
}
} // namespace oneui
