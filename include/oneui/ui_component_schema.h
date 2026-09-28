#pragma once
#include <string_view>

// Authoring metadata only: neither a second widget tree nor a layout engine.
namespace oneui::ui::schema {
enum class ValueType { None, Text, Boolean, Integer, Number, Strings, Columns, Rows };
#define ONEUI_AUTHORING_COMPONENTS(X) \
    X(Page, "stack", "title", None) \
    X(Header, "stack", "title", None) \
    X(Toolbar, "stack", "", None) \
    X(Section, "stack", "title", None) \
    X(SettingsPage, "stack", "title subtitle", None) \
    X(ListPage, "stack", "title subtitle", None) \
    X(DetailPage, "stack", "title subtitle", None) \
    X(ActionBar, "stack", "", None) \
    X(LoadingState, "stack", "title subtitle", None) \
    X(Status, "label", "text tone", None) \
    X(Column, "stack", "", None) \
    X(Row, "stack", "", None) \
    X(Content, "stack", "max-width align", None) \
    X(FormRow, "stack", "label hint error", None) \
    X(FormGrid, "stack", "min-column-width", None) \
    X(EmptyState, "stack", "title subtitle", None) \
    X(Scroll, "scroll-view", "", None) \
    X(Text, "label", "text", None) \
    X(ValidationMessage, "label", "text", None) \
    X(Input, "input", "text placeholder", Text) \
    X(SearchInput, "input", "text placeholder", Text) \
    X(Switch, "switch", "text checked", Boolean) \
    X(Select, "select", "items selectedIndex", Integer) \
    X(Button, "button", "text variant", None) \
    X(DataTable, "table", "items columns selectedKey item-key", Text)

enum class Kind {
#define ONEUI_KIND(name, tag, props, model) name,
    ONEUI_AUTHORING_COMPONENTS(ONEUI_KIND)
#undef ONEUI_KIND
};
struct Component {
    Kind kind;
    const char* name;
    const char* tag;
    std::string_view properties;
    ValueType model;
};
inline constexpr Component components[] = {
#define ONEUI_SPEC(name, tag, props, model) {Kind::name, #name, tag, props, ValueType::model},
    ONEUI_AUTHORING_COMPONENTS(ONEUI_SPEC)
#undef ONEUI_SPEC
};
#undef ONEUI_AUTHORING_COMPONENTS
constexpr const Component& component(Kind kind) { return components[static_cast<unsigned>(kind)]; }
constexpr const Component* component(std::string_view name) {
    for (const auto& spec:components) if (name==spec.name) return &spec;
    return nullptr;
}
constexpr bool contains(std::string_view words,std::string_view key) {
    std::size_t at=0;
    while(at<words.size()) {
        const auto end=words.find(' ',at);
        if(words.substr(at,end==words.npos?end:end-at)==key)return true;
        if(end==words.npos)break;
        at=end+1;
    }
    return false;
}
constexpr bool supports(Kind kind,std::string_view key) {
    return contains("class grow basis min max shrink visible disabled name description",key) || contains(component(kind).properties,key);
}
constexpr bool layoutNumber(std::string_view key) {
    return contains("grow shrink basis min max max-width min-column-width",key);
}
constexpr ValueType propertyType(Kind kind,std::string_view key) {
    if(!supports(kind,key))return ValueType::None;
    if(contains("disabled visible checked",key))return ValueType::Boolean;
    if(key=="selectedIndex")return ValueType::Integer;
    if(key=="items")return kind==Kind::Select?ValueType::Strings:ValueType::Rows;
    if(key=="columns")return ValueType::Columns;
    if(layoutNumber(key))return ValueType::Number;
    return ValueType::Text;
}
constexpr const char* cppType(ValueType type) {
    switch(type) {
    case ValueType::Text:return "std::wstring";
    case ValueType::Boolean:return "bool";
    case ValueType::Integer:return "int";
    case ValueType::Number:return "float";
    case ValueType::Strings:return "std::vector<std::wstring>";
    case ValueType::Columns:return "std::vector<oneui::TableColumn>";
    case ValueType::Rows:return "std::vector<oneui::ui::TableRow>";
    default:return "void";
    }
}
} // namespace oneui::ui::schema
