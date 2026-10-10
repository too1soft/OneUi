#include "../views/fixture.h"
#include "oneui/layout/dock_workspace_view.h"
#include "../support/recording_canvas.h"
using namespace viewtest;
using namespace oneui;
int main(int argc,char** argv) {
    try {
        expect(argc==2,"IR argument");auto image=load(argv[1]);
        Vm root(image,"Root"),node(image,"ToolNode"),pa(image,"Placement"),pb(image,"Placement"),a(image,"Tool"),b(image,"Tool");
        auto text=[&](Vm& v,const char* name,const char* value){v.text(image.schema.model(v.table.model_id).properties.at(name).id,value);};
        auto number=[&](Vm& v,const char* name,double value){v.values[image.schema.model(v.table.model_id).properties.at(name).id].number=value;};
        text(node,"id","n");text(node,"kind","leaf");text(node,"pane","A");number(node,"ratio",.5);
        text(a,"id","A");text(b,"id","B");text(pa,"id","A");text(pb,"id","B");text(pa,"mode","docked");text(pb,"mode","floating");
        number(pb,"x",300);number(pb,"y",200);number(pb,"width",300);number(pb,"height",250);
        text(root,"root","n");root.lists[id(image,"Root","nodes")]={&node};root.lists[id(image,"Root","placements")]={&pa,&pb};root.lists[id(image,"Root","panes")]={&a,&b};
        root.table.invoke=[](void* p,uint32_t id,const OneUiValue* args,size_t count) {
            auto& vm=*static_cast<Vm*>(p);++vm.invokes;
            for(size_t i=0;i<count;++i)vm.values[100+uint32_t(i)]=Value::copy(args[i]);return 0;
        };
        auto runtime=std::make_shared<Runtime>(image);runtime->initialize(&root.table,&root);
        auto dock=runtime->root->element.as<DockWorkspaceView>();dock->setFrame({20,40,1000,700});
        test_support::RecordingCanvas canvas;dock->paint(canvas);
        auto row=runtime->root->children[0]->rows.at("B");auto item=row->element.as<DockItemView>();
        auto host=row->children[1]->children[0]->element.as<ui::NativeHostView>();
        auto content=std::make_shared<View>();int releases=0;auto lease=std::shared_ptr<void>(new int(1),[&](void* p){++releases;delete static_cast<int*>(p);});
        host->attach(content,lease);lease.reset();
        auto handle=row->children[0]->children[0]->element.widget;
        auto h=handle->frame();Point start{h.x+h.width*.5f,h.y+h.height*.5f};
        dock->onMouseDown({start});dock->onMouseMove({{start.x+7,start.y}});
        MouseEvent release{{700,start.y}};release.alt=true;dock->onMouseUp(release);
        expect(root.invokes==2&&root.values[100].text=="B"&&root.values[101].tag==3,"dock drag routes typed bounds and raise");
        expect(dock->snapshot().placements[1].bounds.x==300,"VM still owns bounds");
        number(pb,"x",500);for(auto& scope:runtime->scopes)if(scope->vm==&pb)runtime->notify(scope,0);runtime->flush();dock->paint(canvas);
        expect(item==row->element.as<DockItemView>()&&host->children()[0]==content&&releases==0,"placement update retains content and lease");
        text(pb,"mode","hidden");for(auto& scope:runtime->scopes)if(scope->vm==&pb)runtime->notify(scope,0);runtime->flush();dock->paint(canvas);
        expect(!item->visible()&&releases==0,"hidden retains leased content");
        text(pb,"mode","floating");for(auto& scope:runtime->scopes)if(scope->vm==&pb)runtime->notify(scope,0);runtime->flush();dock->paint(canvas);
        expect(item->visible()&&host->children()[0]==content,"restore identical content");
        runtime->close();expect(root.retains==root.releases&&b.retains==b.releases,"VM retains balance");
        host.reset();row.reset();dock.reset();item.reset();runtime.reset();expect(releases==1,"content lease releases once");
        std::cout<<"dock runtime: typed projection/events, hidden restore, identity and lease passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
