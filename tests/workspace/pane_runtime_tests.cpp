#include "../views/fixture.h"
#include "oneui/layout/pane_grid.h"
#include "oneui/controls/pane_header.h"
#include "oneui/controls/terminal_view.h"
#include "../support/recording_canvas.h"
using namespace viewtest;
using namespace oneui;
int main(int argc,char** argv) {
    try {
        expect(argc==2,"IR argument");auto image=load(argv[1]);
        Vm root(image,"Root"),a(image,"ApplicationNode"),b(image,"ApplicationNode"),split(image,"ApplicationNode"),
           group(image,"ApplicationGroup"),pa(image,"ApplicationPane"),pb(image,"ApplicationPane");
        auto string=[&](Vm& v,const char* name,const char* value){v.text(image.schema.model(v.table.model_id).properties.at(name).id,value);};
        auto number=[&](Vm& v,const char* name,double value){v.values[image.schema.model(v.table.model_id).properties.at(name).id].number=value;};
        string(a,"id","a");string(a,"kind","leaf");string(a,"pane","A");number(a,"ratio",.5);
        string(b,"id","b");string(b,"kind","leaf");string(b,"pane","B");number(b,"ratio",.5);
        string(split,"id","s");string(split,"kind","split");string(split,"axis","horizontal");
        string(split,"first","a");string(split,"second","b");number(split,"ratio",.5);
        string(group,"id","g");string(group,"root","s");string(pa,"id","A");string(pb,"id","B");
        string(root,"activeGroup","g");string(root,"activePane","A");
        root.lists[id(image,"Root","nodes")]={&a,&b,&split};root.lists[id(image,"Root","groups")]={&group};
        root.lists[id(image,"Root","panes")]={&pa,&pb};
        std::vector<Value> args;
        root.table.invoke=[](void* p,uint32_t,const OneUiValue* values,size_t size){
            auto& vm=*static_cast<Vm*>(p);++vm.invokes;
            for(size_t i=0;i<size;++i)vm.values[100+uint32_t(i)]=Value::copy(values[i]);return 0;
        };
        pa.table.invoke=root.table.invoke;pb.table.invoke=root.table.invoke;
        auto runtime=std::make_shared<Runtime>(image);runtime->initialize(&root.table,&root);
        auto grid=runtime->root->element.as<PaneGrid>();grid->setFrame({0,0,1000,600});
        auto pane=runtime->root->children[0]->rows.at("A")->element.as<PaneView>();
        auto header=std::dynamic_pointer_cast<PaneHeader>(pane->children()[0]);
        expect(header->children()[0]->accessibleName()==L"Divide right","authored pane action label");
        expect(header->children()[4]->accessibleName()==L"A","bound pane action label");
        auto host=std::dynamic_pointer_cast<ui::NativeHostView>(pane->children()[1]);
        auto terminal=std::make_shared<TerminalView>();terminal->setGrid(24,80,std::vector<TerminalCell>(1920));
        host->attach(terminal,{});grid->tickAnimations(0);
        test_support::RecordingCanvas canvas;grid->paint(canvas);grid->tickAnimations(1);
        expect(root.invokes==1 && root.values[100].text=="A" && root.values[101].tag==2,"measured viewport semantic ABI arguments");
        grid->onMouseDown({{100,14},MouseButton::Left});grid->onMouseUp({{100,14},MouseButton::Left});
        expect(pa.invokes==1 && pa.values[100].text=="A" && pb.invokes==0,"header routes typed item-scope activation");
        expect(grid->onMouseDown({{500,300},MouseButton::Left}),"native root divider routing");
        grid->onMouseUp({{650,300},MouseButton::Left});
        expect(root.invokes==2 && root.values[100].text=="s" && root.values[101].number>.64,"typed ratio intent");
        expect(grid->snapshot().nodes[2].ratio==.5f,"intent did not mutate VM snapshot");
        number(split,"ratio",.7);
        // Notify the actual acquired item scope; order is not a public contract.
        for(auto& scope:runtime->scopes)if(scope->vm==&split)runtime->notify(scope,0);
        runtime->flush();
        expect(grid->snapshot().nodes[2].ratio==.7f,"record update publishes authority");
        expect(pane->children()[1]==host && host->children()[0]==terminal,"content address retained");
        string(root,"activePane","B");runtime->notify(runtime->rootScope,id(image,"Root","activePane"));runtime->flush();
        expect(header->children()[4]->accessibleName()==L"B","pane action label notification");
        string(split,"first","s");for(auto& scope:runtime->scopes)if(scope->vm==&split)runtime->notify(scope,0);
        try{runtime->flush();expect(false,"cycle accepted");}catch(const std::invalid_argument&){}
        expect(grid->snapshot().nodes[2].first=="a","invalid graph keeps published snapshot");
        runtime->close();expect(root.retains==root.releases && pa.retains==pa.releases,"leases balance");
        std::cout<<"Pane runtime: schema, events, authority, invalid snapshot, identity passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
