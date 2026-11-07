#pragma once
#include "registry.h"
#include "oneui/style_sheet.h"
#include "oneui/ui_declarative.h"

namespace oneui::views {
struct Runtime;
struct Instance;
struct Scope;
namespace components {
struct InstanceState : std::enable_shared_from_this<InstanceState> {
    const std::thread::id thread = std::this_thread::get_id();
    std::shared_ptr<Factory> factory;
    std::weak_ptr<Instance> node;
    OneUiComponentHostV1 host{};
    void *instance = nullptr;
    bool attempted = false, disconnected = false;
    std::shared_ptr<StyleSheet> sheet;
    StyleNode styleNode;
    ~InstanceState();
    void disconnect() noexcept;
    void applyStyle(const StyleSheet &, const StyleNode &);
};
bool create(Runtime &, const std::shared_ptr<Instance> &, const std::string &,
            const std::vector<ui::Element> &, const std::string &);
bool set(const std::shared_ptr<Instance> &, uint32_t property, const OneUiValue &);
void closeScope(Runtime &, const std::shared_ptr<Scope> &);
}
}
