#include "oneui/layout/pane_grid.h"
#include "oneui/layout/workspace_drag.h"
#include "oneui/controls/pane_header.h"
#include "oneui/controls/drag_handle.h"
#include "oneui/controls/icon_button.h"
#include "internal/ui_clock.h"
#include "../support/recording_canvas.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace oneui;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool same(Color a,Color b){return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;}
double now=0;
double clockNow(){return now;}
struct Fixture {
    View root;
    std::shared_ptr<PaneGrid> grid=std::make_shared<PaneGrid>();
    std::vector<std::shared_ptr<PaneView>> panes;
    PaneGridSnapshot state;
    std::vector<WorkspaceIntent> events;
    Fixture() {
        root.setFrame({0,0,1000,650});root.add(grid);grid->setFrame({0,50,1000,600});
        grid->setMotionEnabled(false);grid->setAnimationScheduler([]{});
        for(auto key:{"a","b","c"}){auto p=std::make_shared<PaneView>();p->key=key;panes.push_back(p);}
        state.nodes={{"a","a"},{"b","b"},{"c","c"},{"split","","a","b",DockAxis::Horizontal,.5f}};
        state.groups={{"main","split"},{"other","c"}};state.activeGroup="main";state.activePane="a";
        grid->setSnapshot(state,panes);grid->setOnIntent([this](auto e){events.push_back(e);});
    }
};
void header() {
    View root;root.setFrame({0,0,600,100});auto h=std::make_shared<PaneHeader>();root.add(h);
    PaneHeaderConfig config;config.pane="p";config.title=L"title";h->setConfig(config);
    h->setActionLabel(PaneHeaderAction::Split,L"Localized split");
    check(h->children()[0]->accessibleName()==L"Localized split","localized split accessible name");
    h->setConfig(config);
    check(h->children()[0]->accessibleName()==L"Localized split","config changes retain action labels");
    test_support::RecordingCanvas canvas;
    IconButton splitIcon(IconSymbol::OutlineWorkbenchSplit);splitIcon.setFrame({0,0,24,24});splitIcon.paint(canvas);
    check(canvas.lines.size()>=5,"outline split paints its outline and central divider");
    h->setFrame({0,0,319,29});h->paint(canvas);check(h->compact(),"319 compact");
    check(!h->children()[0]->visible()&&!h->children()[1]->visible(),"compact hides split/max");
    h->setFrame({0,0,320,29});h->paint(canvas);check(!h->compact()&&h->children()[0]->visible(),"320 full header");
    h->setFrame({0,0,600,29});h->paint(canvas);std::vector<PaneHeaderAction> actions;std::vector<int> phases;
    h->setOnIntent([&](auto a,auto){actions.push_back(a);});h->setOnDrag([&](int p,auto){phases.push_back(p);});
    root.onMouseDown({{100,14},MouseButton::Left});root.onMouseMove({{107.9f,14},MouseButton::Left});
    check(phases.empty(),"body below eight pixels");root.onMouseMove({{108,14},MouseButton::Left});
    check(phases.size()==2&&phases[0]==0,"body starts at eight");
    root.onMouseUp({{250,14},MouseButton::Left});check(phases.back()==2,"root capture releases");
    check(actions.size()==1&&actions[0]==PaneHeaderAction::Activate,"body activation only");
    phases.clear();actions.clear();
    auto close=h->actionFrame(PaneHeaderAction::Close);Point p{close.x+12,close.y+12};
    root.onMouseDown({p,MouseButton::Left});root.onMouseUp({p,MouseButton::Left});
    check(phases.empty()&&actions.size()==1&&actions[0]==PaneHeaderAction::Close,"close never arms drag");
    actions.clear();
    auto more=h->children()[6]->frame();Point morePoint{more.x+12,more.y+12};
    check(h->children()[2]->frame().x<h->children()[0]->frame().x &&
          h->children()[0]->frame().x<h->children()[1]->frame().x &&
          h->children()[1]->frame().x<more.x && more.x<close.x,"reference button order");
    h->setActionLabel(PaneHeaderAction::Menu,L"Localized actions");
    check(h->children()[5]->accessibleName()==L"Localized actions" &&
          h->children()[6]->accessibleName()==L"Localized actions","grip and more both localized");
    root.onMouseDown({morePoint,MouseButton::Left});root.onMouseUp({morePoint,MouseButton::Left});
    check(actions.size()==1 && actions[0]==PaneHeaderAction::Menu && phases.empty(),"more button opens menu without drag");
    auto sheet=std::make_shared<StyleSheet>();std::string error;
    check(sheet->addRulesFromCss("pane-header-part.pane-header-path {color:#123456;font-size:13px;font-family:Consolas;} pane-header-part.pane-header-number {background:#112233;border-radius:5px;}",&error),"header part CSS");
    h->setStyleScope(sheet);config.path=L"/tmp";h->setConfig(config);
    test_support::RecordingCanvas styled;h->paint(styled);
    bool pathStyled=false,badgeStyled=false;
    for(auto& text:styled.textBlocks)if(text.text==L"/tmp")pathStyled=same(text.color,Color{0x12,0x34,0x56}) && text.style.fontFamily==L"Consolas";
    for(auto& box:styled.fillRects)if(box.rect.width==17 && box.rect.height==17)badgeStyled=box.radius==5 && same(box.color,Color{0x11,0x22,0x33});
    check(pathStyled && badgeStyled,"path font/color and rounded badge consume authored styles");
    auto next=std::make_shared<StyleSheet>();next->addRulesFromCss("pane-header-part.pane-header-path {color:#abcdef;}",&error);
    h->setStyleScope(next);test_support::RecordingCanvas restyled;h->paint(restyled);
    check(std::any_of(restyled.textBlocks.begin(),restyled.textBlocks.end(),[](auto& text){return text.text==L"/tmp" && same(text.color,Color{0xab,0xcd,0xef});}),"header responds to theme replacement");
    root.onMouseDown({{12,14},MouseButton::Left});root.onMouseMove({{15.9f,14},MouseButton::Left});
    check(phases.empty(),"grip below four");root.onMouseMove({{16,14},MouseButton::Left});
    check(phases[0]==0,"grip starts at four");root.clearInteractionState();check(phases.back()==3,"capture loss cancels");
    auto control=std::make_shared<DragHandle>();control->threshold=7;control->setFrame({0,40,100,29});root.add(control);
    phases.clear();control->setOnDrag([&](int p,auto){phases.push_back(p);});
    root.onMouseDown({{10,50},MouseButton::Left});root.onMouseMove({{17,50},MouseButton::Left});
    KeyEvent escape;escape.key=Key::Escape;root.onKeyDown(escape);check(phases.back()==3,"handle Esc through root");
}
void drag() {
    Fixture f;WorkspaceDragDomain domain;domain.registerGrid(f.grid);
    auto b=f.panes[1]->frame();auto point=[&](float x,float y){return Point{b.x+b.width*x,b.y+b.height*y};};
    check(f.grid->dropTarget(point(.28f,.5f)).second=="left",".28 boundary");
    check(f.grid->dropTarget(point(.72f,.5f)).second=="right",".72 boundary");
    check(f.grid->dropTarget(point(.5f,.5f)).second=="center","central swap");
    check(f.grid->dropTarget(point(.5f,.5f),true).second=="left","tab center tie left");
    check(domain.begin("a",{100,100}),"begin pane");domain.release(point(.9f,.5f));
    check(f.events.size()==1&&f.events.back().edge=="right","release re-hits final point");
    check(f.grid->previewRect().has_value(),"pending preview waits for authority");
    f.state.ackRequest=f.events.back().request;f.state.ackAccepted=false;f.grid->setSnapshot(f.state,f.panes);
    check(!f.grid->previewRect(),"rejected request removes preview");
    domain.begin("a",{100,100});++f.state.revision;f.grid->setSnapshot(f.state,f.panes);domain.release(point(.9f,.5f));
    check(f.events.size()==1&&!domain.active(),"stale drag emits nothing");
    check(!domain.begin("a",{100,100},true),"multi leaf tab cannot detach");
    check(domain.begin("c",{100,100},true),"single leaf inactive tab can detach");domain.release(point(.5f,.5f));
    check(f.events.size()==2&&f.events.back().edge=="left","tab never swaps centrally");
    auto strip=std::make_shared<View>();strip->setFrame({0,0,1000,40});domain.registerTabs("strip",strip);
    check(domain.begin("a",{100,100}),"pane can detach");domain.release({200,20});
    check(f.events.back().edge=="tab-strip"&&f.events.back().target=="strip","pane to tab strip");
    domain.begin("a",{100,100});domain.release({100,100});check(f.events.size()==3,"self release no request");
    WorkspaceDragDomain isolated;check(!isolated.begin("a",{100,100}),"other view/domain cannot see source");
}
void dividerStyles() {
    Fixture f;auto sheet=std::make_shared<StyleSheet>();std::string error;
    check(sheet->addRulesFromCss("pane-grid-part.pane-grid-divider {background:#112233;} pane-grid-part.pane-grid-divider:hover {background:#445566;}",&error),"divider CSS");
    f.grid->setStyleScope(sheet);
    auto color=[&] {test_support::RecordingCanvas c;f.grid->paint(c);check(!c.fillRects.empty(),"divider paint");return c.fillRects.back().color;};
    check(same(color(),{0x11,0x22,0x33}),"normal divider uses authored token");
    auto pane=f.panes[0]->frame();Point hit{pane.x+pane.width+2.5f,pane.y+40};
    f.grid->onMouseMove({hit});check(same(color(),{0x44,0x55,0x66}),"divider hover follows pointer");
    f.grid->onMouseMove({{pane.x+20,pane.y+40}});check(same(color(),{0x11,0x22,0x33}),"leaving divider clears hover");
    f.grid->onMouseMove({hit});f.grid->clearInteractionState();check(same(color(),{0x11,0x22,0x33}),"capture loss clears divider hover");
    auto next=std::make_shared<StyleSheet>();next->addRulesFromCss("pane-grid-part.pane-grid-divider {background:#aabbcc;}",&error);
    f.grid->setStyleScope(next);check(same(color(),{0xaa,0xbb,0xcc}),"divider responds to theme replacement");
}
void navigation() {
    Fixture f;
    f.grid->navigate("left");check(f.events.empty(),"no neighbor");
    f.grid->navigate("right");check(f.events.back().target=="b","nearest right neighbor");
    f.grid->navigate("right",true);check(f.events.back().kind==WorkspaceIntentKind::Swap,"swap is semantic");
    f.events.clear();f.grid->setOnIntent([&](auto e){f.events.push_back(e);f.state.activePane=e.target;f.grid->setSnapshot(f.state,f.panes);});
    f.grid->queueNavigation(1,"right",false);f.grid->queueNavigation(2,"left",false);f.grid->tickAnimations(now);
    check(f.events.size()==2&&f.events.back().target=="a","same frame navigation sees accepted prior state");
    f.events.clear();for(int i=3;i<13;++i)f.grid->queueNavigation(i,i%2?"right":"left",false);
    f.grid->tickAnimations(now);check(f.events.size()==8&&f.grid->navigationDropped()==2,"bounded eight queue with diagnostics");
    f.state.maximized="a";f.grid->setSnapshot(f.state,f.panes);f.grid->navigate("right");check(f.events.size()==8,"maximize suppresses navigation");
}
void keyboardAcceptance() {
    Fixture f;f.grid->onFocusChanged(true);
    f.grid->setOnIntent([&](auto e){if(e.kind==WorkspaceIntentKind::Ratio){f.state.nodes[3].ratio=float(e.value);f.state.ackRequest=e.request;++f.state.revision;f.grid->setSnapshot(f.state,f.panes);}});
    KeyEvent key;key.key=Key::Right;now=100;f.grid->onKeyDown(key);
    check(f.grid->resizeHeld(),"synchronous accepted key holds resize");now=150;f.grid->onKeyDown(key);
    f.grid->tickAnimations(269);check(f.grid->resizeHeld(),"hold through keyboard idle threshold");
    f.grid->tickAnimations(270);check(!f.grid->resizeHeld(),"keyboard settles once at 120ms");
}
void preview() {
    Fixture f;f.grid->setMotionEnabled(true);f.grid->showPreview({0,0,100,100},true);
    f.grid->tickAnimations(0);f.grid->showPreview({100,0,100,100},true);f.grid->tickAnimations(16);
    auto x=f.grid->previewRect()->x;check(x>0&&x<100,"preview eases without teleport");
    f.grid->clearPreview();check(!f.grid->previewRect(),"preview cancellation");
    f.grid->setMotionEnabled(false);f.grid->showPreview({100,0,100,100},true);
    check(f.grid->previewRect()->x==100,"reduced motion snaps");
    auto sample=[](double step) {
        Fixture sample;sample.grid->setMotionEnabled(true);
        sample.grid->showPreview({0,0,100,100},true);sample.grid->tickAnimations(100);
        sample.grid->showPreview({100,0,100,100},true);
        for(double t=step;t<200;t+=step)sample.grid->tickAnimations(100+t);
        sample.grid->tickAnimations(300);return sample.grid->previewRect()->x;
    };
    check(std::abs(sample(8)-sample(16))<.01f && std::abs(sample(16)-sample(50))<.01f,"preview trajectory invariant across 8/16/50 ms steps");
}
}
int main(){try {internal::ScopedUiClock clock(clockNow);header();drag();dividerStyles();navigation();keyboardAcceptance();preview();
    std::cout<<"PASS workspace interactions: 6 groups\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
