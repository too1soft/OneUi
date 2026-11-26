#include "style_css_internal.h"
#include "style_cache_internal.h"
#include "style_layout_internal.h"
#include "internal/unicode.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace oneui {
using namespace style_detail;
namespace {
void appendCacheId(std::string& key, std::size_t id) {
    // Self-delimiting, including when an ID crosses the one-byte boundary.
    for (; id >= 128; id >>= 7) {
        key.push_back(static_cast<char>((id & 127) | 128));
    }
    key.push_back(static_cast<char>(id));
}
} // namespace

StyleSheet::RuleData::RuleData(const RuleData& source)
    : prefix(source.prefix), rules(source.rules), selectors(source.selectors),
      candidatesByTag(source.candidatesByTag), candidatesByClass(source.candidatesByClass),
      selectorClasses(source.selectorClasses), selectorTags(source.selectorTags),
      selectorStates(source.selectorStates), hasLayoutRules(source.hasLayoutRules),
      hasQueries(source.hasQueries) {}

void StyleSheet::RuleData::indexLastRule() {
    const auto& rule = rules.back();
    const auto& selector = selectors.back();
    hasLayoutRules = hasLayoutRules || ruleLayout(rule) != nullptr;
    hasQueries = hasQueries || bool(rule.query);
    if (!selector.valid) return;
    for (const auto& klass : selector.node.classes) {
        selectorClasses.try_emplace(klass, selectorClasses.size() + 1);
    }
    if (!selector.node.tag.empty()) {
        selectorTags.try_emplace(selector.node.tag, selectorTags.size() + 1);
    }
    selectorStates |= selector.node.state;
    if (rule.query) return;
    // A classed selector is only a candidate for nodes which have its first class.
    if (selector.node.classes.empty())
        candidatesByTag[selector.node.tag].push_back(rules.size() - 1);
    else
        candidatesByClass[selector.node.classes.front()].push_back(rules.size() - 1);
}

void StyleSheet::detachRules() {
    if (!ruleData_) ruleData_ = std::make_shared<RuleData>();
    else if (!ruleData_.unique()) ruleData_ = std::make_shared<RuleData>(*ruleData_);
    else if (ruleData_->prefix) ruleData_->flatRules.reset();
}
void StyleSheet::addRule(StyleRule rule) {
    auto selector = parseSelector(rule.selector);
    RuleCompiler::append(*this, std::move(rule), std::move(selector));
}

void style_detail::RuleCompiler::append(StyleSheet& sheet, StyleRule rule, ParsedSelector selector) {
    sheet.detachRules();
    auto& data = *sheet.ruleData_;
    if (rule.order == 0) {
        rule.order = static_cast<int>(data.size()) + 1;
    }
    const int specificity = selector.specificity();
    data.rules.push_back(std::move(rule));
    data.selectors.push_back(StyleSheet::CompiledSelector{
        StyleNode{std::move(selector.tag), std::move(selector.classes), selector.pseudos},
        selector.valid, specificity
    });
    data.indexLastRule();
    sheet.resolveCache_.clear();
    sheet.declarationCache_.clear();
    sheet.layoutCache_.clear();
    ++sheet.version_;
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

std::vector<StyleSheet::RuleData::Match> StyleSheet::RuleData::match(const StyleNode& node) const {
    std::vector<Match> matches;
    const auto appendSegment = [&](const RuleData& segment, std::size_t offset) {
        const auto appendBucket = [&](const auto& buckets, const std::string& key) {
            const auto candidates = buckets.find(key);
            if (candidates == buckets.end()) return;
            for (const auto index : candidates->second) {
                const auto& compiled = segment.selectors[index];
                const auto& selector = compiled.node;
                if ((selector.tag.empty() || selector.tag == node.tag) &&
                    (node.state & selector.state) == selector.state &&
                    std::all_of(selector.classes.begin(), selector.classes.end(),
                        [&](const std::string& klass) { return hasClass(node, klass); })) {
                    matches.push_back({&segment.rules[index], offset + index, compiled.specificity});
                }
            }
        };
        appendBucket(segment.candidatesByTag, {});
        if (!node.tag.empty()) appendBucket(segment.candidatesByTag, node.tag);
        for (auto klass = node.classes.begin(); klass != node.classes.end(); ++klass)
            if (std::find(node.classes.begin(), klass, *klass) == klass)
                appendBucket(segment.candidatesByClass, *klass);
    };
    if (prefix) appendSegment(*prefix, 0);
    appendSegment(*this, prefixSize());
    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
        if (a.specificity != b.specificity) return a.specificity < b.specificity;
        if (a.rule->order != b.rule->order) return a.rule->order < b.rule->order;
        return a.index < b.index;
    });
    return matches;
}

