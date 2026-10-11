#include "runtime.h"
#include "rich_table_events.inc"
#include "oneui/controls/popup.h"
#include "data_menu.h"
namespace oneui::views {
void Runtime::wire(const std::shared_ptr<Instance> &instance) {
    if (image.records[oneview::NODE][instance->node][0] != 1)
        return;
    instance->owner = weak_from_this();
    wireWorkspace(instance);
    wirePicker(instance);
    if(instance->element.component=="Table") {wireRichTable(instance);return;}
    auto node = std::weak_ptr<Instance>(instance);
    auto mutate = [node](uint32_t id, Value value) {
        auto item = node.lock();
        auto runtime = item ? item->owner.lock() : nullptr;
        if (!runtime || !item || runtime->closing || item->applying)
            return;
        try {
            for (const auto &b : item->bindings)
                if (b.kind == oneview::BIND && b.record[1] == id && (b.record[3] >> 16) == 2)
                    if (auto scope = b.scope.lock())
                        runtime->set(scope, b.record[2], value);
        } catch (const Failure &e) {
            runtime->fail(e.result, e.what());
        } catch (...) {
            runtime->fail(ONEUI_VIEW_FOREIGN_FAILURE, "Exception in model callback");
        }
    };
    auto &element = instance->element;
    if(auto tree=element.as<TreeView>()) {
        tree->setOnExpansionChanged([node](const std::wstring& key,bool expanded) {
            auto item=node.lock();auto runtime=item?item->owner.lock():nullptr;
            if(!runtime || runtime->closing || item->applying)return;
            try {
                // Resolve against the committed list lease, never against a new
                // worker snapshot while the previous rows are still on screen.
                for(const auto& b:item->bindings) {
                    if(b.kind!=oneview::BIND || b.record[1]!=6)continue;
                    auto scope=b.scope.lock();if(!scope)continue;
                    auto found=scope->lists.find(b.record[2]);
                    if(found==scope->lists.end())continue;
                    auto& list=found->second;
                    auto at=std::find(list.keys.begin(),list.keys.end(),narrow(key));
                    if(at==list.keys.end())continue;
                    auto row=list.items[size_t(at-list.keys.begin())];
                    auto property=runtime->image.schema.model(row->table.model_id).properties.at("expanded").id;
                    Value value;value.tag=1;value.integer=expanded;
                    runtime->set(row,property,value);
                }
            } catch(const Failure& error) {runtime->fail(error.result,error.what());}
              catch(...) {runtime->fail(ONEUI_VIEW_FOREIGN_FAILURE,"Exception in tree expansion callback");}
        });
    }
    auto rootNode = oneview::componentRoot(image, instance->node);
    const auto &contract = image.components[image.records[oneview::NODE][rootNode][1]];
    if (contract.modelProperty) {
        auto id = contract.modelProperty;
        if (element.component == "Input" || element.component == "SearchInput" || element.component == "TextArea")
            element.as<TextField>()->setOnChanged([mutate, id](const std::wstring &text) {
                Value v;
                v.tag = 4;
                v.text = narrow(text);
                mutate(id, v);
            });
        else if (element.component == "Slider")
            element.as<Slider>()->setOnChanged([mutate,id](double value){Value v;v.tag=3;v.number=value;mutate(id,v);});
        else if (element.component == "Checkbox")
            element.as<Checkbox>()->setOnChanged([mutate, id](bool value) {
                Value v; v.tag = 1; v.integer = value; mutate(id, v);
            });
        else if (element.component == "Switch")
            element.as<Switch>()->setOnChanged([mutate, id](bool value) {
                Value v;
                v.tag = 1;
                v.integer = value;
                mutate(id, v);
            });
        else if (element.component == "List")
            element.as<List>()->setOnChanged([mutate, id](int value) {
                Value v; v.tag = 2; v.integer = value; mutate(id, v);
            });
        else if (element.component == "VirtualList")
            element.as<VirtualList>()->setOnChanged([mutate, id](int value) {
                Value v; v.tag = 2; v.integer = value; mutate(id, v);
            });
        else if (element.component == "SegmentedControl")
            element.as<Tabs>()->setOnChanged([mutate, id](int value) {
                Value v; v.tag = 2; v.integer = value; mutate(id, v);
            });
        else if (element.component == "Select")
            element.as<Select>()->setOnChanged([mutate, id](int value) {
                Value v;
                v.tag = 2;
                v.integer = value;
                mutate(id, v);
            });
        else if (element.component == "SplitView")
            element.as<SplitView>()->setOnSplitRatioChanged([mutate, id](float value) {
                Value v;
                v.tag = 3;
                v.number = value;
                mutate(id, v);
            });
        else {
            auto selection = [mutate, id](std::wstring key) {
                Value v;
                v.tag = 4;
                v.text = narrow(key);
                mutate(id, v);
            };
            if (element.tabs)
                element.tabs->selected = selection;
            if (element.table)
                element.table->selected = selection;
            if(auto tree=element.as<TreeView>())
                tree->setOnSelectionChanged(selection);
        }
    }
    for (const auto &event : contract.events) {
        auto name = event.name;
        auto fire = [node, id=event.id](std::vector<Value> args) {
            auto item = node.lock();
            auto runtime = item ? item->owner.lock() : nullptr;
            if (!runtime || !item || runtime->closing || item->applying)
                return;
            const auto found=std::find_if(item->events.begin(),item->events.end(),
                [&](const auto &e) { return e[1]==id; });
            if(found==item->events.end())return;
            auto event=*found;
            try {
                runtime->invoke(item, event, std::move(args));
            } catch (const Failure &e) {
                runtime->fail(e.result, e.what(), event[7]);
            } catch (...) {
                runtime->fail(ONEUI_VIEW_FOREIGN_FAILURE, "Exception in command callback",
                              event[7]);
            }
        };
        if(name=="menuActivated" && element.component=="DataMenu")
            element.as<DataMenu>()->setOnActivated([fire](std::string id) {
                Value value; value.tag=4; value.text=std::move(id); fire({value});
            });
        else if(name=="reorderRequested" && element.component=="ReorderableGrid")
            element.as<ReorderableGrid>()->setOnReorderRequested([fire](const std::wstring& source,int target) {
                Value key,index;key.tag=4;key.text=narrow(source);index.tag=2;index.integer=target;
                fire({key,index});
            });
        else if(name=="itemDrag" && element.component=="ReorderableGrid")
            element.as<ReorderableGrid>()->setOnItemDrag([fire](const ItemDragEvent& event) {
                Value key,phase,x,y;key.tag=4;key.text=narrow(event.sourceId);
                phase.tag=2;phase.integer=static_cast<int>(event.phase);
                x.tag=y.tag=3;x.number=event.position.x;y.number=event.position.y;
                fire({key,phase,x,y});
            });
        else if (name == "click") {
            if(auto surface=element.as<InteractiveSurface>())surface->setOnClick([fire]{fire({});});
            else element.as<Button>()->setOnClick([fire] { fire({}); });
        }
        else if (name == "pointer" && std::any_of(instance->events.begin(),instance->events.end(),
                 [&](const auto& binding){return binding[1]==event.id;}))
            element.as<InteractiveSurface>()->setOnPointerActivated([fire](const MouseEvent& event) {
                Value x,y,control,shift,count;
                x.tag=y.tag=3; x.number=event.position.x; y.number=event.position.y;
                control.tag=shift.tag=1;control.integer=event.control;shift.integer=event.shift;
                count.tag=2;count.integer=event.clickCount;
                fire({x,y,control,shift,count});
            });
        else if (name == "contextMenu" && std::any_of(instance->events.begin(),instance->events.end(),
                 [&](const auto& binding){return binding[1]==event.id;}))
            element.as<InteractiveSurface>()->setOnContextMenuRequested([fire](const MouseEvent& event) {
                Value x,y;x.tag=y.tag=3;x.number=event.position.x;y.number=event.position.y;
                fire({x,y});
            });
        else if (name == "submit")
            element.as<TextField>()->setOnSubmitted([fire](const std::wstring &) { fire({}); });
        else if (name == "closed" && element.component == "Popup")
            element.as<Popup>()->setOnClosed([fire] { fire({}); });
        else if (name == "close" && element.component == "Dialog")
            element.as<Dialog>()->setOnClose([fire] { fire({}); });
        else if (name == "close")
            element.tabs->closed = [fire](std::wstring key) {
                Value v;
                v.tag = 4;
                v.text = narrow(key);
                fire({v});
            };
        else if (name == "activate" && element.component == "VirtualList")
            element.as<VirtualList>()->setOnActivated([fire](int) { fire({}); });
        else if (name == "activate")
            element.as<Table>()->setOnActivated([fire](int) { fire({}); });
        else if (name == "delete")
            element.as<Table>()->setOnDeleteRequested([fire](const auto &) { fire({}); });
    }
}
} // namespace oneui::views
