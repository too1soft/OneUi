#include "StatusDemo.g.h"
#include "oneui_generated_components.h"
#include <iostream>
#include <stdexcept>
namespace {void require(bool value) {if(!value)throw std::runtime_error("Component consumer assertion");}}
int main() {
    try {
        int calls=0;std::shared_ptr<oneui::Widget> retained;
        {
            OneUiComponentRegistry *registry=nullptr;
            require(oneui_component_registry_create_v1(&registry)==0);
            require(register_components(registry)==0);
            oneui::ui::Mount ui({},std::make_shared<oneui::ui::StyleRegistry>(),nullptr);
            ui.setComponents(registry);
            require(oneui_component_registry_destroy(registry)==0);
            auto vm=std::make_shared<StatusVM>();vm->activate.execute=[&]{++calls;};
            auto root=build_StatusDemo(vm,ui);retained=root.widget;
            require(root.widget->preferredSize().width==260);
            require(root.widget->accessibilityInfo().value==L"25%");
            require(root.widget->accessibilityInfo().name==L"Transfer status");
            oneui::KeyEvent key;key.key=oneui::Key::Enter;
            require(root.widget->onKeyDown(key) && calls==1);
            vm->value.set(0.75);ui.flush();
            require(root.widget->accessibilityInfo().value==L"75%");
            root.widget->setDisabled(true);require(!root.widget->onKeyDown(key) && calls==1);
            root.widget->setDisabled(false);
        }
        oneui::KeyEvent key;key.key=oneui::Key::Space;
        require(!retained->onKeyDown(key) && calls==1);retained.reset();
        std::cout<<"Independent component consumer passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
