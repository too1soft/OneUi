#include "oneui/layout/pane_grid.h"
#include "oneui/controls/terminal_view.h"
#include "oneui/ui_native_host.h"
#include "internal/ui_clock.h"
#include "../support/recording_canvas.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
using namespace oneui;
namespace { thread_local long allocationFailure=-1; }
void* operator new(size_t size) {
    if(allocationFailure>=0 && allocationFailure--==0){allocationFailure=-1;throw std::bad_alloc();}
    if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
bool near(float a,float b){return std::abs(a-b)<.01f;}
double now=0;
double timeNow(){return now;}
struct Fixture {
    PaneGrid grid;
    std::vector<std::shared_ptr<PaneView>> panes;
    std::vector<std::shared_ptr<TerminalView>> terminals;
    std::vector<int> resized;
    std::vector<WorkspaceIntent> events;
    PaneGridSnapshot snapshot;
    Fixture(int count=2) {
        grid.setFrame({0,0,1000,600});grid.setAnimationScheduler([]{});
        resized.resize(count);
        for(int i=0;i<count;++i) {
            auto pane=std::make_shared<PaneView>();pane->key=std::to_string(i);
            auto terminal=std::make_shared<TerminalView>();terminal->setCursorBlinking(false);
            terminal->setGrid(24,80,std::vector<TerminalCell>(24*80));
            terminal->setOnViewportChanged([this,i](auto){++resized[i];});
            auto host=std::make_shared<ui::NativeHostView>(terminal);host->setSettledViewport(true);
            pane->add(host);panes.push_back(pane);terminals.push_back(terminal);
        }
        grid.setOnIntent([this](auto e){events.push_back(e);});
        snapshot.nodes={{"a","0"}};snapshot.groups={{"g","a"}};snapshot.activeGroup="g";snapshot.activePane="0";
        grid.setSnapshot(snapshot,panes);paint();grid.tickAnimations(now);
        resized.assign(count,0);events.clear();
    }
    void paint(){test_support::RecordingCanvas canvas;grid.paint(canvas);}
    void split() {
        snapshot.nodes.push_back({"b","1"});
        snapshot.nodes.push_back({"s","","a","b",DockAxis::Horizontal,.5f});
        snapshot.groups[0].root="s";++snapshot.revision;grid.setSnapshot(snapshot,panes);
    }
    void frame(double t){now=t;grid.tickAnimations(now);paint();}
    void finish() {frame(now+1);frame(now+1);frame(now+201);frame(now+1);}
};
void motion() {
    Fixture f;auto address=f.terminals[0].get();f.split();
    check(f.grid.resizeHeld(),"split holds viewport before first paint");
    check(f.grid.motionPhase()==PaneMotionPhase::Preparing,"preparing");
    f.frame(10);check(f.grid.motionPhase()==PaneMotionPhase::WarmFrame,"one warm frame");
    f.frame(20);check(f.grid.motionPhase()==PaneMotionPhase::Running,"clock starts after warm");
    f.frame(120);
    auto generation=f.grid.motionGeneration();f.grid.setSnapshot(f.snapshot,f.panes);
    check(f.grid.motionPhase()==PaneMotionPhase::Running && f.grid.motionGeneration()==generation,"unchanged authority does not restart motion");
    check(near(f.panes[0]->frame().width,995*(1-.5f*.875f)),"cubic ease, half time");
    check(f.resized[0]==0 && f.resized[1]==0,"no intermediate terminal resize");
    f.frame(220);f.frame(221);
    check(!f.grid.resizeHeld(),"hold released at final geometry");
    if(f.resized[0]!=1||f.resized[1]!=1)std::cerr<<"resize counts "<<f.resized[0]<<','<<f.resized[1]<<'\n';
    check(f.resized[0]==1 && f.resized[1]==1,"one final resize per changed terminal");
    check(address==f.terminals[0].get(),"existing content never rebuilt");
    int settled=0;for(auto& e:f.events)settled+=e.kind==WorkspaceIntentKind::Resize;
    check(settled==2,"measured semantic viewport events queued after paint");
    f.snapshot.maximized="1";f.grid.setSnapshot(f.snapshot,f.panes);f.finish();
    check(near(f.snapshot.nodes[2].ratio,.5f),"maximize keeps saved ratio");
    check(!f.panes[0]->visible() && f.panes[1]->visible(),"maximize hides sibling after motion");
    check(near(f.panes[1]->frame().x,0) && near(f.panes[1]->frame().width,1000),"maximize consumes divider gap");
    f.snapshot.maximized.clear();f.grid.setSnapshot(f.snapshot,f.panes);f.finish();
    check(near(f.panes[0]->frame().width,497.5f),"restore saved ratio");
}
void redirect() {
    Fixture f;f.split();f.frame(1000);f.frame(1010);f.frame(1110);
    auto at=f.panes[0]->frame().width;
    f.snapshot.nodes[2].ratio=.7f;++f.snapshot.revision;f.grid.setSnapshot(f.snapshot,f.panes);
    check(near(at,f.panes[0]->frame().width),"redirect begins at current geometry");
    check(f.resized[0]==0,"redirect does not release shared hold");
    f.finish();check(f.resized[0]==1 && near(f.panes[0]->frame().width,696.5f),"latest epoch settles once");
    auto before=f.grid.snapshot().revision;
    f.snapshot.nodes[2].first="s";
    try {f.grid.setSnapshot(f.snapshot,f.panes);check(false,"cycle accepted");}catch(const std::invalid_argument&){}
    check(f.grid.snapshot().revision==before && near(f.panes[0]->frame().width,696.5f),"invalid candidate keeps scene");
}
void nestedMaximize() {
    for(bool motion:{false,true}) {
        Fixture f(3);f.grid.setMotionEnabled(motion);f.split();f.finish();
        f.snapshot.nodes.push_back({"c","2"});
        f.snapshot.nodes.push_back({"v","","b","c",DockAxis::Vertical,.4f});
        f.snapshot.nodes[2].second="v";
        f.grid.setSnapshot(f.snapshot,f.panes);f.finish();
        for(int pane=0;pane<3;++pane) {
            auto terminal=f.terminals[pane].get();
            f.snapshot.maximized=std::to_string(pane);f.grid.setSnapshot(f.snapshot,f.panes);f.finish();
            auto rect=f.panes[pane]->frame();
            check(near(rect.x,0)&&near(rect.y,0)&&near(rect.width,1000)&&near(rect.height,600),"nested maximized pane fills grid on both axes");
            check(f.terminals[pane].get()==terminal,"maximize retains terminal");
            f.snapshot.maximized.clear();f.grid.setSnapshot(f.snapshot,f.panes);f.finish();
            check(near(f.panes[0]->frame().width,497.5f)&&near(f.panes[1]->frame().height,238),"restore recovers both divider gaps and ratios");
        }
    }
    Fixture f;f.split();f.finish();
    f.snapshot.maximized="1";f.grid.setSnapshot(f.snapshot,f.panes);
    f.frame(now+1);f.frame(now+1);f.frame(now+100);
    auto before=f.panes[1]->frame();
    f.snapshot.maximized.clear();f.grid.setSnapshot(f.snapshot,f.panes);
    auto after=f.panes[1]->frame();
    check(near(before.x,after.x)&&near(before.width,after.width),"redirect retains interpolated gap without a jump");
    f.finish();check(near(f.panes[1]->frame().x,502.5f),"redirect restores divider");
}
void paintBeforeFirstTick() {
    now=3900;Fixture f;f.split();f.paint();
    check(f.grid.motionPhase()==PaneMotionPhase::WarmFrame,"first completed paint is the single warm frame");
    check(f.resized[0]==0&&f.resized[1]==0,"paint-first warm holds both viewports");
    now=4000;f.paint();check(f.grid.motionPhase()==PaneMotionPhase::Running,"second paint without tick starts clock without second warm frame");
    f.frame(4100);check(near(f.panes[0]->frame().width,995*(1-.5f*.875f)),"paint-first ordering retains full cubic duration");
    f.frame(4200);f.frame(4201);check(f.resized[0]==1&&f.resized[1]==1,"paint-first final resize once");
}
void divider() {
    Fixture f;f.grid.setMotionEnabled(false);f.split();f.paint();f.frame(now+1);f.resized.assign(2,0);
    check(f.grid.onMouseDown({{500,300},MouseButton::Left}),"divider hit");
    f.grid.onMouseMove({{650,300},MouseButton::Left});f.paint();
    check(f.resized[0]==0,"drag holds dimensions");
    f.frame(now+119);check(f.resized[0]==0,"no settle before 120ms");
    f.frame(now+1);check(f.resized[0]==1,"idle epoch settles at 120ms");
    f.grid.onMouseMove({{700,300},MouseButton::Left});f.paint();
    f.grid.onMouseUp({{750,300},MouseButton::Left});f.paint();f.frame(now+1);
    check(near(f.panes[0]->frame().width,747.5f),"release uses latest point");
    check(f.resized[0]==2,"second idle/release epoch once");
    f.grid.onMouseDown({{750,300},MouseButton::Left});
    f.grid.onMouseMove({{600,300},MouseButton::Left});
    f.grid.clearInteractionState();f.paint();
    check(near(f.panes[0]->frame().width,747.5f),"capture loss restores begin ratio");
    check(f.resized[0]==2,"cancel before settle causes no duplicate resize");
}
void delayedContent() {
    Fixture f;auto host=std::dynamic_pointer_cast<ui::NativeHostView>(f.panes[1]->children()[0]);
    host->detach();f.split();f.frame(2000);f.frame(2200);
    check(f.grid.motionPhase()==PaneMotionPhase::Preparing && f.resized[0]==0,"missing content keeps preparation hold");
    host->attach(f.terminals[1],{});f.finish();
    check(f.grid.motionPhase()==PaneMotionPhase::Idle && f.resized[0]==1,"late attachment resumes and settles");
}
void closingAndKeyboard() {
    Fixture f;f.grid.setMotionEnabled(false);f.split();f.paint();f.frame(now+1);
    f.grid.onFocusChanged(true);
    KeyEvent key;key.key=Key::Right;f.grid.onKeyDown(key);
    check(near(f.panes[0]->frame().width,995*.525f),"keyboard divider .025 step");
    key.control=true;f.grid.onKeyDown(key);check(near(f.panes[0]->frame().width,995*.525f),"modifier does not resize");
    key.control=false;key.key=Key::Home;f.grid.onKeyDown(key);
    check(near(f.panes[0]->frame().width,220),"Home clamps minimum");
    f.panes[1]->closing=true;f.panes[1]->closeRequest=91;
    f.grid.setSnapshot(f.snapshot,f.panes);f.frame(now+1);f.frame(now+1);
    auto count=[&]{return std::count_if(f.events.begin(),f.events.end(),[](auto& e){return e.kind==WorkspaceIntentKind::Transition && e.request==91;});};
    check(count()==1,"reduced motion still publishes close completion");
    f.grid.setSnapshot(f.snapshot,f.panes);f.frame(now+1);check(count()==1,"close completion idempotent");
}
void allocationRollback() {
    Fixture f;auto snapshot=f.snapshot;
    snapshot.nodes={{"a","0"},{"b","1"},{"s","","a","b",DockAxis::Horizontal,.5f}};
    snapshot.groups[0].root="s";
    int failures=0;
    for(int limit=0;limit<512;++limit) {
        allocationFailure=limit;
        try {
            auto publish=f.grid.prepare(snapshot,f.panes);allocationFailure=-1;
            // The complete publication path must allocate no storage.
            allocationFailure=0;publish();allocationFailure=-1;break;
        } catch(const std::bad_alloc&) {
            allocationFailure=-1;++failures;
            check(f.grid.snapshot().nodes.size()==1,"allocation failure leaves published tree");
            check(!f.grid.resizeHeld(),"allocation failure leaves old resize policy");
        }
    }
    check(failures>10 && f.grid.snapshot().nodes.size()==3,"all prepare allocation sites exercised");
}
void idleAndDividerBoundaries() {
    Fixture f;f.grid.setMotionEnabled(false);f.split();f.paint();f.frame(now+1);
    // 5 px visible gap centred at 500, expanded to a 10 px hit region.
    check(f.grid.cursor({495,300})==CursorKind::ResizeHorizontal,"left hit boundary");
    check(f.grid.cursor({494.99f,300})!=CursorKind::ResizeHorizontal,"outside left boundary belongs to body");
    check(f.grid.cursor({504.99f,300})==CursorKind::ResizeHorizontal,"inside right boundary");
    f.grid.onFocusChanged(true);KeyEvent key;key.key=Key::End;f.grid.onKeyDown(key);
    check(near(f.panes[1]->frame().width,220),"End preserves second minimum");
    key.key=Key::Escape;f.grid.onKeyDown(key);f.paint();f.frame(now+1);
    check(near(f.panes[0]->frame().width,497.5f),"keyboard Esc restores begin geometry");
    MouseEvent doubleClick{{500,300},MouseButton::Left};doubleClick.clickCount=2;
    f.grid.onMouseDown(doubleClick);check(f.events.back().kind==WorkspaceIntentKind::Ratio&&near(float(f.events.back().value),.5f),"double click requests exactly half");
    f.paint();f.frame(now+1);int requested=0;f.grid.setAnimationScheduler([&]{++requested;});
    auto end=now+500;for(double t=now+16;t<=end;t+=16)check(!f.grid.tickAnimations(t),"settled workspace has no active animation");
    check(requested==0,"500ms idle schedules no animation frames with cursor/output stopped");
}
}
int main(){try{internal::ScopedUiClock clock(timeNow);motion();paintBeforeFirstTick();redirect();nestedMaximize();divider();delayedContent();closingAndKeyboard();allocationRollback();idleAndDividerBoundaries();std::cout<<"Pane motion: 9 groups passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
