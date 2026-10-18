#pragma once
#include "oneui/style_sheet.h"

namespace oneui::style_detail {
// A rule's existing query pointer keeps its exact observable get()/bool()
// value. The control block owns extension data; ordinary rule copies share
// immutable state and a declaration update installs a new owner (copy on write).
struct RuleLayoutOwner {
    std::shared_ptr<const ContainerQuery> query;
    StyleLayout value;
    void operator()(const ContainerQuery*) {}
};
inline const StyleLayout* ruleLayout(const StyleRule& rule) {
    const auto* owner = std::get_deleter<RuleLayoutOwner>(rule.query);
    return owner ? &owner->value : nullptr;
}
inline void storeRuleLayout(StyleRule& rule, StyleLayout value) {
    const auto* owner = std::get_deleter<RuleLayoutOwner>(rule.query);
    auto query = owner ? owner->query : rule.query;
    auto* pointer = query.get();
    rule.query = std::shared_ptr<const ContainerQuery>(pointer, RuleLayoutOwner{std::move(query), std::move(value)});
}
inline void clearRuleQuery(StyleRule& rule) {
    if (const auto* layout = ruleLayout(rule)) {
        auto value = *layout;
        rule.query = std::shared_ptr<const ContainerQuery>(nullptr, RuleLayoutOwner{{}, std::move(value)});
    } else rule.query.reset();
}
bool layoutDeclaration(StyleLayout&, const std::string& name, const std::string& value);
} // namespace oneui::style_detail
