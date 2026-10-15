#include "reload_plan.h"
#include "tabs_presentation.h"
#include "oneui/layout/pane_grid.h"
#include "oneui/layout/dock_workspace_view.h"
namespace oneui::views {
std::map<std::string,Value> authored(Runtime &runtime,const Instance &node) {
    std::map<std::string,Value> result;
    for(auto owner:node.owners) {
        const auto root=oneview::componentRoot(runtime.image,owner);
        const auto &contract=runtime.image.components[runtime.image.records[oneview::NODE][root][1]];
        for(auto &r:runtime.image.records[oneview::PROP])if(r[0]==owner)
            result[contract.property(r[1]).name]=runtime.constant(r[2]);
        for(auto &r:runtime.image.records[oneview::BIND])if(r[0]==owner)
            result[contract.property(r[1]).name]={};
    }
    return result;
}
std::function<void()> resetProperty(ui::Mount &factory,ui::Element &target,
                                    const ui::Element &fresh,const std::string &key) {
    if(target.component=="FontPicker"||target.component=="MediaPicker"||target.component=="MediaPreview"||target.component=="Slider"||target.component=="ThemeScope") {
        auto spec=ui::schema::component(target.component);if(spec&&ui::schema::contains(spec->properties,key))return []{};
    }
    if(target.component=="DockWorkspace" && ui::schema::contains(ui::schema::component(ui::schema::Kind::DockWorkspace).properties,key))return []{};
    if(target.component=="DockItem" && ui::schema::contains(ui::schema::component(ui::schema::Kind::DockItem).properties,key))
        return [w=target.as<DockItemView>(),n=fresh.as<DockItemView>()]{w->key=n->key;w->dockMinimum=n->dockMinimum;w->floatMinimum=n->floatMinimum;w->hideable=n->hideable;};
    if(target.component=="PaneGrid" && ui::schema::contains(ui::schema::component(ui::schema::Kind::PaneGrid).properties,key))return []{};
    if(target.component=="Pane" && ui::schema::contains(ui::schema::component(ui::schema::Kind::Pane).properties,key))
        return [w=target.as<PaneView>(),n=fresh.as<PaneView>()]{w->key=n->key;w->minimum=n->minimum;w->closing=n->closing;w->closeRequest=n->closeRequest;};
    if(target.component=="NativeHost" && key=="resize-policy")
        return [w=target.as<ui::NativeHostView>(),n=fresh.as<ui::NativeHostView>()]{w->setSettledViewport(n->settledViewport());};
    if(target.component=="PaneHeader" && ui::schema::contains(ui::schema::component(ui::schema::Kind::PaneHeader).properties,key))
        return [w=target.as<PaneHeader>(),n=fresh.as<PaneHeader>()]{w->setConfig(n->config());};
    if(target.component=="Tabs" && (key=="drag-domain"||key=="drag-kind"||key=="drag-sources"))return []{};
    if(target.component=="Tabs" && key=="reorderable")return [w=target.as<Tabs>()]{w->setReorderEnabled(false);};
    if(target.component=="Tabs" && key=="sizing")return [&target] {
        auto* state=ui::detail::tabsPresentation(target.tabs);
        check(state!=nullptr,ONEUI_VIEW_TYPE,"Tabs sizing requires Mount::make Tabs");
        state->sizing.reset();
        target.as<Tabs>()->setSizingMode(state->document ? TabsSizingMode::Compact : TabsSizingMode::Equal);
    };
    if(target.component=="ContainerPage"&&key=="selected")return [w=target.as<ContainerPage>()]{w->setSelected(false);};
    if(key=="icon")return [w=target.as<Button>()]{w->clearIcon();};
    if(key=="symbol")return [w=target.as<IconView>()]{w->setSymbol(IconSymbol::RemoteAssist);};
    if(key=="grow" || key=="shrink" || key=="basis" || key=="min" || key=="max")return []{};
    if(key=="items") {
        if(target.component=="Select")return [w=target.as<Select>()]{w->setItems({});};
        if(target.component=="SegmentedControl")return [w=target.as<Tabs>()]{w->setItems({});};
        if(target.component=="List")return [w=target.as<List>()]{w->setItems({});};
        if(target.component=="VirtualList")return [w=target.as<VirtualList>()]{w->setItems({});};
        if(target.table)return [w=target.table]{w->update({});};
        if(target.tabs)return [w=target.tabs]{w->update({});};
    }
    if(key=="columns")return [w=target.as<Table>()]{w->setColumns({});};
    if(target.component=="TimeSeriesChart") {
        if(key=="sample-positions")return [w=target.as<TimeSeriesChart>()]{w->setSamplePositions({});};
        if(key=="axis-labels")return [w=target.as<TimeSeriesChart>()]{w->setAxisLabels({});};
        if(key=="grid-style")return [w=target.as<TimeSeriesChart>()]{w->setGridStyle(5,1);};
        if(key=="plot-insets")return [w=target.as<TimeSeriesChart>()]{w->setPlotInsets({4,2,2,2});};
        if(key=="fill-extent")return [w=target.as<TimeSeriesChart>()]{w->setFillExtent(TimeSeriesFillExtent::Plot);};
    }
    if(key=="series")return [w=target.as<TimeSeriesChart>()]{w->setSeries({});};
    if(key=="size" && target.component=="Icon")
        return [w=target.as<IconView>(),size=fresh.widget->preferredSize(),glyph=fresh.as<IconView>()->glyphSize()]{
            w->setPreferredSize(size);w->setGlyphSize(glyph);};
    if(key=="size")return [w=target.widget,size=fresh.widget->preferredSize()]{w->setPreferredSize(size);};
    Value value;
    const auto &property=oneview::registry().at(target.component).property(key);
    value.tag=property.tag;
    if(value.tag==1) {
        value.integer=key=="visible" ? fresh.widget->visible() : 0;
        if(key=="wrap")value.integer=target.component=="Row" || target.component=="FormRow" ||
            target.component=="FormGrid" || target.component=="Toolbar" || target.component=="ActionBar";
        if(key=="open")value.integer=fresh.as<Reveal>()->open();
    }
    if(target.component=="TimeSeriesChart") {
        if(key=="fill-alpha")value.integer=32;
        if(key=="grid-lines")value.integer=4;
        if(key=="axes-visible" || key=="smooth" || key=="area-fill" || key=="dashed-grid")value.integer=1;
    }
    if(value.tag==2 && key=="selectedIndex") {
        if(target.component=="List")value.integer=fresh.as<List>()->selectedIndex();
        else if(target.component=="VirtualList")value.integer=fresh.as<VirtualList>()->selectedIndex();
        else if(target.component=="SegmentedControl")value.integer=fresh.as<Tabs>()->selectedIndex();
        else value.integer=fresh.as<Select>()->selectedIndex();
    }
    if(value.tag==3) {
        const std::map<std::string,double> numbers={{"ratio",0.5},{"first-min",80},{"second-min",80},
            {"collapsed-extent",0},{"min-column-width",320},{"max-width",880},
            {"breakpoint",target.component=="SidebarLayout" ? 960 : 800},
            {"pane-width",target.component=="SidebarLayout" ? 208 : 380},{"value",0},
            {"range-min",0},{"range-max",1},{"line-width",2.25},{"row-height",42},{"content-height",0},{"wheel-step",48},{"scroll-offset",0},{"scrollbar-inset",5},{"trailing-width",58},{"trailing-gap",8}};
        auto found=numbers.find(key);
        check(found!=numbers.end(),ONEUI_VIEW_TYPE,"Missing reload numeric default");
        value.number=found->second;
    }
    if(value.tag==4) {
        const std::map<std::string,std::string> defaults={{"orientation","horizontal"},
            {"presentation","document"},{"align",target.component=="Content" ? "start" : "stretch"},
            {"justify","start"},{"variant","normal"},{"appearance","flat"},{"tone","neutral"},
            {"preset","expand"},{"symbol","Terminal"}};
        if(defaults.count(key))value.text=defaults.at(key);
    }
    return [&factory,&target,key,value]{setScalar(factory,target,key,value);};
}
}
