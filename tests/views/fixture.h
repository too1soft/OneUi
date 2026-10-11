#pragma once
#include "views/runtime.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
namespace viewtest {
using namespace oneui::views;
inline void expect(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Vm {
    OneUiViewModelVTable table{};
    std::map<uint32_t, Value> values;
    std::map<uint32_t, int> reads;
    std::vector<Vm *> items;
    std::map<uint32_t,std::vector<Vm*>> lists;
    std::map<uint32_t,int> infos,keys,leases;
    std::string scratch;
    int retains = 0, releases = 0, connects = 0, disconnects = 0, sets = 0, invokes = 0,
        acquires = 0;
    uint64_t revision = 1;
    uint32_t key = 0;
    std::function<void()> onGet, onInvoke, onConnect, onDisconnect, onRelease;
    OneUiViewNotifier *notifier = nullptr;
    int connectResult = 0;
    uint32_t invokedCommand=0;
    std::vector<Value> invokedArgs;
    explicit Vm(const oneview::Image &image, const char *model) {
        table.struct_size = sizeof(table);
        table.protocol_version = 1;
        auto hash = oneview::sha256(image.schema.encode());
        std::copy(hash.begin(), hash.end(), table.schema_hash);
        const auto &m = image.schema.model(model);
        table.model_id = m.id;
        key = m.key;
        for (const auto &p : m.properties)
            if (p.second.tag != 5) {
                Value v;
                v.tag = p.second.tag;
                values[p.second.id] = v;
            }
        table.retain = [](void *p) { ++static_cast<Vm *>(p)->retains; };
        table.release = [](void *p) {
            auto &v = *static_cast<Vm *>(p);
            ++v.releases;
            if (v.onRelease)
                v.onRelease();
        };
        table.connect = [](void *p, OneUiViewNotifier *n) {
            auto &v = *static_cast<Vm *>(p);
            ++v.connects;
            v.notifier = n;
            if (v.onConnect)
                v.onConnect();
            return v.connectResult;
        };
        table.disconnect = [](void *p, OneUiViewNotifier *) {
            auto &v = *static_cast<Vm *>(p);
            ++v.disconnects;
            if (v.onDisconnect)
                v.onDisconnect();
        };
        table.get = [](void *p, uint32_t id, OneUiValue *out) {
            auto &v = *static_cast<Vm *>(p);
            ++v.reads[id];
            if (v.onGet)
                v.onGet();
            auto value = v.values.at(id);
            *out = value.borrow();
            if (value.tag == 4) {
                v.scratch = value.text;
                out->as.utf8 = {v.scratch.data(), v.scratch.size()};
            }
            return 0;
        };
        table.set = [](void *p, uint32_t id, const OneUiValue *value) {
            auto &v = *static_cast<Vm *>(p);
            ++v.sets;
            v.values[id] = Value::copy(*value);
            return 0;
        };
        table.invoke = [](void *p, uint32_t command, const OneUiValue *args, size_t count) {
            auto &v = *static_cast<Vm *>(p);
            v.invokedCommand=command;v.invokedArgs.clear();
            for(size_t i=0;i<count;++i)v.invokedArgs.push_back(Value::copy(args[i]));
            ++v.invokes;
            if (v.onInvoke)
                v.onInvoke();
            return 0;
        };
        table.list_info = [](void *p, uint32_t property, OneUiViewListInfo *out) {
            auto &v = *static_cast<Vm *>(p);
            ++v.infos[property];
            *out = {v.revision, v.lists.count(property) ? v.lists.at(property).size() : v.items.size()};
            return 0;
        };
        table.list_key = [](void *p, uint32_t property, uint64_t i, OneUiValue *out) {
            auto &v = *static_cast<Vm *>(p);
            ++v.keys[property];
            auto& items=v.lists.count(property) ? v.lists.at(property) : v.items;
            v.scratch = items.at(size_t(i))->values.at(items.at(size_t(i))->key).text;
            *out = {};
            out->tag = 4;
            out->as.utf8 = {v.scratch.data(), v.scratch.size()};
            return 0;
        };
        table.list_acquire = [](void *p, uint32_t property, uint64_t i, OneUiViewModelRef *out) {
            auto &v = *static_cast<Vm *>(p);
            ++v.acquires;
            ++v.leases[property];
            auto *item = (v.lists.count(property) ? v.lists.at(property) : v.items).at(size_t(i));
            ++item->retains;
            *out = {&item->table, item};
            return 0;
        };
    }
    void text(uint32_t id, std::string text) {
        values[id].tag = 4;
        values[id].text = std::move(text);
    }
};
inline oneview::Image load(const char *path) {
    std::ifstream in(path, std::ios::binary);
    expect(bool(in), "missing IR fixture");
    return oneview::readImage(oneview::Bytes(std::istreambuf_iterator<char>(in), {}));
}
inline uint32_t id(const oneview::Image &image, const char *model, const char *name) {
    return image.schema.model(model).properties.at(name).id;
}
struct Case {
    oneview::Image image;
    Vm vm, a, b;
    std::shared_ptr<Runtime> runtime;
    uint32_t title, rows;
    explicit Case(const char *file)
        : image(load(file)), vm(image, "Root"), a(image, "Item"), b(image, "Item"),
          title(id(image, "Root", "title")), rows(id(image, "Root", "rows")) {
        vm.text(title, "initial");
        vm.values[id(image, "Root", "visible")].integer = 1;
        vm.values[id(image, "Root", "enabled")].integer = 1;
        a.text(a.key, "a");
        b.text(b.key, "b");
        a.text(id(image, "Item", "text"), "Alpha");
        b.text(id(image, "Item", "text"), "Beta");
        vm.items = {&a, &b};
        runtime = std::make_shared<Runtime>(image);
    }
    void start() { runtime->initialize(&vm.table, &vm); }
    ~Case() { runtime->close(); }
};
} // namespace viewtest
