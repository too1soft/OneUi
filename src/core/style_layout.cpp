#include "style_css_internal.h"
#include "style_layout_internal.h"
#include <algorithm>
#include <cmath>

namespace oneui::style_detail {
bool layoutDeclaration(StyleLayout& result, const std::string& name, const std::string& value) {
    if (name == "max-width" || name == "max-height") {
        const auto number = parsePx(value);
        if (!number || !std::isfinite(*number) || *number < 0) return false;
        (name == "max-width" ? result.maxWidth : result.maxHeight) = *number;
        return true;
    }
    if (name == "order") {
        const auto number = parseNumber(value);
        if (!number || !std::isfinite(*number) || std::floor(*number) != *number ||
            *number < -1000000 || *number > 1000000) return false;
        result.flexOrder = int(*number); return true;
    }
    if (name == "flex-basis") {
        const bool percent = !value.empty() && value.back() == '%';
        const auto number = percent ? parseNumber(value.substr(0, value.size() - 1)) : parsePx(value);
        if (!number || !std::isfinite(*number) || *number < 0) return false;
        result.flexBasisSpecified = true;
        result.flexBasisPercent = percent ? number : std::optional<float>{};
        return true;
    }
    if (name == "table-column-widths") {
        std::vector<float> widths;
        for (const auto& token : splitWhitespaceTopLevel(value)) {
            if (lower(token) == "auto") widths.push_back(0);
            else if (const auto width = parsePx(token); width && std::isfinite(*width) && *width >= 0)
                widths.push_back(*width);
            else return false;
        }
        if (widths.empty()) return false;
        result.tableColumnWidths = std::move(widths); return true;
    }
    if (name == "table-selection-width") {
        const auto width = parsePx(value);
        if (!width || !std::isfinite(*width) || *width < 24 || *width > 128) return false;
        result.tableSelectionWidth = *width; return true;
    }
    return false;
}
} // namespace oneui::style_detail

namespace oneui {
StyleLayout StyleSheet::resolveLayout(const StyleNode& node) const {
    using namespace style_detail;
    // No new allocations/sorting or declaration parsing for an ordinary sheet.
    if (std::none_of(rules_.begin(), rules_.end(), [](const auto& rule) { return ruleLayout(rule); })) return {};
    StyleLayout resolved;
    for (const auto* rule : matchingRules(node)) {
        const auto* initial = ruleLayout(*rule);
        StyleLayout current = initial ? *initial : StyleLayout{};
        if (rule->box.flexBasis) { current.flexBasisSpecified = true; current.flexBasisPercent.reset(); }
        if (!rule->declarations.empty()) {
            StyleRule dynamic;
            for (const auto& [property, value] : rule->declarations)
                if (!applyDeclaration(dynamic, property, value, customProperties_, nullptr)) {
                    dynamic = *rule; break;
                }
            if (const auto* layout = ruleLayout(dynamic)) current = *layout;
            if (dynamic.box.flexBasis) { current.flexBasisSpecified = true; current.flexBasisPercent.reset(); }
        }
        if (current.flexBasisSpecified) {
            resolved.flexBasisSpecified = true;
            resolved.flexBasisPercent = current.flexBasisPercent;
        }
        if (current.flexOrder) resolved.flexOrder = current.flexOrder;
        if (current.maxWidth) resolved.maxWidth = current.maxWidth;
        if (current.maxHeight) resolved.maxHeight = current.maxHeight;
        if (current.tableColumnWidths) resolved.tableColumnWidths = current.tableColumnWidths;
        if (current.tableSelectionWidth) resolved.tableSelectionWidth = current.tableSelectionWidth;
    }
    return resolved;
}
} // namespace oneui
