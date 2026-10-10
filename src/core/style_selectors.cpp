#include "style_css_internal.h"
#include "internal/unicode.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace oneui::style_detail {
ParsedSelector parseSelector(const std::string& selector) {
    ParsedSelector parsed;
    const auto raw_parts = split(trim(selector), ':');
    if (raw_parts.empty()) {
        return parsed;
    }

    const auto base_parts = split(trim(raw_parts.front()), '.');
    if (!base_parts.empty() && !base_parts.front().empty()) {
        parsed.tag = trim(base_parts.front());
    }
    for (std::size_t index = 1; index < base_parts.size(); ++index) {
        const std::string klass = trim(base_parts[index]);
        if (!klass.empty()) {
            parsed.classes.push_back(klass);
        }
    }

    const bool universal = parsed.tag == "*";
    if (universal) parsed.tag.clear();
    if (parsed.tag.empty() && parsed.classes.empty() && !universal) {
        return parsed;
    }

    for (std::size_t index = 1; index < raw_parts.size(); ++index) {
        const auto pseudo = parseStylePseudoState(trim(raw_parts[index]));
        if (pseudo == StyleStateNone) {
            return parsed;
        }
        parsed.pseudos |= pseudo;
    }
    parsed.valid = true;
    return parsed;
}

bool hasClass(const StyleNode& node, const std::string& klass) {
    return std::find(node.classes.begin(), node.classes.end(), klass) != node.classes.end();
}

std::string resolveCacheKey(const StyleNode& node) {
    std::string key;
    key.reserve(node.tag.size() + node.classes.size() * 12 + 16);
    key.append(node.tag);
    key.push_back('\x1f');
    for (const auto& klass : node.classes) {
        key.append(klass);
        key.push_back('\x1e');
    }
    key.push_back('\x1f');
    key.append(std::to_string(node.state));
    return key;
}
} // namespace oneui::style_detail
namespace oneui {
using namespace style_detail;
StylePseudoMask parseStylePseudoState(const std::string& pseudo) {
    if (pseudo == "hover") {
        return StyleStateHover;
    }
    if (pseudo == "active" || pseudo == "pressed") {
        return StyleStateActive;
    }
    if (pseudo == "focus" || pseudo == "focus-visible") {
        return StyleStateFocus;
    }
    if (pseudo == "disabled") {
        return StyleStateDisabled;
    }
    if (pseudo == "selected" || pseudo == "checked") {
        return StyleStateSelected;
    }
    if (pseudo == "read-only" || pseudo == "readonly") {
        return StyleStateReadOnly;
    }
    return StyleStateNone;
}

bool selectorMatches(const std::string& selector, const StyleNode& node) {
    const ParsedSelector parsed = parseSelector(selector);
    if (!parsed.valid) {
        return false;
    }
    if (!parsed.tag.empty() && parsed.tag != node.tag) {
        return false;
    }
    for (const auto& klass : parsed.classes) {
        if (!hasClass(node, klass)) {
            return false;
        }
    }
    return (node.state & parsed.pseudos) == parsed.pseudos;
}

int selectorSpecificity(const std::string& selector) {
    const ParsedSelector parsed = parseSelector(selector);
    if (!parsed.valid) {
        return 0;
    }

    int pseudo_count = 0;
    StylePseudoMask pseudos = parsed.pseudos;
    while (pseudos != 0) {
        pseudo_count += static_cast<int>(pseudos & 1u);
        pseudos >>= 1;
    }

    return (parsed.tag.empty() ? 0 : 1) +
           static_cast<int>(parsed.classes.size()) * 10 +
           pseudo_count * 10;
}

}
