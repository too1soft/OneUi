#include "../views/fixture.h"
#include "views/components/instance.h"
#include "oneui/native_interop.h"
#include "../support/recording_canvas.h"
#include <thread>
using namespace viewtest;
namespace {
struct Stats { int retains=0,releases=0,created=0,freed=0,disconnected=0,styles=0; bool fail=false; };
struct Meter final : oneui::Widget {
    std::string title;
    bool checked=false;
    double offset=0;
    int64_t count=0;
    void paint(oneui::Canvas &canvas) override { canvas.fillRect(frame(),{12,34,56},0); }
};
struct State { Stats *stats; Meter *widget; const OneUiComponentHostV1 *host; };
State *last=nullptr;
void retain(void *data) { ++static_cast<Stats*>(data)->retains; }
void release(void *data) { ++static_cast<Stats*>(data)->releases; }
int create(void *data,const OneUiComponentHostV1 *host,OneUiWidget *const *,size_t count,void **out,OneUiWidget **widget) {
    auto &stats=*static_cast<Stats*>(data);++stats.created;
    expect(count==0,"leaf child count");
    auto meter=std::make_shared<Meter>();
    last=new State{&stats,meter.get(),host};*out=last;
    if(stats.fail) return ONEUI_VIEW_FOREIGN_FAILURE;
    *widget=oneui::createNativeWidgetHandle(meter);return ONEUI_VIEW_OK;
}
int set(void *instance,uint32_t id,const OneUiValue *value) {
    auto &state=*static_cast<State*>(instance);
    if(id==1000) state.widget->title.assign(value->as.utf8.data,value->as.utf8.length);
    else if(id==1001) state.widget->checked=value->as.boolean!=0;
    else if(id==1002) state.widget->offset=value->as.number;
    else if(id==1003) state.widget->count=value->as.integer;
    else return ONEUI_VIEW_UNKNOWN_ID;
    return ONEUI_VIEW_OK;
}
void disconnect(void *instance) { if(instance) ++static_cast<State*>(instance)->stats->disconnected; }
void freed(void *instance) { if(instance) {auto state=static_cast<State*>(instance);++state->stats->freed;delete state;} }
void styles(void *instance) { if(instance) ++static_cast<State*>(instance)->stats->styles; }
OneUiComponentFactoryV1 descriptor(Stats &stats,const std::string &document) {
    return {sizeof(OneUiComponentFactoryV1),1,{document.data(),document.size()},&stats,retain,release,create,set,disconnect,freed,styles};
}
std::string read(const char *path) {std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
void live(const char *file,const std::string &contract) {
    Stats stats;auto d=descriptor(stats,contract);
    OneUiComponentRegistry *registry=nullptr;
    expect(oneui_component_registry_create_v1(&registry)==0,"create component registry");
    expect(oneui_component_registry_register_v1(registry,&d)==0,"register external factory");
    expect(oneui_component_registry_register_v1(registry,&d)==ONEUI_VIEW_INVALID_ARGUMENT,"duplicate component rejected");
    auto snapshot=registry->value;
    expect(oneui_component_registry_destroy(registry)==0 && stats.releases==0,"snapshot retains factory data");
    std::shared_ptr<oneui::Widget> retained;
    {
        Case c(file,snapshot);c.start();
        retained=c.runtime->root->element.widget;
        auto widget=std::dynamic_pointer_cast<Meter>(retained);
        expect(widget && widget->title=="initial" && widget->offset==-2.5 && widget->count==9000000000000000000ll,"typed static and bound properties");
        expect(widget->preferredSize().width==200 && stats.styles>0,"class styles and layout");
        OneUiComponentStyleV1 style{sizeof(style)};
        expect(last->host->style(last->host->token,0,&style)==0 && style.foreground==0x123456ff,"inherited style query");
        c.vm.text(c.title,"updated");c.runtime->notify(c.runtime->rootScope,c.title);c.runtime->flush();
        expect(widget->title=="updated","scalar notification reaches plugin");
        OneUiValue checked{};checked.tag=ONEUI_VALUE_BOOL;checked.as.boolean=1;
        expect(last->host->change(last->host->token,1001,&checked)==0 && c.vm.sets==1,"v-model writes through the VM");
        c.runtime->flush();expect(widget->checked,"v-model update reaches native widget");
        expect(last->host->emit(last->host->token,1,nullptr,0)==0 && c.vm.invokes==1,"semantic event reaches command");
        int result=0;std::thread worker([&]{result=last->host->emit(last->host->token,1,nullptr,0);});worker.join();
        expect(result==ONEUI_VIEW_WRONG_THREAD && c.vm.invokes==1,"wrong-thread event is rejected");
        c.vm.onRelease=[&]{expect(stats.disconnected==1,"disconnect before releasing VM");};
        c.runtime->close();
        expect(last->host->emit(last->host->token,1,nullptr,0)==ONEUI_VIEW_CLOSED,"late event is closed");
        expect(stats.freed==0,"native owner survives view close");
    }
    snapshot.reset();expect(stats.releases==0,"retained widget keeps code/data owner");
    retained.reset();expect(stats.freed==1 && stats.disconnected==1 && stats.releases==1,"complete paired release");
}
void failed(const char *file,const std::string &contract) {
    Stats stats;stats.fail=true;auto d=descriptor(stats,contract);
    OneUiComponentRegistry *registry=nullptr;expect(oneui_component_registry_create_v1(&registry)==0,"registry");
    expect(oneui_component_registry_register_v1(registry,&d)==0,"register");
    { Case c(file,registry->value);bool rejected=false;try{c.start();}catch(...){rejected=true;}
      expect(rejected && stats.created==1 && stats.freed==1 && c.vm.retains==c.vm.releases,"failed factory rolls back ownership"); }
    expect(oneui_component_registry_destroy(registry)==0 && stats.releases==1,"failed factory data released");
}
void compatibility(const char *file,const std::string &contract) {
    bool rejected=false;try{load(file);}catch(...){rejected=true;}expect(rejected,"unknown component rejected before VM access");
    Stats stats;auto changed=contract;auto at=changed.find("Test_Meter 1");changed.replace(at,12,"Test_Meter 2");auto d=descriptor(stats,changed);
    OneUiComponentRegistry *registry=nullptr;expect(oneui_component_registry_create_v1(&registry)==0,"registry");
    expect(oneui_component_registry_register_v1(registry,&d)==0,"register version 2");
    rejected=false;try{Case c(file,registry->value);}catch(...){rejected=true;}
    expect(rejected && stats.created==0,"contract mismatch rejects before factory");
    expect(oneui_component_registry_destroy(registry)==0,"destroy");
    auto malformed=contract;auto id=malformed.find("1000");malformed.replace(id,4,"-1");
    rejected=false;try{oneview::parseComponentExtensions(malformed);}catch(...){rejected=true;}
    expect(rejected,"negative unsigned metadata rejected");
}
void reloading(const char *file,const std::string &contract) {
    Stats stats;auto d=descriptor(stats,contract);OneUiComponentRegistry *registry=nullptr;
    expect(oneui_component_registry_create_v1(&registry)==0 && oneui_component_registry_register_v1(registry,&d)==0,"reload registry");
    {
        Case c(file,registry->value);c.start();auto original=c.runtime->root;
        auto retained=original->element.widget;auto previous=last;
        stats.fail=true;bool rejected=false;try{c.runtime->reload(c.image);}catch(...){rejected=true;}
        expect(rejected && c.runtime->root==original && !original->extension->disconnected,"failed reload retains live factory");
        expect(previous->host->emit(previous->host->token,1,nullptr,0)==0,"old event works after failed reload");
        stats.fail=false;c.runtime->reload(c.image);
        expect(c.runtime->root!=original,"reload commits a new external widget");
        // Callers may retain the old widget; its context closes when the old semantic node leaves.
        original.reset();
        expect(previous->host->emit(previous->host->token,1,nullptr,0)==ONEUI_VIEW_CLOSED,"replaced widget callback closes");
        expect(last->host->emit(last->host->token,1,nullptr,0)==0,"new event reaches retained VM");
        c.vm.text(c.title,"after reload");c.runtime->notify(c.runtime->rootScope,c.title);c.runtime->flush();
        expect(std::dynamic_pointer_cast<Meter>(c.runtime->root->element.widget)->title=="after reload","reload bindings remain active");
    }
    expect(stats.created==3 && stats.freed==3 && stats.disconnected==3,"reload releases all factory instances");
    expect(oneui_component_registry_destroy(registry)==0 && stats.releases==1,"reload releases factory owner");
}
}
int main(int argc,char **argv) {
    try {expect(argc==3,"IR and contract paths required");auto document=read(argv[2]);live(argv[1],document);failed(argv[1],document);compatibility(argv[1],document);reloading(argv[1],document);std::cout<<"Component extension lifecycle passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
