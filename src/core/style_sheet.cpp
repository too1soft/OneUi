#include "style_css_internal.h"
#include "style_cache_internal.h"
#include "style_layout_internal.h"
#include "internal/unicode.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <sstream>

namespace oneui {
using namespace style_detail;
void StyleSheet::RuleData::indexLastRule() {
    const auto& rule = rules.back();
    const auto& selector = selectors.back();
    hasLayoutRules = hasLayoutRules || ruleLayout(rule) != nullptr;
    if (!selector.valid) return;
    selectorClasses.insert(selector.node.classes.begin(), selector.node.classes.end());
    selectorStates |= selector.node.state;
    if (!rule.query) candidatesByTag[selector.node.tag].push_back(rules.size() - 1);
}

void StyleSheet::detachRules() {
    if (!ruleData_) ruleData_ = std::make_shared<RuleData>();
    else if (!ruleData_.unique()) ruleData_ = std::make_shared<RuleData>(*ruleData_);
}
void StyleSheet::addRule(StyleRule rule) {
    detachRules();
    auto& rules_ = ruleData_->rules;
    auto& selectors_ = ruleData_->selectors;
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
    ruleData_->indexLastRule();
    resolveCache_.clear();
    declarationCache_.clear();
    layoutCache_.clear();
    ++version_;
}

void StyleSheet::setCustomProperty(std::string name, std::string value) {
    name = normalizedCustomPropertyName(std::move(name));
    if (name.size() <= 2) {
        return;
    }
    value = trim(std::move(value));
    const auto current = customProperty(name);
    if (!current || *current != value) {
        if (!customProperties_) customProperties_ = std::make_shared<std::map<std::string, std::string>>();
        else if (!customProperties_.unique())
            customProperties_ = std::make_shared<std::map<std::string, std::string>>(*customProperties_);
        (*customProperties_)[std::move(name)] = std::move(value);
    }
    resolveCache_.clear();
    declarationCache_.clear();
    layoutCache_.clear();
    ++version_;
}

std::optional<std::string> StyleSheet::customProperty(const std::string& name) const {
    const std::string key = normalizedCustomPropertyName(name);
    const auto& tokens = customProperties();
    auto found = tokens.find(key);
    if (found == tokens.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<const StyleRule*> StyleSheet::matchingRules(const StyleNode& node) const {
    std::vector<const StyleRule*> matches;
    if (!ruleData_) return matches;
    const auto& rules_ = ruleData_->rules;
    const auto& selectors_ = ruleData_->selectors;
    const auto appendMatches = [&](const std::string& tag) {
        const auto candidates = ruleData_->candidatesByTag.find(tag);
        if (candidates == ruleData_->candidatesByTag.end()) return;
        for (const auto index : candidates->second) {
            const auto& selector = selectors_[index].node;
            if ((node.state & selector.state) == selector.state &&
                std::all_of(selector.classes.begin(), selector.classes.end(),
                    [&](const std::string& klass) { return hasClass(node, klass); })) {
                matches.push_back(&rules_[index]);
            }
        }
    };
    appendMatches({});
    if (!node.tag.empty()) appendMatches(node.tag);
    std::sort(matches.begin(), matches.end(), [&](const StyleRule* lhs, const StyleRule* rhs) {
        const auto left = selectors_[size_t(lhs - rules_.data())].specificity;
        const auto right = selectors_[size_t(rhs - rules_.data())].specificity;
        if (left != right) {
            return left < right;
        }
        if (lhs->order != rhs->order) return lhs->order < rhs->order;
        // Tag buckets are collected separately. Ties still use source order.
        return lhs < rhs;
    });
    return matches;
}

std::string StyleSheet::nodeCacheKey(const StyleNode& node) const {
    // Every lookup uses this key, including cache hits. Normal component
    // selectors fit in inline storage and can borrow the existing strings.
    std::array<std::string_view, 8> inlineClasses;
    std::vector<std::string_view> overflow;
    auto* relevant = inlineClasses.data();
    if (node.classes.size() > inlineClasses.size()) {
        overflow.resize(node.classes.size());
        relevant = overflow.data();
    }
    std::size_t count = 0;
    for (const auto& klass : node.classes)
        if (ruleData_->selectorClasses.count(klass)) relevant[count++] = klass;
    // Selector matching tests class membership, never order or multiplicity.
    // A new unmatched mount scope must not allocate a new cached result.
    std::sort(relevant, relevant + count);
    const auto end = std::unique(relevant, relevant + count);
    const auto state = std::to_string(node.state & ruleData_->selectorStates);
    std::size_t size = node.tag.size() + state.size() + 2;
    for (auto it = relevant; it != end; ++it) size += it->size() + 1;
    std::string key;
    key.reserve(size);
    key.append(node.tag);
    key.push_back('\x1f');
    for (auto it = relevant; it != end; ++it) {
        key.append(*it);
        key.push_back('\x1e');
    }
    key.push_back('\x1f');
    key.append(state);
    return key;
}

StyleBox StyleSheet::resolve(const StyleNode& node) const {
    if (!ruleData_ || ruleData_->rules.empty()) return {};
    const std::string cacheKey = nodeCacheKey(node);
    if (auto cached = resolveCache_.find(cacheKey); cached != resolveCache_.end()) return cached->second;
    StyleBox resolved;
    for (const auto* rule : matchingRules(node)) {
        StyleBox current = rule->box;
        if (!rule->declarations.empty()) {
            // A declaration depends on this sheet's tokens, not on the node
            // matching its selector. Share it across classes and pseudo-states.
            // Token/rule edits already invalidate the entire resolve cache.
            const auto declarationKey = std::size_t(rule - ruleData_->rules.data());
            if (auto cached = declarationCache_.find(declarationKey); cached != declarationCache_.end()) {
                current = cached->second;
                resolved = mergeStyleBox(std::move(resolved), current);
                continue;
            }
            StyleRule dynamicRule;
            for (const auto& [property, value] : rule->declarations) {
                // The declaration was validated when the CSS was loaded. A
                // failed runtime resolution leaves the last valid parsed box
                // in place instead of partially applying a theme.
                if (!applyDeclaration(
                        dynamicRule, property, value, customProperties(), nullptr)) {
                    dynamicRule.box = rule->box;
                    break;
                }
            }
            current = mergeStyleBox(std::move(current), dynamicRule.box);
            declarationCache_.emplace(declarationKey, current);
        }
        resolved = mergeStyleBox(std::move(resolved), current);
    }
    resolved = resolveBackgroundOpacity(std::move(resolved));
    rememberNodeStyle(resolveCache_, cacheKey, resolved);
    return resolved;
}

const std::vector<StyleRule>& StyleSheet::rules() const {
    static const std::vector<StyleRule> empty;
    return ruleData_ ? ruleData_->rules : empty;
}

StyleBox StyleSheet::resolveTokens(const std::string& declarations) const {
    struct Cached {
        std::shared_ptr<const std::map<std::string, std::string>> tokens;
        std::string declarations;
        StyleBox box;
    };
    thread_local std::vector<Cached> cache;
    for (const auto& entry : cache)
        if (entry.declarations == declarations && (entry.tokens == customProperties_ ||
            (entry.tokens && *entry.tokens == customProperties())))
            return entry.box;
    StyleSheet palette;
    for (const auto& [name, value] : customProperties())
        palette.setCustomProperty(name, value);
    if (!palette.addRulesFromCss("tokens {" + declarations + "}")) return {};
    auto box = palette.resolve({"tokens", {}, 0});
    // Include every custom property: callers may reference arbitrary names.
    // Bound ownership so theme editing cannot grow this forever.
    if (cache.size() == 16) cache.erase(cache.begin());
    cache.push_back({customProperties_, declarations, box});
    return box;
}

const std::map<std::string, std::string>& StyleSheet::customProperties() const {
    static const std::map<std::string, std::string> empty;
    return customProperties_ ? *customProperties_ : empty;
}

std::size_t StyleSheet::version() const {
    return version_;
}

}
