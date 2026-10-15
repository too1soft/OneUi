#include "component_contract.h"
#include "legacy_component_contract.h"
#include "oneui/ui_component_schema.h"
#include <sstream>

namespace oneview
{
const ComponentProperty &ComponentContract::property(const std::string &n) const
{
    for (auto &p : properties)
        if (p.name == n)
            return p;
    throw Error("COMPONENT_PROPERTY: unknown property " + name + "." + n);
}
const ComponentProperty &ComponentContract::property(uint32_t id) const
{
    for (auto &p : properties)
        if (p.id == id)
            return p;
    throw Error("BAD_IR: component property ID");
}
const ComponentEvent &ComponentContract::event(const std::string &n) const
{
    for (auto &e : events)
        if (e.name == n)
            return e;
    throw Error("COMPONENT_EVENT: unknown event " + name + "." + n);
}
const ComponentEvent &ComponentContract::event(uint32_t id) const
{
    for (auto &e : events)
        if (e.id == id)
            return e;
    throw Error("BAD_IR: component event ID");
}
const std::map<std::string, ComponentContract> &registry()
{
    static const auto value = [] {
        std::map<std::string, ComponentContract> r;
        // IDs and complete v1 contracts are frozen data, not native enum ordinals.
#define C(name, rule, model) r.emplace(name, ComponentContract{name, rule, 1, model});
#define P(component, id, name, tag, flags, record) r.at(component).properties.push_back({id, tag, flags, name, record});
#define E(component, id, name, key)                                                                                    \
    r.at(component).events.push_back({id, name, key ? std::vector<Argument>{{"key", 4}} : std::vector<Argument>{}});
#include "component_registry.inc"
#include "component_registry_panes.inc"
#include "component_registry_dock.inc"
#undef E
#undef P
#undef C
        r.at("TimeSeriesChart").events = {{1,"inspection",{{"index",2},{"pinned",1}}}};
        ++r.at("TimeSeriesChart").version;
        r.at("Table").events = {
            {1,"activate",{{"key",4}}}, {2,"edit",{{"key",4}}},
            {3,"delete",{}}, {4,"selection",{}}, {5,"current",{{"key",4}}},
            {6,"sort",{{"column",2}}}, {7,"cell",{{"key",4},{"column",2}}},
            {8,"context",{{"key",4},{"x",3},{"y",3}}},
        };
        for (const auto *name : {"Input", "Text", "Scroll", "FontPicker", "MediaPicker"}) r.at(name).version = 2;
        ++r.at("Text").version;
        ++r.at("Input").version; // Explicit password mask, appended property ID 15.
        ++r.at("Input").version; // Explicit accessible-name contract, property ID 17.
        ++r.at("Input").version; // Placeholder visible while focused, property ID 18.
        ++r.at("Checkbox").version; // Tooltip property ID 13.
        ++r.at("DockWorkspace").version; // Caption and raise-on-press properties IDs 21-24.
        ++r.at("DockWorkspace").version; // Drop policy properties IDs 25-27.
        ++r.at("Scroll").version; // Explicit content width, appended property ID 15.
        ++r.at("Scroll").version; // Scrollbar inset, appended property ID 16.
        ++r.at("VirtualList").version; // Unified trailing metrics/click contract, IDs 16-18.
        ++r.at("Scroll").version; // Viewport size event, no native layout change.
        ++r.at("TimeSeriesChart").version; // Fill extent, property ID 26; optional series fill/dashed.
        ++r.at("TimeSeriesChart").version; // Live scroll rate, property ID 27.
        r.at("Scroll").events.push_back({1,"viewport-size-changed",{{"width",3},{"height",3}}});
        ++r.at("SearchInput").version; // Optional prefix icon; absent keeps old rendering.
        ++r.at("VirtualList").version; // Optional row selection and typed multi-selection events.
        ++r.at("FontPicker").version;
        // Read-only class bindings preserve static and scoped classes. Keep the
        // reviewed extension limited to the presentation nodes that need it.
        for (const auto *name : {"Surface", "Text", "Column", "Row"}) {
            auto &contract = r.at(name);
            ++contract.version;
            for (auto &property : contract.properties)
                if (property.name == "class") property.flags |= 2;
        }
        for (const auto *name : {"Surface", "Column"}) {
            auto &contract = r.at(name);
            ++contract.version;
            for (auto &property : contract.properties)
                if (property.name == "basis") property.flags |= 2;
        }
        ++r.at("Text").version; // Explicit line boxes, alignment and tooltip.
        ++r.at("Button").version; // Tooltip and content alignment.
        // Preserve frozen registry rows while separating the two menu contracts.
        ++r.at("Menu").version;
        for (auto &property : r.at("Menu").properties)
            if (property.name == "items") property.record = "@native-menu.item";
        ++r.at("Table").version; // Native table tooltip.
        // Detect authoring metadata drift before publishing a contract hash.
        // Intentional changes require a reviewed registry/version update.
        using namespace oneui::ui::schema;
        require(r.size() == sizeof(components) / sizeof(components[0]), "REGISTRY: component set drift");
        for (const auto &spec : components)
        {
            auto found = r.find(spec.name);
            require(found != r.end(), "REGISTRY: missing authoring component");
            auto &contract = found->second;
            std::istringstream names(std::string("class grow basis min max shrink visible disabled name description ") +
                                     std::string(spec.properties));
            std::string name;
            size_t count = 0;
            while (names >> name)
            {
                contract.property(name);
                ++count;
            }
            require(count == contract.properties.size(), "REGISTRY: property set drift for " + contract.name);
            uint32_t previous = 0;
            for (const auto &p : contract.properties)
            {
                require(p.id > previous, "REGISTRY: duplicate or unsorted property IDs");
                previous = p.id;
                auto type = propertyType(spec.kind, p.name);
                uint32_t expected = type == ValueType::Boolean   ? 1
                                    : type == ValueType::Integer ? 2
                                    : type == ValueType::Number  ? 3
                                    : type == ValueType::Text    ? 4
                                                                 : 5;
                require(type != ValueType::None && p.tag == expected, "REGISTRY: property type drift");
            }
            if (spec.model != ValueType::None)
                require(contract.modelProperty && (contract.property(contract.modelProperty).flags & 4),
                        "REGISTRY: missing model property");
        }
        return r;
    }();
    return value;
}
void validateRecord(const Model& model, const std::string& projection) {
    if (projection.empty() || projection[0]!='@') {
        require(model.name==projection,"BIND_RECORD: component requires "+projection);
        return;
    }
    std::string fields;
    if(projection=="@list.item") fields="id:4 title:4 detail:4";
    else if(projection=="@native-menu.item") fields="id:4 kind:4 title:4 detail:4 icon:4 danger:1 disabled:1";
    else if(projection=="@tree.item") fields="id:4 parent:4 title:4 detail:4 expanded:1";
    else if(projection=="@chart.series") fields="id:4 name:4 rgba:2 values:5";
    else if(projection=="@chart.point") fields="id:4 value:3";
    else if(projection=="@table.row") fields="id:4 cells:5 selected:1";
    else if(projection=="@table.cell") fields="id:4 text:4";
    else if(projection=="@text.span") fields="id:4 start:2 end:2 rgba:2 monospace:1";
    else if(projection=="@workspace.node") fields="id:4 kind:4 axis:4 ratio:3 first:4 second:4 pane:4";
    else if(projection=="@workspace.placement") fields="id:4 mode:4 x:3 y:3 width:3 height:3 pinned:1 order:2";
    else if(projection=="@picker.font") fields="id:4 family:4 label:4 available:1";
    else if(projection=="@picker.color") fields="id:4 rgba:2";
    else if(projection=="@menu.item") fields="id:4 parent:4 text:4 icon:4 shortcut:4 disabled:1 checked:1 danger:1 separator:1";
    else if(projection=="@workspace.group") fields="id:4 root:4";
    else if(projection=="@workspace.tab-source") fields="id:4 pane:4 enabled:1";
    else if(projection=="@workspace.tab-decoration") fields="id:4 icon:2 rgba:2";
    else throw Error("BIND_RECORD: unknown projection "+projection);
    require(model.keyName=="id","BIND_RECORD: workspace key must be id");
    std::istringstream stream(fields);std::string field;
    while(stream>>field) {
        auto separator=field.find(':');auto name=field.substr(0,separator);
        auto found=model.properties.find(name);
        require(found!=model.properties.end() && found->second.tag==uint32_t(field.back()-'0'),
                "BIND_RECORD: missing or incompatible "+name);
    }
    if(projection=="@tree.item")
        require((model.properties.at("expanded").access & 2)!=0,
                "BIND_RECORD: TreeView expanded must be writable");
    if(projection=="@table.row")
        require((model.properties.at("selected").access & 2)!=0,
                "BIND_RECORD: Table selected must be writable");
}
void validateRecord(const Schema& schema,const Model& model,const std::string& projection) {
    validateRecord(model,projection);
    if(projection=="@chart.series") {
        for(const char* name:{"fill","dashed"}) {
            const auto found=model.properties.find(name);
            require(found==model.properties.end() || found->second.tag==1,
                    std::string("BIND_RECORD: Chart series ")+name+" must be bool");
        }
        const auto& point=schema.model(model.properties.at("values").item);
        validateRecord(point,"@chart.point");
        auto valid=point.properties.find("valid");
        require(valid==point.properties.end() || valid->second.tag==1,"BIND_RECORD: Chart valid must be bool");
        return;
    }
    if(projection!="@table.row")return;
    const auto& cell=schema.model(model.properties.at("cells").item);
    validateRecord(cell,"@table.cell");
    const std::map<std::string,uint32_t> optional={
        {"detail",4},{"icon",4},{"trailing_icon",4},{"foreground",2},{"indicator",2},
        {"alignment",2},{"weight",2},{"font_size",3},{"icon_size",3},{"badge",4},
        {"badge_foreground",2},{"badge_background",2}};
    for(const auto& [name,tag]:optional) {
        const auto found=cell.properties.find(name);
        require(found==cell.properties.end() || found->second.tag==tag,
                "BIND_RECORD: incompatible Table cell "+name);
    }
}
Bytes encodeComponents(const std::vector<ComponentContract> &components)
{
    Bytes b{'O', 'V', 'C', 'O', 'M', 'P', '1', 0};
    put(b, components.size());
    for (auto &c : components)
    {
        string(b, c.name);
        words(b, {c.version, uint32_t(c.properties.size()), uint32_t(c.events.size())});
        for (auto &p : c.properties)
        {
            put(b, p.id);
            string(b, p.name);
            words(b, {p.tag, p.flags});
            string(b, p.record);
        }
        for (auto &e : c.events)
        {
            put(b, e.id);
            string(b, e.name);
            put(b, e.args.size());
            for (auto &a : e.args)
            {
                string(b, a.name);
                put(b, a.tag);
            }
        }
        string(b, c.childRule);
        put(b, c.modelProperty);
    }
    return b;
}
std::vector<ComponentContract> decodeComponents(const Bytes &b)
{
    Cursor c(b);
    require(c.get(8) == 0x0031504d4f43564full, "BAD_IR: COMP magic");
    auto n = c.u32();
    require(n <= registry().size(), "BAD_IR: component count");
    std::vector<ComponentContract> result;
    for (uint32_t i = 0; i < n; ++i)
    {
        auto name = c.str();
        auto known = registry().find(name);
        require(known != registry().end() && (result.empty() || result.back().name < name),
                "BAD_IR: unknown/unsorted component");
        result.push_back(decodeComponentContract(c, known->second));
    }
    c.done();
    return result;
}
void addBuiltins(Schema &schema)
{
    auto model = [&](const std::string &name, const std::string &fields) {
        Model m;
        m.name = "oneui.v1." + name;
        m.keyName = "id";
        m.location = {"<builtin>", 1, 1};
        std::istringstream in(fields);
        std::string field;
        while (in >> field)
        {
            auto at = field.find(':');
            Property p;
            p.name = field.substr(0, at);
            auto type = field.substr(at + 1);
            p.location = m.location;
            p.tag = type == "utf8" ? 4 : type == "f64" ? 3 : type == "i64" ? 2 : type == "bool" ? 1 : 5;
            if (p.tag == 5)
                p.itemName = "oneui.v1." + type;
            m.properties.emplace(p.name, p);
        }
        require(schema.models.emplace(m.name, m).second, "SCHEMA_DUPLICATE: reserved builtin " + m.name);
    };
    model("StringItem", "id:utf8 value:utf8");
    model("NumberItem", "id:utf8 value:f64");
    model("TabItem", "id:utf8 title:utf8"); // Decorations use a separate optional keyed projection.
    model("Row", "id:utf8 cells:StringItem");
    model("Column", "id:utf8 header:utf8 width:f64 alignment:i64 action:bool kind:i64");
    model("Series", "id:utf8 name:utf8 rgba:i64 values:NumberItem");
}
} // namespace oneview
