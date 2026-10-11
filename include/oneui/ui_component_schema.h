#pragma once
#include <string_view>

// Authoring metadata only: neither a second widget tree nor a layout engine.
namespace oneui::ui::schema {
enum class ValueType { None, Text, Boolean, Integer, Number, Strings, Columns, Rows, TabItems, Series };
#define ONEUI_AUTHORING_COMPONENTS(X) \
    X(ReorderableGrid, "reorderable-grid", "column-count column-gap row-gap item-height reorder-enabled item-drag-enabled", None) \
    X(GridItem, "grid-item", "grid-key", None) \
    X(Overlay, "overlay-host", "overlay-width overlay-height inset-left inset-right inset-top inset-bottom horizontal-align vertical-align overlay-visible", None) \
    X(TemplateTable, "scroll-view", "minimum-width header-height content-height content-width wheel-step scroll-offset horizontal-offset", None) \
    X(Dialog, "dialog", "title subtitle icon close-visible", None) \
    X(PaneGrid, "workspace-collection", "nodes groups active-group active-pane maximized revision ack-request ack-accepted navigation-serial navigation-edge navigation-mode drag-domain gap divider-hit motion-ms resize-idle-ms motion", None) \
    X(Pane, "workspace-item", "pane-key closing close-request min-width min-height", None) \
    X(PaneHeader, "pane-header", "pane-key number title path active broadcast-role broadcast-hint broadcast-label viewing-history can-close can-split can-maximize split-label maximize-label close-label live-label menu-label tooltip", None) \
    X(HeaderSlot, "stack", "slot-name", None) \
    X(FontPicker, "font-picker", "items selected loading error preview-text size trigger-label confirm-selection monospace-only monospace-items apply-label cancel-label filter-label search-label retry-label", None) \
    X(MediaPicker, "media-picker", "asset label loading error accept choose-label clear-label cancel-label", None) \
    X(MediaPreview, "media-preview", "source fit position opacity", None) \
    X(MediaSlot, "stack", "slot-name", None) \
    X(Slider, "slider", "minimum maximum step value", Number) \
    X(Card, "card", "", None) \
    X(InteractiveSurface, "interactive-surface", "", None) \
    X(ThemeScope, "theme-scope", "tokens", None) \
    X(ContainerPage, "container-page", "selected", None) \
    X(DragHandle, "drag-handle", "source domain kind threshold", None) \
    X(DropSurface, "drop-surface", "domain role", None) \
    X(DockWorkspace, "workspace-collection", "nodes root placements revision maximized compact-active ack-request ack-accepted gap divider-hit", None) \
    X(DockItem, "workspace-item", "pane-key dock-min-width dock-min-height float-min-width float-min-height hideable", None) \
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
    X(Tabs, "tabs", "items selectedKey closable presentation reorderable drag-domain drag-kind drag-sources decorations item-min item-max edit-key", Text) \
    X(SplitView, "split-view", "orientation ratio first-min second-min second-collapsed collapsed-extent", Number) \
    X(TimeSeriesChart, "chart", "series", None) \
    X(Icon, "icon", "symbol size", None) \
    X(NativeHost, "native-host", "host themed resize-policy", None) \
    X(Column, "stack", "align justify wrap", None) \
    X(Row, "stack", "align justify wrap", None) \
    X(Content, "stack", "max-width align", None) \
    X(FormRow, "stack", "label hint error", None) \
    X(FormGrid, "stack", "min-column-width", None) \
    X(EmptyState, "stack", "title subtitle", None) \
    X(Scroll, "scroll-view", "content-height content-width wheel-step chrome-visible scroll-offset", None) \
    X(Reveal, "reveal", "open preset reduced-motion", None) \
    X(Text, "label", "text wrap max-lines spans line-height text-align tooltip", None) \
    X(ValidationMessage, "label", "text", None) \
    X(TextArea, "input", "text placeholder line-height read-only", Text) \
    X(Checkbox, "checkbox", "text checked", Boolean) \
    X(Input, "input", "text placeholder password password-mask read-only prefix-icon", Text) \
    X(SearchInput, "input", "text placeholder prefix-icon placeholder-when-focused", Text) \
    X(Switch, "switch", "text checked", Boolean) \
    X(Select, "select", "items selectedIndex", Integer) \
    X(SegmentedControl, "tabs", "items selectedIndex", Integer) \
    X(Button, "button", "text variant icon tooltip content-align trailing-text trailing-icon", None) \
    X(DataTable, "table", "items columns selectedKey item-key", Text) \
    X(List, "list", "items selectedIndex", Integer) \
    X(VirtualList, "virtual-list", "items selectedIndex row-height scroll-offset", Integer) \
    X(StateView, "state-view", "title message action icon", None) \
    X(Menu, "menu", "items", None) \
    X(DataMenu, "menu", "title subtitle items", None) \
    X(Popup, "popup", "open anchor-x anchor-y anchor-width anchor-height content-width content-height placement", None)

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
    if(key=="placeholder-when-focused")return ValueType::Boolean;
    if(kind==Kind::Popup && contains("anchor-x anchor-y anchor-width anchor-height",key))return ValueType::Number;
    if(kind==Kind::ReorderableGrid) {
        if(key=="column-count")return ValueType::Integer;
        if(key=="reorder-enabled"||key=="item-drag-enabled")return ValueType::Boolean;
        if(key=="column-gap"||key=="row-gap"||key=="item-height")return ValueType::Number;
    }
    if(kind==Kind::Dialog&&key=="close-visible")return ValueType::Boolean;
    if(kind==Kind::FontPicker&&key=="selected")return ValueType::Text;
    if(kind==Kind::FontPicker&&(key=="confirm-selection"||key=="monospace-only"))return ValueType::Boolean;
    if(kind==Kind::FontPicker&&key=="monospace-items")return ValueType::Rows;
    if(contains("disabled visible selected checked open reduced-motion detail-open wrap closable themed second-collapsed collapsed closing ack-accepted motion active viewing-history can-close can-split can-maximize reorderable hideable loading password read-only chrome-visible",key))return ValueType::Boolean;
    if(contains("selectedIndex revision ack-request close-request navigation-serial number max-lines",key))return ValueType::Integer;
    if(contains("nodes groups drag-sources placements tokens spans",key))return ValueType::Rows;
    if(contains("gap divider-hit motion-ms resize-idle-ms min-width min-height threshold dock-min-width dock-min-height float-min-width float-min-height minimum maximum step opacity",key))return ValueType::Number;
    if(contains("content-height content-width wheel-step scroll-offset row-height line-height minimum-width header-height horizontal-offset overlay-width overlay-height inset-left inset-right inset-top inset-bottom",key))return ValueType::Number;
    if(key=="overlay-visible")return ValueType::Boolean;
    if(kind==Kind::Tabs && key=="decorations")return ValueType::Rows;
    if(kind==Kind::Tabs && (key=="item-min"||key=="item-max"))return ValueType::Number;
    if(key=="items")return kind==Kind::Select||kind==Kind::SegmentedControl?ValueType::Strings:kind==Kind::Tabs?ValueType::TabItems:ValueType::Rows;
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
