#include "runtime.h"
namespace oneui::views {
void Runtime::wire(const std::shared_ptr<Instance> &instance) {
    if (image.records[oneview::NODE][instance->node][0] != 1)
        return;
    instance->owner = weak_from_this();
    wireWorkspace(instance);
    wirePicker(instance);
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
    auto rootNode = oneview::componentRoot(image, instance->node);
    const auto &contract = image.components[image.records[oneview::NODE][rootNode][1]];
    if (contract.modelProperty) {
        auto id = contract.modelProperty;
        if (element.component == "Input" || element.component == "SearchInput")
            element.as<TextField>()->setOnChanged([mutate, id](const std::wstring &text) {
                Value v;
                v.tag = 4;
                v.text = narrow(text);
                mutate(id, v);
            });
        else if (element.component == "Slider")
            element.as<Slider>()->setOnChanged([mutate,id](double value){Value v;v.tag=3;v.number=value;mutate(id,v);});
        else if (element.component == "Switch")
            element.as<Switch>()->setOnChanged([mutate, id](bool value) {
                Value v;
                v.tag = 1;
                v.integer = value;
                mutate(id, v);
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
        if (name == "click")
            element.as<Button>()->setOnClick([fire] { fire({}); });
        else if (name == "close")
            element.tabs->closed = [fire](std::wstring key) {
                Value v;
                v.tag = 4;
                v.text = narrow(key);
                fire({v});
            };
        else if (name == "activate")
            element.as<Table>()->setOnActivated([fire](int) { fire({}); });
        else if (name == "delete")
            element.as<Table>()->setOnDeleteRequested([fire](const auto &) { fire({}); });
    }
}
} // namespace oneui::views