std::vector<const StyleRule*> StyleSheet::matchingRules(const StyleNode& node) const {
    if (!ruleData_) return {};
    const auto matches = ruleData_->match(node);
    std::vector<const StyleRule*> rules;
    rules.reserve(matches.size());
    for (const auto& match : matches) rules.push_back(match.rule);
    return rules;
}

std::string StyleSheet::nodeCacheKey(const StyleNode& node) const {
    // Use stylesheet-local IDs so long, scoped class names don't allocate
    // a new lookup string each time a widget resolves paint or layout.
    std::array<std::size_t, 8> inlineClasses;
    std::vector<std::size_t> overflow;
    auto* relevant = inlineClasses.data();
    if (node.classes.size() > inlineClasses.size()) {
        overflow.resize(node.classes.size());
        relevant = overflow.data();
    }
    std::size_t count = 0;
    const auto layeredId = [](const auto& ids, const auto* prefix, const std::string& key) {
        if (auto found = ids.find(key); found != ids.end())
            return (prefix ? prefix->size() : 0) + found->second;
        if (prefix)
            if (auto found = prefix->find(key); found != prefix->end()) return found->second;
        return std::size_t{0};
    };
    const auto* prefix = ruleData_->prefix.get();
    for (const auto& klass : node.classes) {
        const auto id = layeredId(ruleData_->selectorClasses, prefix ? &prefix->selectorClasses : nullptr, klass);
        if (id) relevant[count++] = id;
    }
    // Selector matching tests class membership, never order or multiplicity.
    // A new unmatched mount scope must not allocate a new cached result.
    std::sort(relevant, relevant + count);
    const auto end = std::unique(relevant, relevant + count);
    std::string key;
    appendCacheId(key, layeredId(ruleData_->selectorTags, prefix ? &prefix->selectorTags : nullptr, node.tag));
    appendCacheId(key, node.state & ruleData_->selectorStates);
    for (auto it = relevant; it != end; ++it) appendCacheId(key, *it);
    return key;
}

StyleBox StyleSheet::resolve(const StyleNode& node) const {
    if (!ruleData_ || ruleData_->size() == 0) return {};
    const std::string cacheKey = nodeCacheKey(node);
    if (auto cached = resolveCache_.find(cacheKey); cached != resolveCache_.end()) return cached->second;
    StyleBox resolved;
    for (const auto& match : ruleData_->match(node)) {
        const auto* rule = match.rule;
        if (rule->declarations.empty()) {
            mergeStyleBoxInto(resolved, rule->box);
            continue;
        }
        // A declaration depends on this sheet's tokens, not on the node
        // matching its selector. Share it across classes and pseudo-states.
        // Token/rule edits already invalidate the entire resolve cache.
        const auto declarationKey = match.index;
        auto cached = declarationCache_.find(declarationKey);
        if (cached == declarationCache_.end()) {
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
            auto current = rule->box;
            mergeStyleBoxInto(current, dynamicRule.box);
            cached = declarationCache_.emplace(declarationKey, std::move(current)).first;
        }
        mergeStyleBoxInto(resolved, cached->second);
    }
    applyBackgroundOpacity(resolved);
    rememberNodeStyle(resolveCache_, cacheKey, resolved);
    return resolved;
}

const std::vector<StyleRule>& StyleSheet::rules() const {
    static const std::vector<StyleRule> empty;
    if (!ruleData_) return empty;
    if (!ruleData_->prefix) return ruleData_->rules;
    auto cached = std::atomic_load(&ruleData_->flatRules);
    if (cached) return *cached;
    auto merged = std::make_shared<std::vector<StyleRule>>();
    merged->reserve(ruleData_->size());
    visitRules([&](const StyleRule& rule) { merged->push_back(rule); });
    std::shared_ptr<const std::vector<StyleRule>> view = std::move(merged);
    if (std::atomic_compare_exchange_strong(&ruleData_->flatRules, &cached, view)) return *view;
    return *cached;
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
