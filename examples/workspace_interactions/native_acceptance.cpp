// Example-local test bridge. These symbols are linked into the example executable
// only, never exported by oneui.dll or declared in the frozen C ABI.
#include "capi/oneui_c_api_widget_internal.h"
#include "oneui/view.h"
#include "oneui/layout/panel.h"
#include "oneui/layout/pane_grid.h"
#include "oneui/controls/pane_header.h"
#include "oneui/controls/terminal_view.h"
#include "internal/workspace_frame_probe.h"
#include <chrono>
#include <cstring>
using namespace oneui;
namespace {
std::shared_ptr<View> root;
std::shared_ptr<PaneGrid> grid;
std::vector<std::shared_ptr<Widget>> walk() {
    std::vector<std::shared_ptr<Widget>> result{root};
    for(size_t i=0;i<result.size();++i)if(auto view=std::dynamic_pointer_cast<View>(result[i]))
        for(auto& child:view->children())result.push_back(child);
    return result;
}
double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
}
extern "C" {
void workspace_acceptance_start(OneUiWidget* widget,void(*callback)(double,double,double,double,double,int,int)) {
    root=std::dynamic_pointer_cast<View>(widget->widget);
    for(auto& w:walk())if(auto g=std::dynamic_pointer_cast<PaneGrid>(w)){grid=g;break;}
    internal::setWorkspaceFrameProbe([callback](auto timing){
        callback(now(),timing.paintMs-timing.presentMs+timing.updateMs,timing.presentMs,timing.updateMs,timing.censusMs,
                 grid?int(grid->motionPhase()):-1,grid?int(grid->visiblePanes().size()):0);
    });
}
void workspace_acceptance_stop(){internal::setWorkspaceFrameProbe({});grid.reset();root.reset();}
int workspace_acceptance_phase(){return grid?int(grid->motionPhase()):-1;}
void workspace_acceptance_redraw(){if(auto panel=std::dynamic_pointer_cast<Panel>(root))panel->setBackground({0,0,0,0});}
int workspace_acceptance_rect(const char* name,int kind,float* rect) {
    if(!root)return 0;
    for(auto& w:walk()) {
        bool match=false;
        if(kind==0)match=w->accessibleName()==std::wstring(name,name+std::strlen(name));
        if(kind==1)if(auto pane=std::dynamic_pointer_cast<PaneHeader>(w))match=pane->config().pane==name;
        if(kind==2)if(auto pane=std::dynamic_pointer_cast<PaneView>(w))match=pane->key==name;
        if(match&&w->visible()) {auto r=w->frame();rect[0]=r.x;rect[1]=r.y;rect[2]=r.width;rect[3]=r.height;return r.width>0&&r.height>0;}
    }return 0;
}
int workspace_acceptance_pointer(int phase,float x,float y,int button,int alt,int clicks) {
    MouseEvent e{{x,y},MouseButton(button),false,false,alt!=0,clicks};
    return phase==0?root->onMouseDown(e):phase==1?root->onMouseMove(e):root->onMouseUp(e);
}
int workspace_acceptance_key(int key,int control,int shift) {
    KeyEvent e;e.key=Key(key);e.control=control!=0;e.shift=shift!=0;return root->onKeyDown(e);
}
void workspace_acceptance_text(int composition){
    // Establish focus through the native root, then route IME/text through the
    // same root. No terminal callback is invoked directly by the test.
    for(auto& widget:walk())if(auto terminal=std::dynamic_pointer_cast<TerminalView>(widget))
        if(terminal->frame().width>0&&root->requestFocus(terminal.get(),false))break;
    if(composition)root->setTextComposition(L"本地输入",4);
    else root->onTextInputText(L"echo local\n");
}
uintptr_t workspace_acceptance_address(OneUiWidget* widget){return reinterpret_cast<uintptr_t>(widget->widget.get());}
void workspace_acceptance_preview(OneUiWidget* widget) {
    root=std::dynamic_pointer_cast<View>(widget->widget);
    std::shared_ptr<PaneHeader> source;
    std::shared_ptr<PaneView> target;
    for(auto& w:walk()) {
        if(auto h=std::dynamic_pointer_cast<PaneHeader>(w))
            if(h->config().pane=="session/0")source=h;
        if(auto p=std::dynamic_pointer_cast<PaneView>(w))
            if(p->key=="session/1")target=p;
    }
    if(!source||!target)return;
    auto a=source->frame(),b=target->frame();
    root->onMouseDown({{a.x+60,a.y+14},MouseButton::Left});
    root->onMouseMove({{a.x+70,a.y+14},MouseButton::Left});
    root->onMouseMove({{b.x+b.width*.15f,b.y+b.height*.5f},MouseButton::Left});
}
}
