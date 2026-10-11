#include "../views/fixture.h"
#include "../support/recording_canvas.h"
#include "../../src/views/data_menu.h"
using namespace viewtest;
using namespace oneui;
int main(int argc,char** argv) {
    try {
        expect(argc==2,"IR argument");auto image=load(argv[1]);Vm vm(image,"Root"),font(image,"FontItem"),color(image,"ColorToken");
        auto text=[&](Vm& v,const char* name,const char* value){v.text(image.schema.model(v.table.model_id).properties.at(name).id,value);};
        text(font,"id","mono");text(font,"family","Consolas");text(font,"label","Monospace");font.values[id(image,"FontItem","available")].integer=1;
        text(color,"id","ink");color.values[id(image,"ColorToken","rgba")].integer=0xff0000ff;
        vm.lists[id(image,"Root","fonts")]={&font};vm.lists[id(image,"Root","colors")]={&color};
        Vm menuItem(image,"MenuEntry");
        text(menuItem,"id","menu-command"); text(menuItem,"text","Run");
        text(menuItem,"icon","terminal-native");
        vm.lists[id(image,"Root","menu")]={&menuItem};
        vm.values[id(image,"Root","maximum")].number=1;vm.values[id(image,"Root","level")].number=.5;
        std::vector<std::pair<uint32_t,std::vector<Value>>> events;
        static decltype(events)* recorded;recorded=&events;
        vm.table.invoke=[](void* p,uint32_t command,const OneUiValue* args,size_t count){
            ++static_cast<Vm*>(p)->invokes;
            std::vector<Value> values;for(size_t i=0;i<count;++i)values.push_back(Value::copy(args[i]));recorded->emplace_back(command,std::move(values));return 0;
        };
        auto runtime=std::make_shared<Runtime>(image);runtime->initialize(&vm.table,&vm);
        auto nodes=runtime->root->children;auto picker=nodes[0]->element.as<FontPicker>();
        auto menu=nodes[5]->element.as<DataMenu>();
        menu->setFrame({0,0,224,100});
        KeyEvent menuKey; menuKey.key=Key::Down; menu->onKeyDown(menuKey);
        menuKey.key=Key::Enter; menu->onKeyDown(menuKey);
        expect(events.back().second[0].text=="menu-command","declarative menu emits string command ID");
        const auto menuCalls=events.size();
        menuItem.values[id(image,"MenuEntry","disabled")].integer=1;
        for(auto& s:runtime->scopes)if(s->vm==&menuItem)runtime->notify(s,0);
        runtime->flush(); menuKey.key=Key::Down; menu->onKeyDown(menuKey);
        menuKey.key=Key::Enter; menu->onKeyDown(menuKey);
        expect(events.size()==menuCalls,"live disabled update prevents activation");
        picker->setFrame({0,0,300,36});picker->setOpen(true);KeyEvent enter;enter.key=Key::Enter;picker->onKeyDown(enter);
        expect(events.back().second[0].text=="mono"&&picker->config().selected.empty(),"font semantic key does not mutate selection");
        auto slider=nodes[3]->element.as<Slider>();slider->setFrame({0,0,300,36});int sets=vm.sets;
        slider->setValueFromInput(.8);expect(vm.sets==sets+1,"v-model setter once for input");
        expect(events.back().second[0].integer==2&&events.back().second[1].tag==3,"slider commit typed phase/value");
        runtime->flush();expect(slider->value()==.8,"slider accepted value");
        auto calls=events.size();vm.values[id(image,"Root","level")].number=.25;runtime->notify(runtime->rootScope,id(image,"Root","level"));runtime->flush();
        expect(slider->value()==.25&&events.size()==calls&&vm.sets==sets+1,"programmatic slider does not echo write or interaction");
        auto preview=nodes[2]->element.as<MediaPreview>();auto nativeImage=preview->image();
        expect(nativeImage->contentMode()==ImageContentMode::Cover&&nativeImage->horizontalContentAlignment()==ImageContentAlignment::End,"preview properties registered");
        auto scope=nodes[4]->element.as<ThemeScope>();auto revision=scope->revision();
        color.values[id(image,"ColorToken","rgba")].integer=0x00ff00ff;
        for(auto& s:runtime->scopes)if(s->vm==&color)runtime->notify(s,0);runtime->flush();
        expect(scope->revision()==revision+1&&preview->image()==nativeImage,"token delta keeps unrelated native identities");
        auto popup=nodes[6]->element.as<Popup>();popup->setFrame({0,0,320,240});
        const auto beforePopup=events.size();
        vm.values[id(image,"Root","popupOpen")].integer=1;
        runtime->notify(runtime->rootScope,0);runtime->flush();
        expect(popup->isOpen()&&events.size()==beforePopup,"VM opens popup without command echo");
        KeyEvent escape;escape.key=Key::Escape;popup->onKeyDown(escape);
        expect(!popup->isOpen()&&events.size()==beforePopup+1&&events.back().second.empty(),"native Escape emits closed command");
        vm.values[id(image,"Root","popupOpen")].integer=0;
        runtime->notify(runtime->rootScope,0);runtime->flush();
        expect(events.size()==beforePopup+1,"accepting close does not echo command");
        runtime->reload(image);expect(runtime->root->children[3]->element.widget==slider,"reload retains slider");
        runtime->close();expect(vm.retains==vm.releases&&font.retains==font.releases&&color.retains==color.releases,"picker VM leases balanced");
        std::cout<<"picker runtime: projections/events, slider model, local token delta and reload passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
