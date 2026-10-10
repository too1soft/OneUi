#include "style_css_internal.h"
#include "internal/unicode.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace oneui {
using namespace style_detail;
void StyleSheet::addRule(StyleRule rule) {
    if (rule.order == 0) {
        rule.order = static_cast<int>(rules_.size()) + 1;
    }
    const ParsedSelector parsed = parseSelector(rule.selector);
    CompiledSelector compiled;
    compiled.node = StyleNode{parsed.tag, parsed.classes, parsed.pseudos};
    compiled.valid = parsed.valid;
    compiled.specificity = selectorSpecificity(rule.selector);
    rules_.push_back(std::move(rule));
    selectors_.push_back(std::move(compiled));
    resolveCache_.clear();
    ++version_;
}

void StyleSheet::setCustomProperty(std::string name, std::string value) {
    name = normalizedCustomPropertyName(std::move(name));
    if (name.size() <= 2) {
        return;
    }
    customProperties_[std::move(name)] = trim(std::move(value));
    resolveCache_.clear();
    ++version_;
}

std::optional<std::string> StyleSheet::customProperty(const std::string& name) const {
    const std::string key = normalizedCustomPropertyName(name);
    auto found = customProperties_.find(key);
    if (found == customProperties_.end()) {
        return std::nullopt;
    }
    return found->second;
}

StyleBox StyleSheet::resolve(const StyleNode& node) const {
    const std::string cacheKey = resolveCacheKey(node);
    if (auto cached = resolveCache_.find(cacheKey); cached != resolveCache_.end()) {
        return cached->second;
    }

    struct Match {
        const StyleRule* rule = nullptr;
        int specificity = 0;
    };

    std::vector<Match> matches;
    for (std::size_t index = 0; index < rules_.size(); ++index) {
        if(rules_[index].query)continue;
        const auto& compiled = selectors_[index];
        if (!compiled.valid ||
            (!compiled.node.tag.empty() && compiled.node.tag != node.tag) ||
            (node.state & compiled.node.state) != compiled.node.state) {
            continue;
        }
        if (!std::all_of(compiled.node.classes.begin(), compiled.node.classes.end(),
                [&](const std::string& klass) { return hasClass(node, klass); })) {
            continue;
        }
        matches.push_back(Match{&rules_[index], compiled.specificity});
    }

    std::stable_sort(matches.begin(), matches.end(), [](const Match& lhs, const Match& rhs) {
        if (lhs.specificity != rhs.specificity) {
            return lhs.specificity < rhs.specificity;
        }
        return lhs.rule->order < rhs.rule->order;
    });

    StyleBox resolved;
    for (const auto& match : matches) {
        StyleBox current = match.rule->box;
        if (!match.rule->declarations.empty()) {
            StyleRule dynamicRule;
            for (const auto& [property, value] : match.rule->declarations) {
                // The declaration was validated when the CSS was loaded. A
                // failed runtime resolution leaves the last valid parsed box
                // in place instead of partially applying a theme.
                if (!applyDeclaration(
                        dynamicRule, property, value, customProperties_, nullptr)) {
                    dynamicRule.box = match.rule->box;
                    break;
                }
            }
            current = mergeStyleBox(std::move(current), dynamicRule.box);
        }
        resolved = mergeStyleBox(std::move(resolved), current);
    }
    resolved = resolveBackgroundOpacity(std::move(resolved));
    resolveCache_.emplace(cacheKey, resolved);
    return resolved;
}

const std::vector<StyleRule>& StyleSheet::rules() const {
    return rules_;
}

const std::map<std::string, std::string>& StyleSheet::customProperties() const {
    return customProperties_;
}

std::size_t StyleSheet::version() const {
    return version_;
}

}
