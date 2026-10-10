#include "../views/fixture.h"
#include "../support/recording_canvas.h"
using namespace viewtest;
using namespace oneui;
int main(int argc,char** argv) {
    try {
        expect(argc==2,"IR argument");auto image=load(argv[1]);Vm vm(image,"Root"),font(image,"FontItem"),color(image,"ColorToken");
        auto text=[&](Vm& v,const char* name,const char* value){v.text(image.schema.model(v.table.model_id).properties.at(name).id,value);};
        text(font,"id","mono");text(font,"family","Consolas");text(font,"label","Monospace");font.values[id(image,"FontItem","available")].integer=1;
        text(color,"id","ink");color.values[id(image,"ColorToken","rgba")].integer=0xff0000ff;
        vm.lists[id(image,"Root","fonts")]={&font};vm.lists[id(image,"Root","colors")]={&color};
        vm.values[id(image,"Root","maximum")].number=1;vm.values[id(image,"Root","level")].number=.5;
        std::vector<std::pair<uint32_t,std::vector<Value>>> events;
        static decltype(events)* recorded;recorded=&events;
        vm.table.invoke=[](void* p,uint32_t command,const OneUiValue* args,size_t count){
            ++static_cast<Vm*>(p)->invokes;
            std::vector<Value> values;for(size_t i=0;i<count;++i)values.push_back(Value::copy(args[i]));recorded->emplace_back(command,std::move(values));return 0;
        };
        auto runtime=std::make_shared<Runtime>(image);runtime->initialize(&vm.table,&vm);
        auto nodes=runtime->root->children;auto picker=nodes[0]->element.as<FontPicker>();
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
        runtime->reload(image);expect(runtime->root->children[3]->element.widget==slider,"reload retains slider");
        runtime->close();expect(vm.retains==vm.releases&&font.retains==font.releases&&color.retains==color.releases,"picker VM leases balanced");
        std::cout<<"picker runtime: projections/events, slider model, local token delta and reload passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
