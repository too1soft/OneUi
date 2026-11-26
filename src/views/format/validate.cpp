#include "validation.h"
#include <cmath>

namespace oneview
{
namespace
{
bool asciiDigit(char value)
{
    return value >= '0' && value <= '9';
}

bool decimalLiteral(const std::string &text)
{
    const auto dot = std::find(text.begin(), text.end(), '.');
    return dot != text.begin() && std::all_of(text.begin(), dot, asciiDigit) &&
           (dot == text.end() || (dot + 1 != text.end() && std::all_of(dot + 1, text.end(), asciiDigit)));
}
} // namespace

void validateChildren(const Node &n, const ComponentContract &c, const Location &at)
{
    validateChildSequence(n.children.size(), [&](size_t i) {
        const auto &child = n.children[i];
        return ChildShape{child.tag, child.attrs.count("v-for") != 0};
    }, c, at);
}
void validateStatic(const std::string &component, const std::string &key, const std::string &text)
{
    const auto &p = registry().at(component).property(key);
    require(p.flags & 1, "STATIC_TYPE: property requires binding");
    if (p.tag == 1)
        require(text == "true" || text == "false", "STATIC_TYPE: expected bool");
    if (p.tag == 2)
    {
        require(text == "-1" || (!text.empty() && std::all_of(text.begin(), text.end(), asciiDigit)),
                "STATIC_TYPE: selectedIndex >= -1");
        size_t end = 0;
        auto n = std::stoll(text, &end);
        require(end == text.size() && n <= std::numeric_limits<int>::max(), "STATIC_TYPE: integer range");
    }
    if (p.tag == 3)
    {
        require(decimalLiteral(text), "STATIC_TYPE: finite nonnegative number");
        float f = std::stof(text);
        oneui::ui::syntax::validateLayoutNumber(key, f);
        require(std::isfinite(f), "STATIC_TYPE: finite number");
    }
    static const std::map<std::string, std::set<std::string>> enums = {
        {"orientation", {"horizontal", "vertical"}},
        {"presentation", {"document", "segmented"}},
        {"sizing", {"compact", "equal"}},
        {"variant", {"normal", "primary", "danger", "ghost"}},
        {"justify", {"start", "center", "end", "space-between"}},
        {"preset", {"fade", "expand"}},
        {"tone", {"neutral", "success", "warning", "error", "pending"}},
        {"appearance", {"flat", "outlined", "raised", "tinted"}},
        {"item-key", {"id"}}};
    if(key=="resize-policy")require(text=="settled"||text=="immediate","STATIC_ENUM: resize-policy");
    if(key=="slot-name") {
        if(component=="MediaSlot")require(text=="empty"||text=="preview"||text=="error"||text=="loading","MEDIA_SLOT: unknown slot");
        else require(text=="identity"||text=="actions"||text=="menu","HEADER_SLOT: unknown slot");
    }
    if (const auto values = enums.find(key); values != enums.end() && !values->second.count(text))
        throw Error("STATIC_ENUM: invalid " + key);
    if (key == "align")
        require(text == "start" || text == "center" || text == "end" ||
                    (text == "stretch" && (component == "Column" || component == "Row")),
                "STATIC_ENUM: align");
    if (key == "icon" || key == "symbol")
        require(oneui::ui::namedIcon(text).has_value(), "STATIC_ICON: unknown icon");
    require(utf8(text), "STATIC_UTF8: invalid UTF-8");
}
} // namespace oneview
