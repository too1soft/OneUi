#include "oneui/controls/window_title_bar.h"
#include "oneui/controls/button.h"
#include "oneui/oneui_c_api.h"
#include "support/recording_canvas.h"
#include <cmath>
#include <iostream>
#include <limits>
#ifdef _WIN32
#include "oneui/platform/window.h"
#include <windows.h>
#endif

int main() {
    using namespace oneui;
    int failures=0;
    const auto check=[&](bool ok,const char* name){if(!ok){++failures;std::cerr<<name<<'\n';}};
    WindowTitleBar bar(L"Native tool"); bar.setFrame({100,50,600,40});
    auto tools=std::make_shared<Button>(L"Tools");bar.setAccessory(tools);
    TitleBarPresentation p;
    p.title={110,0,250,40};p.logo=p.logoIcon={90,10,18,18};p.accessory={370,3,210,34};
    p.titleFontSize=13;p.hideLogoBorder=true;
    for(size_t i=0;i<3;++i){auto& b=p.buttons[i];b.frame={float(8+i*26),5,24,28};b.visual={float(14+i*26),13,12,12};b.icon=b.visual;
        b.ellipse=true;b.fill={40,100,160,255};b.pressedFill={40,100,160,180};b.glyphOnGroupHover=true;
        b.glyph.moveTo({2,8});b.glyph.lineTo({14,8});}
    check(bar.setPresentation(&p),"accept finite local geometry");
    int actions[3]={};bar.setOnMinimize([&]{++actions[0];});bar.setOnMaximize([&]{++actions[1];});bar.setOnClose([&]{++actions[2];});
    test_support::RecordingCanvas canvas;bar.paint(canvas);
    check(tools->frame().x==470 && tools->frame().y==53,"accessory uses local origin");
    size_t discs=0;for(const auto& call:canvas.fillEllipses)if(call.color.r==40 && call.color.g==100 && call.color.b==160)++discs;
    check(discs==3,"custom ellipse paint");
    for(size_t i=0;i<3;++i){Point at{120+float(i*26),68};check(bar.hitsWindowButton(at),"paint and hit agree");bar.onMouseDown({at});bar.onMouseUp({at});check(actions[i]==1,"semantic action independent of visual placement");}
    check(!bar.hitsWindowButton({680,68}),"no phantom default buttons");
    const auto before=bar.windowButtonFrame(TitleBarButtonId::Close);
    bar.setVariant("any-css-name");check(bar.windowButtonFrame(TitleBarButtonId::Close).x==before.x,"CSS cannot select geometry");
    auto invalid=p;invalid.buttons[0].frame.width=-1;
    check(!bar.setPresentation(&invalid),"negative sizes rejected");
    invalid=p;invalid.logo.x=std::numeric_limits<float>::quiet_NaN();check(!bar.setPresentation(&invalid),"NaN rejected atomically");
    check(bar.windowButtonFrame(TitleBarButtonId::Close).x==before.x,"invalid input preserves presentation");
    // Source data is copied; changing it afterwards does not mutate hit testing.
    p.buttons[0].frame.x=400;check(bar.windowButtonFrame(TitleBarButtonId::Minimize).x==108,"owned copy");
    bar.onMouseDown({{172,68}});bar.onMouseUp({{120,68}});check(actions[2]==1,"cross release cancels");
    bar.onMouseDown({{172,68}});bar.setPresentation(&p);bar.onMouseUp({{172,68}});check(actions[2]==1,"layout change cancels press");
    bar.setDisabled(true);check(!bar.hitsWindowButton({172,68}),"disabled hit excluded");check(!bar.activateWindowButton(TitleBarButtonId::Close),"disabled action excluded");bar.setDisabled(false);
    check(bar.setPresentation(nullptr),"reset succeeds");check(bar.windowButtonFrame(TitleBarButtonId::Close).x!=before.x,"reset restores default");

    auto* raw=oneui_title_bar_create(L"ABI");
    OneUiTitleBarPresentation c{};c.struct_size=sizeof(c);c.title_font_size=12;
    check(oneui_title_bar_set_presentation(raw,&c)==1,"C ABI valid struct");
    c.struct_size=0;check(!oneui_title_bar_set_presentation(raw,&c),"C ABI size guard");c.struct_size=sizeof(c);
    c.buttons[0].glyph.count=1;check(!oneui_title_bar_set_presentation(raw,&c),"C ABI null path guard");
    OneUiCaptionPathCommand command{9,0,0};c.buttons[0].glyph.commands=&command;
    check(!oneui_title_bar_set_presentation(raw,&c),"C ABI verb guard");command.verb=0;c.buttons[0].glyph.count=65;
    check(!oneui_title_bar_set_presentation(raw,&c),"C ABI bounded path");c.buttons[0].glyph.count=1;
    check(oneui_title_bar_set_presentation(raw,&c)==1,"C ABI copied path");
    check(oneui_title_bar_set_presentation(raw,nullptr)==1,"C ABI reset");
    check(!oneui_title_bar_set_presentation(nullptr,&c),"C ABI invalid handle");oneui_widget_destroy(raw);
#ifdef _WIN32
    WindowOptions options;options.title=L"Caption contract fixture";options.width=600;options.height=300;options.borderless=true;
    auto window=Window::create(options);
    auto nativeBar=std::make_shared<WindowTitleBar>(L"Native fixture");
    // Independent geometry, including a left-side close control and drag lane.
    auto nativePresentation=p;nativePresentation.buttons[0].frame.x=8;
    nativeBar->setPresentation(&nativePresentation);window->setContent(nativeBar);
    window->setTitleBarDragMetrics(40,0);window->initialize();window->show();window->prepareLayoutSnapshot();
    const HWND hwnd=static_cast<HWND>(window->nativeHandle());
    const auto screenPoint=[&](float x,float y){POINT pt{LONG(x*window->dpiScale()),LONG(y*window->dpiScale())};ClientToScreen(hwnd,&pt);return MAKELPARAM(pt.x,pt.y);};
    check(SendMessageW(hwnd,WM_NCHITTEST,0,screenPoint(72,18))==HTCLIENT,"native left caption button stays client input");
    check(SendMessageW(hwnd,WM_NCHITTEST,0,screenPoint(300,18))==HTCAPTION,"empty caption remains native draggable");
    SendMessageW(hwnd,WM_NCLBUTTONDBLCLK,HTCAPTION,screenPoint(300,18));
    WindowPlacement placement;
    check(window->getWindowPlacement(placement) && placement.maximized,"native double click maximizes");
    SendMessageW(hwnd,WM_NCLBUTTONDBLCLK,HTCAPTION,screenPoint(300,18));
    check(window->getWindowPlacement(placement) && !placement.maximized,"native double click restores");
    window->close();
#endif
    if(!failures)std::cout<<"Title bar presentation: geometry, actions, CSS independence, cancellation, ownership, C ABI and native caption contracts passed\n";
    return failures?1:0;
}
