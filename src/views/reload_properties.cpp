#include "reload_plan.h"
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
    if(key=="icon")return [w=target.as<Button>()]{w->clearIcon();};
    if(key=="symbol")return [w=target.as<IconView>()]{w->setSymbol(IconSymbol::RemoteAssist);};
    if(key=="grow" || key=="shrink" || key=="basis" || key=="min" || key=="max")return []{};
    if(key=="items") {
        if(target.component=="Select")return [w=target.as<Select>()]{w->setItems({});};
        if(target.table)return [w=target.table]{w->update({});};
        if(target.tabs)return [w=target.tabs]{w->update({});};
    }
    if(key=="columns")return [w=target.as<Table>()]{w->setColumns({});};
    if(key=="series")return [w=target.as<TimeSeriesChart>()]{w->setSeries({});};
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
    if(value.tag==2)value.integer=key=="selectedIndex" ? fresh.as<Select>()->selectedIndex() : 0;
    if(value.tag==3) {
        const std::map<std::string,double> numbers={{"ratio",0.5},{"first-min",80},{"second-min",80},
            {"collapsed-extent",0},{"min-column-width",320},{"max-width",880},
            {"breakpoint",target.component=="SidebarLayout" ? 960 : 800},
            {"pane-width",target.component=="SidebarLayout" ? 208 : 380},{"value",0}};
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
