#include "oneui/layout/dock_workspace_view.h"
#include "oneui/controls/drag_handle.h"
#include "internal/ui_clock.h"
#include "../support/recording_canvas.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace oneui;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double now=0;double clockNow(){return now;}
bool near(float a,float b){return std::abs(a-b)<.01f;}
struct Fixture {
    View root;
    std::shared_ptr<DockWorkspaceView> dock=std::make_shared<DockWorkspaceView>();
    std::vector<std::shared_ptr<DockItemView>> items;
    DockWorkspaceSnapshot state;
    std::vector<DockIntent> events;
    test_support::RecordingCanvas canvas;
    Fixture() {
        root.setFrame({0,0,1200,800});root.add(dock);dock->setFrame({20,40,1000,700});
        dock->setMotionEnabled(false);dock->setAnimationScheduler([]{});
        for(auto key:{"terminal","sftp","monitor","ai"}) {
            auto item=std::make_shared<DockItemView>();item->key=key;
            auto h=std::make_shared<DragHandle>();h->source=key;h->kind="dock";h->threshold=7;
            h->setOnDrag([this,key](int phase,Point p){dock->drag(key,phase,p);});
            item->add(h);item->header=h;auto body=std::make_shared<View>();item->add(body);item->body=body;items.push_back(item);
            state.placements.push_back({key,"docked"});
        }
        items[0]->dockMinimum={340,180};items[0]->floatMinimum={320,180};items[0]->hideable=false;
        state.nodes={{"t","terminal"},{"s","sftp"},{"m","monitor"},{"a","ai"},
                     {"left","","t","s",DockAxis::Vertical,.53f},{"right","","m","a",DockAxis::Vertical,.5f},
                     {"root","","left","right",DockAxis::Horizontal,.699f}};
        state.root="root";state.revision=1;publish();dock->setOnIntent([this](auto e){events.push_back(e);});
    }
    void publish(){dock->setSnapshot(state,items);dock->paint(canvas);}
    void floating() {
        state.nodes.erase(state.nodes.begin()+3); // ai
        state.nodes.erase(state.nodes.begin()+4); // right
        state.nodes.back().second="m";
        state.placements[3]={"ai","floating",{250,180,400,300},false,1};++state.revision;publish();
    }
    Point title(size_t i){auto r=items[i]->frame();return {r.x+50,r.y+14};}
    void begin(size_t i){auto p=title(i);root.onMouseDown({p});root.onMouseMove({{p.x+7,p.y}});}
};
void geometry() {
    Fixture f;auto a=f.items[0]->frame(),b=f.items[1]->frame(),c=f.items[2]->frame();
    check(near(a.width,992*.699f)&&near(c.width,992*(1-.699f)),"minimum clamps .699 without growing viewport");
    check(near(a.height,(700-8)*.53f)&&near(b.y,(a.y+a.height)+8),"vertical ratio/gap");
    auto address=f.items[3].get();f.floating();check(f.items[3].get()==address,"floating preserves object");
    auto saved=f.state.placements[3].bounds;f.dock->setFrame({20,40,200,100});f.dock->paint(f.canvas);
    check(near(f.items[3]->frame().width,200)&&near(f.dock->snapshot().placements[3].bounds.width,saved.width),"display clamp does not mutate persistence");
    f.dock->setFrame({20,40,1000,700});f.state.compactActive="sftp";f.publish();
    check(!f.items[0]->visible()&&f.items[1]->visible()&&f.items[3]->visible(),"compact retains float");
    f.state.maximized="terminal";f.publish();check(f.items[0]->visible()&&!f.items[3]->visible(),"maximize priority");
    auto bad=f.state;bad.placements[0].mode="hidden";bool rejected=false;
    try{f.dock->setSnapshot(bad,f.items);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&f.dock->snapshot().maximized=="terminal","bad candidate is atomic");
}
void drag() {
    Fixture f;auto p=f.title(0);f.root.onMouseDown({p});f.root.onMouseMove({{p.x+6.9f,p.y}});
    check(!f.dock->previewRect(),"below seven pixels");
    f.root.onMouseMove({{p.x+7,p.y}});auto r=f.items[2]->frame();Point drop{(r.x+r.width)-2,r.y+80};
    f.root.onMouseUp({drop});check(f.events.size()==1&&f.events.back().kind==DockIntentKind::Dock&&f.events.back().target=="monitor"&&f.events.back().edge=="right","release final point rehit");
    check(f.dock->snapshot().nodes.back().ratio==.699f&&f.dock->paintPending(),"request keeps authority");
    f.state.ackRequest=f.events.back().request;f.state.ackAccepted=false;f.publish();check(!f.dock->previewRect(),"ack rejection retires preview");
    f.begin(0);MouseEvent up{drop};up.alt=true;f.root.onMouseUp(up);
    check(f.events.back().kind==DockIntentKind::Float,"final Alt wins over previous docking target");
    check(f.events.back().bounds.x>=0&&(f.events.back().bounds.x+f.events.back().bounds.width)<=1000,"float event coordinates local and clamped");
    auto count=f.events.size();f.begin(0);++f.state.revision;f.publish();f.root.onMouseUp({drop});check(f.events.size()==count,"stale source revision cancelled");
    f.begin(0);f.root.clearInteractionState();f.root.onMouseUp({drop});check(f.events.size()==count,"capture loss no commit");
    f.begin(0);KeyEvent esc;esc.key=Key::Escape;f.root.onKeyDown(esc);f.root.onMouseUp({drop});check(f.events.size()==count,"escape no commit");
    f.state.compactActive="terminal";f.publish();f.begin(0);f.root.onMouseUp(up);check(f.events.size()==count,"compact docked cannot drag");
}
void floatingResize() {
    Fixture f;f.floating();f.begin(3);auto target=f.items[0]->frame();Point center{target.x+target.width*.5f,target.y+target.height*.5f};
    f.root.onMouseUp({center});check(f.events.back().kind==DockIntentKind::Bounds,"float strict center does not dock");
    f.state.ackRequest=f.events.back().request;f.publish();
    const DockResizeEdge edges[]={DockResizeEdge::North,DockResizeEdge::South,DockResizeEdge::East,DockResizeEdge::West,DockResizeEdge::NorthEast,DockResizeEdge::NorthWest,DockResizeEdge::SouthEast,DockResizeEdge::SouthWest};
    for(auto edge:edges) {
        auto r=f.items[3]->frame();Point p{r.x+r.width*.5f,r.y+r.height*.5f};
        if(edge==DockResizeEdge::North||edge==DockResizeEdge::NorthEast||edge==DockResizeEdge::NorthWest)p.y=r.y+1;
        if(edge==DockResizeEdge::South||edge==DockResizeEdge::SouthEast||edge==DockResizeEdge::SouthWest)p.y=(r.y+r.height)-1;
        if(edge==DockResizeEdge::West||edge==DockResizeEdge::NorthWest||edge==DockResizeEdge::SouthWest)p.x=r.x+1;
        if(edge==DockResizeEdge::East||edge==DockResizeEdge::NorthEast||edge==DockResizeEdge::SouthEast)p.x=(r.x+r.width)-1;
        f.root.onMouseDown({p});f.root.onMouseMove({{p.x+2000,p.y+2000}});f.root.onMouseUp({{p.x+10,p.y+10}});
        Rect expected;resizeDockFloating(f.state.placements[3].bounds,{10,10},edge,{0,0,1000,700},f.items[3]->floatMinimum,expected);
        auto event=f.events.back();check(event.kind==DockIntentKind::Bounds&&near(event.bounds.x,expected.x)&&near(event.bounds.width,expected.width)&&near(event.bounds.height,expected.height),"eight-way reverse uses original plus total delta");
        f.state.ackRequest=event.request;f.publish();
    }
    auto before=f.items[3].get();f.state.placements[3].pinned=true;f.state.placements[3].order=9;f.publish();check(f.items[3].get()==before&&f.dock->children().back().get()==before,"pin/order retains object");
}
void dividerAndActions() {
    Fixture f;auto r=f.items[0]->frame();Point p{(r.x+r.width)+4,50};
    f.root.onMouseDown({p});f.root.onMouseMove({{p.x-50,p.y}});f.root.onMouseUp({{p.x-80,p.y}});
    check(f.events.back().kind==DockIntentKind::Ratio&&f.events.back().ratio<.699,"divider final point and semantic ratio");
    auto count=f.events.size();KeyEvent right;right.key=Key::Right;f.root.onKeyDown(right);
    now=119;f.dock->tickAnimations(now);check(f.events.size()==count,"keyboard 120ms idle");
    now=121;f.dock->tickAnimations(now);check(f.events.size()==count+1,"one ratio after keyboard idle");
    f.dock->requestAction(DockIntentKind::Visibility,"terminal",false);check(f.events.size()==count+1,"nonhideable suppresses request");
    f.dock->requestAction(DockIntentKind::Float,"terminal");check(f.events.back().bounds.width<=560&&f.events.back().bounds.height<=620,"menu float size caps");
    f.dock->requestAction(DockIntentKind::Pin,"sftp",true);check(f.events.back().flag,"pin intent");
    f.dock->requestAction(DockIntentKind::Redock,"sftp");check(f.events.back().kind==DockIntentKind::Redock,"redock intent");
    auto title=f.title(0);MouseEvent dbl{title};dbl.clickCount=2;f.root.onMouseDown(dbl);check(f.events.back().kind==DockIntentKind::Maximize,"title doubleclick maximize intent");
}
}
int main(){try{internal::ScopedUiClock clock(clockNow);geometry();drag();floatingResize();dividerAndActions();std::cout<<"dock workspace: 4 scenario groups passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
