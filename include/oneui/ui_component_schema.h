#pragma once
#include <string_view>

// Authoring metadata only: neither a second widget tree nor a layout engine.
namespace oneui::ui::schema {
enum class ValueType { None, Text, Boolean, Integer, Number, Strings, Columns, Rows, TabItems, Series };
#define ONEUI_AUTHORING_COMPONENTS(X) \
    X(Workspace, "stack", "", None) \
    X(TitleBar, "stack", "", None) \
    X(NavigationRail, "stack", "", None) \
    X(SessionBar, "stack", "", None) \
    X(WorkspaceBody, "stack", "", None) \
    X(StatusBar, "stack", "", None) \
    X(DockPanel, "stack", "collapsed", None) \
    X(PanelHeader, "stack", "", None) \
    X(PanelBody, "stack", "", None) \
    X(PanelFooter, "stack", "", None) \
    X(Spacer, "stack", "", None) \
    X(ToolButton, "button", "text icon variant", None) \
    X(Progress, "progress", "value", None) \
    X(Surface, "stack", "appearance", None) \
    X(Sidebar, "stack", "", None) \
    X(SidebarLayout, "panes", "breakpoint pane-width", None) \
    X(MasterDetail, "panes", "breakpoint pane-width detail-open", None) \
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
    X(Tabs, "tabs", "items selectedKey closable presentation", Text) \
    X(SplitView, "split-view", "orientation ratio first-min second-min second-collapsed collapsed-extent", Number) \
    X(TimeSeriesChart, "chart", "series", None) \
    X(Icon, "icon", "symbol size", None) \
    X(NativeHost, "native-host", "host themed", None) \
    X(Column, "stack", "align justify wrap", None) \
    X(Row, "stack", "align justify wrap", None) \
    X(Content, "stack", "max-width align", None) \
    X(FormRow, "stack", "label hint error", None) \
    X(FormGrid, "stack", "min-column-width", None) \
    X(EmptyState, "stack", "title subtitle", None) \
    X(Scroll, "scroll-view", "", None) \
    X(Reveal, "reveal", "open preset reduced-motion", None) \
    X(Text, "label", "text", None) \
    X(ValidationMessage, "label", "text", None) \
    X(Input, "input", "text placeholder", Text) \
    X(SearchInput, "input", "text placeholder", Text) \
    X(Switch, "switch", "text checked", Boolean) \
    X(Select, "select", "items selectedIndex", Integer) \
    X(Button, "button", "text variant icon", None) \
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
    return contains("grow shrink basis min max max-width min-column-width breakpoint pane-width ratio first-min second-min size collapsed-extent value",key);
}
constexpr ValueType propertyType(Kind kind,std::string_view key) {
    if(!supports(kind,key))return ValueType::None;
    if(contains("disabled visible checked open reduced-motion detail-open wrap closable themed second-collapsed collapsed",key))return ValueType::Boolean;
    if(key=="selectedIndex")return ValueType::Integer;
    if(key=="items")return kind==Kind::Select?ValueType::Strings:kind==Kind::Tabs?ValueType::TabItems:ValueType::Rows;
    if(key=="series")return ValueType::Series;
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
    case ValueType::TabItems:return "std::vector<oneui::ui::TabItem>";
    case ValueType::Series:return "std::vector<oneui::TimeSeriesChartSeries>";
    default:return "void";
    }
}
} // namespace oneui::ui::schema
