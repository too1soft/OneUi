#pragma once
#include "oneui/oneui_component_c_api.h"
#include "../format/component_extensions.h"
#include <memory>
#include <thread>

namespace oneui::views::components {
struct Factory {
    OneUiComponentFactoryV1 api{};
    oneview::ComponentContract contract;
    std::string document;
    bool retained = false;
    ~Factory();
};
struct Registry {
    oneview::ComponentContracts contracts = oneview::registry();
    std::map<std::string,std::shared_ptr<Factory>> factories;
};
}
struct OneUiComponentRegistry {
    std::thread::id thread = std::this_thread::get_id();
    std::shared_ptr<oneui::views::components::Registry> value = std::make_shared<oneui::views::components::Registry>();
    bool busy = false;
};
