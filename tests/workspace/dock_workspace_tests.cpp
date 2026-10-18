#include "oneui/layout/dock_workspace_view.h"
#include "oneui/controls/drag_handle.h"
#include "oneui/layout/stack.h"
#include "oneui/style_sheet.h"
#include "oneui/animation.h"
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
    DockWorkspacePresentation presentation;
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
    void publish(){dock->setSnapshot(state,items,presentation);dock->paint(canvas);}
    void floating() {
        state.nodes.erase(state.nodes.begin()+3); // ai
        state.nodes.erase(state.nodes.begin()+4); // right
        state.nodes.back().second="m";
        state.placements[3]={"ai","floating",{250,180,400,300},false,1};++state.revision;publish();
    }
    Point title(size_t i){auto r=items[i]->frame();return {r.x+50,r.y+14};}
    void begin(size_t i){auto p=title(i);root.onMouseDown({p});root.onMouseMove({{p.x+7,p.y}});}
};
#include "dock_header_geometry.inc"
void presentationTransaction() {
    Fixture f;
    auto presentation=f.presentation;
    presentation.previewKeepText="next caption";presentation.raiseOnPress=false;
    auto bad=f.state;bad.placements.front().mode="invalid";
    bool rejected=false;
    try {f.dock->prepare(bad,f.items,presentation);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&f.dock->presentation().raiseOnPress&&
          f.dock->presentation().previewKeepText==f.presentation.previewKeepText,
          "failed snapshot preparation preserves all presentation configuration");
    auto commit=f.dock->prepare(f.state,f.items,presentation);
    check(f.dock->presentation().raiseOnPress,"prepared configuration remains unpublished until commit");
    commit();
    check(!f.dock->presentation().raiseOnPress&&f.dock->presentation().previewKeepText=="next caption",
          "snapshot and presentation commit together");
    f.dock->setSnapshot(f.state,f.items);
    check(!f.dock->presentation().raiseOnPress,"old snapshot API preserves independent presentation");
}
void geometry() {
    Fixture f;auto a=f.items[0]->frame(),b=f.items[1]->frame(),c=f.items[2]->frame();
    auto sheet=std::make_shared<StyleSheet>();std::string cssError;
    check(sheet->addRulesFromCss("dock-workspace-part.dock-workspace-divider {background:#112233;}",&cssError),"divider part stylesheet");
    f.dock->setStyleScope(sheet);test_support::RecordingCanvas themed;f.dock->paint(themed);
    check(!themed.fillRects.empty()&&themed.fillRects[0].color.r==0x11&&themed.fillRects[0].color.g==0x22,
          "dock divider consumes application theme rather than fixed material");
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
void dropPolicy() {
    using P=DockWorkspacePresentation;
    {   // Outside-guides: releasing over empty space floats without a modifier.
        Fixture f;f.presentation.floatOnDrop=P::FloatOnDrop::OutsideGuides;f.presentation.guideOnlyDocking=true;f.publish();
        f.begin(1);auto r=f.items[0]->frame();Point open{r.x+r.width*.5f,r.y+r.height*.5f+90};
        f.root.onMouseMove({open});check(f.dock->willFloat(),"outside-guides floats by default");
        f.root.onMouseUp({open});check(!f.events.empty()&&f.events.back().kind==DockIntentKind::Float,"outside-guides drop floats");
    }
    {   // Explicit guides still dock under outside-guides.
        Fixture f;f.presentation.floatOnDrop=P::FloatOnDrop::OutsideGuides;f.publish();
        f.begin(0);auto r=f.items[2]->frame();Point edge{(r.x+r.width)-2,r.y+80};
        f.root.onMouseUp({edge});check(!f.events.empty()&&f.events.back().kind==DockIntentKind::Dock,"nearest docking keeps working");
    }
    {   // Shift as the configured modifier; Alt no longer floats.
        Fixture f;f.presentation.floatModifier=P::FloatModifier::Shift;f.publish();
        auto r=f.items[2]->frame();Point drop{(r.x+r.width)-2,r.y+80};
        f.begin(0);MouseEvent alt{drop};alt.alt=true;f.root.onMouseUp(alt);
        check(f.events.back().kind==DockIntentKind::Dock,"Alt is not the configured modifier");
        f.state.ackRequest=f.events.back().request;f.state.ackAccepted=false;++f.state.revision;f.publish();
        f.begin(0);MouseEvent shift{drop};shift.shift=true;f.root.onMouseUp(shift);
        check(f.events.back().kind==DockIntentKind::Float,"Shift floats when configured");
    }
    {   // Never: no modifier floats a docked pane by drag.
        Fixture f;f.presentation.floatOnDrop=P::FloatOnDrop::Never;f.publish();
        auto count=f.events.size();f.begin(1);auto r=f.items[0]->frame();MouseEvent up{{r.x+r.width*.5f,r.y+r.height*.5f}};up.alt=true;
        f.root.onMouseUp(up);check(f.events.size()==count||f.events.back().kind!=DockIntentKind::Float,"never does not float");
    }
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
bool painted(const test_support::RecordingCanvas& c,Color color,const Rect* rect=nullptr) {
    for(const auto& call:c.fillRects)
        if(call.color.r==color.r&&call.color.g==color.g&&call.color.b==color.b&&
           (!rect||(near(call.rect.x,rect->x)&&near(call.rect.y,rect->y)&&near(call.rect.width,rect->width)&&near(call.rect.height,rect->height))))return true;
    return false;
}
void scopedPartsAndFloatingSurface() {
    Fixture f;f.floating();
    auto sheet=std::make_shared<StyleSheet>();std::string error;
    // viewc appends the template scope class to every scoped selector.
    check(sheet->addRulesFromCss(
        "dock-workspace-part.dock-workspace-caption.one-s-t {background:#102030;color:#e0e0e0;}"
        "dock-workspace-part.dock-workspace-guide.one-s-t {background:#334455;color:#aabbcc;}"
        "dock-workspace-part.dock-workspace-guide.one-s-t:selected {background:#00aa55;}"
        "dock-workspace-part.dock-workspace-float.one-s-t {background:#202428;border-color:#445566;border-width:1px;border-radius:10px;}",&error),
        "dock part CSS parses");
    f.dock->setStyleSheet(sheet,StyleNode{"dock-workspace",{"one-DockWorkspace","one-s-t"},0});f.publish();
    test_support::RecordingCanvas idle;f.dock->paint(idle);
    const auto floating=f.items[3]->frame(),docked=f.items[0]->frame();
    check(painted(idle,{0x20,0x24,0x28},&floating),"floating item paints the scoped dock-workspace-float surface");
    check(!painted(idle,{0x20,0x24,0x28},&docked),"docked items paint no floating surface");
    check(near(f.items[3]->body.lock()->frame().x,floating.x+1),"floating content sits inside the surface border");
    f.begin(0);auto target=f.items[2]->frame();f.root.onMouseMove({{target.x+target.width-2,target.y+80}});
    check(f.dock->previewRect().has_value(),"dragging onto a docked target shows a preview");
    test_support::RecordingCanvas dragging;f.dock->paint(dragging);
    check(painted(dragging,{0x10,0x20,0x30}),"scoped caption rule styles the drag caption");
    check(painted(dragging,{0x33,0x44,0x55}),"scoped guide rule styles idle compass guides");
    KeyEvent esc;esc.key=Key::Escape;f.root.onKeyDown(esc);
}
void transactionalPointerAndPreview() {
    Fixture f;f.floating();f.presentation.raiseOnPress=false;f.publish();
    const auto before=f.state.placements[3];auto p=f.title(3);
    f.root.onMouseDown({p});f.root.onMouseMove({{p.x+20,p.y+20}});
    KeyEvent esc;esc.key=Key::Escape;f.root.onKeyDown(esc);
    check(f.events.empty()&&!f.dock->interactionActive()&&!f.dock->previewRect(),"transactional drag Escape sends no mutation including Raise");
    auto r=f.items[3]->frame();Point corner{r.x+r.width-1,r.y+r.height-1};
    f.root.onMouseDown({corner});f.root.onMouseMove({{corner.x-50,corner.y-50}});
    f.root.onKeyDown(esc);f.root.onMouseUp({corner});
    check(f.events.empty()&&near(f.items[3]->frame().width,before.bounds.width)&&f.dock->snapshot().placements[3].order==before.order,"resize Escape preserves saved geometry and order");
    f.dock->requestAction(DockIntentKind::Raise,"ai");
    check(f.events.size()==1&&f.events.back().kind==DockIntentKind::Raise,"explicit Raise remains available with raise-on-press disabled");
    f.state.placements[3].order=42;++f.state.revision;f.state.ackRequest=f.events.back().request;f.publish();
    check(f.dock->snapshot().placements[3].order==42&&f.dock->children().back()==f.items[3],"accepted explicit Raise updates paint order");
    f.state.nodes={{"t","terminal"},{"s","sftp"},{"root","","t","s",DockAxis::Vertical,.5f}};f.state.root="root";
    f.state.placements[2]={"monitor","floating",before.bounds,true,2};++f.state.revision;f.publish();
    auto count=f.events.size();f.root.onMouseDown({corner});f.root.onMouseUp({{corner.x-10,corner.y-10}});
    check(f.events.size()==count+1&&f.events.back().source=="monitor","pinned overlapping tool consumes the edge; lower AI never receives it");
}
void previewMotion() {
    auto setup=[](Fixture& f) {
        now=0;f.dock->setMotionEnabled(true);f.begin(0);
        auto r=f.items[2]->frame();f.root.onMouseMove({{r.x+r.width-2,r.y+80}});
        return Rect{r.x+r.width*.5f,r.y,r.width*.5f,r.height};
    };
    Fixture f;auto from=f.items[0]->frame(),target=setup(f);
    now=16;f.dock->tickAnimations(now);auto mid=*f.dock->previewRect();
    check(mid.x>from.x&&mid.x<target.x,"preview advances without overshoot");
    auto origin=f.title(0);f.root.onMouseMove({{origin.x,origin.y}});
    now=32;f.dock->tickAnimations(now);auto reverse=*f.dock->previewRect();
    check(reverse.x>from.x&&reverse.x<mid.x,"preview retargets from current display");
    auto r=f.items[2]->frame();f.root.onMouseMove({{r.x+r.width-2,r.y+80}});
    for(int i=0;i<31;++i){now+=16;f.dock->tickAnimations(now);}
    auto settled=*f.dock->previewRect();
    check(settled.x==target.x&&settled.y==target.y&&settled.width==target.width&&settled.height==target.height,"old preview contract settles exactly in thirty frames");
    Fixture sample;setup(sample);now=8;sample.dock->tickAnimations(now);now=16;sample.dock->tickAnimations(now);
    check(std::abs(sample.dock->previewRect()->x-mid.x)<.001f,"two 8ms steps equal one 16ms step");
    f.root.onMouseMove({{origin.x,origin.y}});f.dock->setMotionEnabled(false);
    check(f.dock->previewRect()->x==from.x,"reduced motion snaps immediately");
    f.presentation.previewKeepText="Custom keep and cancel";f.publish();
    test_support::RecordingCanvas painted;f.dock->paint(painted);
    check(!painted.texts.empty()&&painted.texts.back().text==L"Custom keep and cancel","localized caption is actually painted");
    f.root.onMouseUp({{origin.x,origin.y}});check(!f.dock->previewRect(),"release clears feedback immediately");
    Fixture reduced;auto reducedTarget=setup(reduced);setPlatformMotionEnabled(false);reduced.dock->tickAnimations(now);
    check(reduced.dock->previewRect()->x==reducedTarget.x,"system reduced-motion preference snaps an active preview");
    setPlatformMotionEnabled(true);
}
}
int main(){try{internal::ScopedUiClock clock(clockNow);geometry();drag();dropPolicy();floatingResize();dividerAndActions();transactionalPointerAndPreview();previewMotion();presentationTransaction();headerGeometry();scopedPartsAndFloatingSurface();std::cout<<"dock workspace: 10 scenario groups passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
