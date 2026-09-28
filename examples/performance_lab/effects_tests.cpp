#include "oneui/ui_compose.h"
#include "oneui/ui_theme.h"
#include "oneui/effect_transition.h"
#include "oneui/platform/window.h"
#include "support/recording_canvas.h"
#include "EffectsProbe.g.h"
#include <windows.h>
#include <GL/gl.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <limits>
using namespace oneui;
using namespace oneui::ui;
using oneui::test_support::RecordingCanvas;
#define CHECK(x) do{if(!(x))throw std::runtime_error("Failed at " + std::to_string(__LINE__) + ": " #x);}while(0)
static double nowMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static StyleBox box(const std::string& body){StyleSheet sheet;std::string error;CHECK(sheet.addRulesFromCss("stack {"+body+"}",&error));return sheet.resolve({"stack"});}
static void parserTests(){
    auto a=box("background: linear-gradient(90deg, #ff0000, #00ff00 30%, #0000ff, #ffffff);");
    const auto& stops=a.background.gradient->stops;CHECK(stops.size()==4);CHECK(stops[0].position==0);CHECK(stops[1].position==.3f);CHECK(std::abs(stops[2].position-.65f)<.0001f);CHECK(stops[3].position==1);
    auto r=box("background: radial-gradient(80% at 25% 30%, rgba(255,0,0,.5), #00ff00 50%, #0000ff);");CHECK(r.background.gradient->radial);CHECK(r.background.gradient->center.x==.25f);CHECK(r.background.gradient->radius==.8f);
    CHECK(box("background:linear-gradient(#ff0000 50%,#0000ff 50%);").background.gradient->stops.size()==2);
    for(auto value:{"linear-gradient(#ff0000)","linear-gradient(#ff0000 70%,#0000ff 20%)","linear-gradient(nandeg,#ff0000,#0000ff)","linear-gradient(#ff0000 -1%,#0000ff)","radial-gradient(0% at 50% 50%,#ff0000,#0000ff)","linear-gradient(#ff0000 20px,#0000ff)"}){StyleSheet bad;CHECK(!bad.addRulesFromCss(std::string("stack {background:")+value+";}"));}
    CHECK(!mergeStyleBox(a,box("background:#123456;")).background.gradient);CHECK(mergeStyleBox(box("background:#ffffff;"),a).background.gradient);
    auto shadow=box("box-shadow:0px 3px 8px #00000040,inset 1px 2px 5px 1px #00000080;");CHECK(shadow.shadows.size()==2);CHECK(shadow.shadows[1].inset);
    CHECK(mergeStyleBox(shadow,box("box-shadow:none;")).shadows.empty());
    for(auto value:{"0px 0px -1px #000000", "0px 1px nanpx #000000", "inset 1px #000000"}){StyleSheet bad;CHECK(!bad.addRulesFromCss(std::string("stack {box-shadow:")+value+";}"));}
    bool rejected=false;try{syntax::css("Switch {box-shadow:0px 0px 2px #000000;}");}catch(...){rejected=true;}CHECK(rejected);
    rejected=false;try{syntax::css("Select {background-color:LINEAR-GRADIENT(#ffffff,#000000);}");}catch(...){rejected=true;}CHECK(rejected);
    { Mount mount;bool rejected=false;try{mount.styles()->replace(syntax::css(":root {--g:linear-gradient(#ffffff,#000000);} Select {background-color:var(--g);}"));}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected); }
    RecordingCanvas canvas;shadow.background=a.background;paintStyleBox(canvas,{30,40,300,90},shadow);CHECK(canvas.gradients.size()==1);CHECK(canvas.boxShadows.size()==1);CHECK(canvas.insetShadows.size()==1);CHECK(canvas.strokeRects.empty());
    auto bounds=stylePaintBounds({30,40,300,90},shadow);CHECK(bounds.x<30 && bounds.y<40 && bounds.width>300 && bounds.height>90);
    CHECK(stylePaintBounds({30,40,300,90},box("box-shadow:inset 0px 0px 50px #000000;")).width==300);
    auto stack=std::make_shared<Stack>();stack->setFrame({100,100,300,90});Rect dirty{};stack->setRectInvalidator([&](Rect r){dirty=r;});stack->setStyleBox(shadow);CHECK(dirty.x<100);stack->clearStyleBox();CHECK(dirty.x<100 && dirty.width>300);
    stack->setStyleBox(shadow);auto middle=std::make_shared<View>();middle->setFrame({100,100,300,90});middle->add(stack);View parent;parent.add(middle);RecordingCanvas fringe;fringe.clipOverride=Rect{80,110,10,20};parent.paint(fringe);CHECK(fringe.boxShadows.size()==1);
}
struct VM {State<bool> open{true},reduced{false};State<std::wstring> preset{L"expand"},text{L"中文输入保留"};};
static void transitionTests(){
    setMotionEnabled(true);setPlatformMotionEnabled(true);
    ShadowTransition shadow;ControlShadowStyle a,b;a.color={0,0,0,120};a.blurRadius=8;b=a;b.inset=true;b.blurRadius=4;
    shadow.reset({a});shadow.animateTo({b},0,{100,EasingCurve::Linear});CHECK(shadow.running());shadow.tick(50);CHECK(shadow.value().size()==2);
    for(int i=0;i<1000;++i){shadow.animateTo({i%2?a:b},50+i,{100,EasingCurve::Linear});shadow.tick(51+i);CHECK(shadow.value().size()<=2);}
    shadow.tick(2000);CHECK(!shadow.running());CHECK(shadow.value().size()==1);shadow.animateTo({a},2000,{0});CHECK(!shadow.running());
    FloatTransition f;f.animateTo(1,0,{100});setMotionEnabled(false);f.tick(1);CHECK(!f.running() && f.value()==1);setMotionEnabled(true);
    for(bool compiled:{false,true}){
        VM vm;Mount mount;Compose ui(mount);Element root=compiled?build_EffectsProbe(vm,mount):Element(ui.reveal(ui.input(vm.text).ref("input")).open(vm.open).preset(vm.preset).reducedMotion(vm.reduced).ref("reveal"));
        auto reveal=root.as<Reveal>();auto input=std::dynamic_pointer_cast<TextField>(mount.find("input"));CHECK(reveal && input);
        root.widget->setFrame({0,0,320,44});int scheduled=0;root.widget->setAnimationScheduler([&]{++scheduled;});mount.styles()->replace(declarativeTheme()+styles_EffectsProbe());
        RecordingCanvas c;root.widget->paint(c);CHECK(reveal->requestFocus(input.get()));input->setCaretIndex(2);input->setSelectionRange(1,2);input->setTextComposition(L"中文",1);
        const auto subscriptions=mount.diagnostics().subscriptions,styles=mount.styles()->size();
        for(int i=0;i<50;++i){mount.styles()->replace(declarativeTheme(i%2)+styles_EffectsProbe());mount.flush();CHECK(input->focused());CHECK(input->hasTextComposition());CHECK(input->caretIndex()==2);}
        vm.open.set(false);mount.flush();CHECK(reveal->running());CHECK(!input->focused());CHECK(!reveal->requestFocus(input.get()));CHECK(!reveal->hitTest({2,2}));CHECK(!reveal->isFocusable());
        reveal->tickAnimations(nowMs()+1000);CHECK(!reveal->running() && !reveal->visible());CHECK(reveal->measure({320,200}).height==0);
        for(int i=0;i<1000;++i){vm.open.set(i%2==0);mount.flush();reveal->tickAnimations(nowMs()+1);CHECK(reveal->children().front()==input);}
        vm.open.set(true);mount.flush();reveal->tickAnimations(nowMs()+1000);CHECK(reveal->visible());CHECK(!reveal->running());CHECK(input->text()==L"中文输入保留");
        vm.preset.set(L"fade");mount.flush();const double start=nowMs();vm.open.set(false);mount.flush();reveal->tickAnimations(start+30);RecordingCanvas fade;reveal->paint(fade);CHECK(fade.opacityLayers.size()==1);CHECK(fade.opacityLayers[0].opacity>0 && fade.opacityLayers[0].opacity<1);CHECK(fade.saves==fade.restores);
        vm.reduced.set(true);mount.flush();CHECK(!reveal->running() && !reveal->visible());vm.open.set(true);mount.flush();CHECK(!reveal->running() && reveal->visible());CHECK(mount.diagnostics().subscriptions==subscriptions);CHECK(mount.styles()->size()==styles);CHECK(scheduled>0);
        vm.reduced.set(false);mount.flush();setPlatformMotionEnabled(false);vm.open.set(false);mount.flush();CHECK(!reveal->running());setPlatformMotionEnabled(true);
    }
    // Close while focused: Tab routed through the owning View must leave the closed subtree.
    { auto input=std::make_shared<TextField>();auto reveal=std::make_shared<Reveal>(input);auto next=std::make_shared<Button>(L"next");View parent;parent.add(reveal);parent.add(next);parent.setAnimationScheduler([]{});reveal->setFrame({0,0,300,44});CHECK(parent.requestFocus(input.get()));reveal->setOpen(false);KeyEvent tab;tab.key=Key::Tab;parent.onKeyDown(tab);CHECK(!input->focused());CHECK(!reveal->focusFirstLeaf());CHECK(!reveal->focusLastLeaf());CHECK(!reveal->onFocusChanged(true));
    reveal->setOpen(true);MouseEvent pointer;pointer.position={10,100};CHECK(!reveal->onMouseDown(pointer));CHECK(!reveal->requestFocus(input.get()));reveal->tickAnimations(nowMs()+1000);
    // Fully open Select popups retain the standard overflow painting and hit region.
    auto select=std::make_shared<Select>();select->setItems({L"one",L"two",L"three"});Reveal popup(select);popup.setFrame({0,0,240,40});RecordingCanvas base;popup.paint(base);KeyEvent enter;enter.key=Key::Enter;select->onKeyDown(enter);RecordingCanvas opened;popup.paint(opened);CHECK(popup.paintsAboveSiblings());CHECK(popup.hitTest({10,65}));RecordingCanvas direct;select->paint(direct);CHECK(opened.clips.size()==direct.clips.size()); }
    // Replace is transactional; deleting a rule removes the effect without replacing widgets.
    Mount mount;Compose ui(mount);State<std::wstring> text;auto input=ui.input(text).classes("probe");mount.styles()->replace(declarativeTheme()+syntax::css("Input.probe {box-shadow:inset 0px 2px 6px #00000040;}"));input.widget()->setFrame({0,0,300,44});RecordingCanvas first;input.widget()->paint(first);CHECK(first.insetShadows.size()==1);
    bool rejected=false;try{mount.styles()->replace(declarativeTheme()+syntax::css("Input.probe {box-shadow:inset 0px 2px -6px #000000;}"));}catch(...){rejected=true;}CHECK(rejected);RecordingCanvas kept;input.widget()->paint(kept);CHECK(kept.insetShadows.size()==1);
    mount.styles()->replace(declarativeTheme());RecordingCanvas removed;input.widget()->paint(removed);CHECK(removed.insetShadows.empty());
}
class Fixture:public Widget {
public:void paint(Canvas& c)override{
    c.clear({255,255,255});
    auto linear=box("background:linear-gradient(90deg,#ff0000 0%,#00ff00 50%,#0000ff 100%);border-radius:16px;");paintStyleBox(c,{40,40,300,90},linear);
    paintStyleBox(c,{40,180,300,90},box("background:radial-gradient(50% at 50% 50%,#ff0000 0%,#00ff00 50%,#0000ff 100%);border-radius:16px;"));
    paintStyleBox(c,{400,40,180,100},box("background:#ffffff;box-shadow:inset 0px 4px 12px 3px #000000cc;border-radius:16px;"));
    paintStyleBox(c,{400,210,180,100},box("background:#ffffff;box-shadow:0px 4px 12px 3px #00000099;border-radius:16px;"));
    c.saveOpacity({40,340,300,90},.5f);c.fillRect({40,340,300,90},{255,0,0},16);c.restore();
    paintStyleBox(c,{400,370,180,60},box("background:linear-gradient(90deg,#ff0000 50%,#0000ff 50%);border-radius:12px;"));
}};
static void rasterTests(bool gpu,const std::filesystem::path& output){
    std::filesystem::create_directories(output);SetEnvironmentVariableW(L"ONEUI_ENABLE_GPU",gpu?L"1":L"0");
    auto window=Window::create(L"OneUI effect fixtures",1000,750);window->setContent(std::make_shared<Fixture>());window->initialize();window->show();
    for(float scale:{1.f,1.25f,1.5f}){
        window->setContentScale(scale);window->requestRedraw();UpdateWindow(static_cast<HWND>(window->nativeHandle()));CHECK(window->rendererInfo().backend==(gpu?RenderBackend::OpenGL:RenderBackend::Software));
        const auto name=std::string(gpu?"gpu-":"cpu-")+std::to_string(scale);
        if(!gpu)CHECK(window->captureFramePng((output/(name+".png")).wstring()));
        else {
            CHECK(wglGetCurrentContext()!=nullptr);CHECK(WindowFromDC(wglGetCurrentDC())==static_cast<HWND>(window->nativeHandle()));RECT r{};GetClientRect(static_cast<HWND>(window->nativeHandle()),&r);
            std::vector<unsigned char> pixels(size_t(r.right)*r.bottom*4);GLint buffer,pack;glGetIntegerv(GL_READ_BUFFER,&buffer);glGetIntegerv(GL_PACK_ALIGNMENT,&pack);glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,r.right,r.bottom,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());CHECK(glGetError()==GL_NO_ERROR);glReadBuffer(buffer);glPixelStorei(GL_PACK_ALIGNMENT,pack);
            std::ofstream out(output/(name+".ppm"),std::ios::binary);out<<"P6\n"<<r.right<<' '<<r.bottom<<"\n255\n";for(int y=r.bottom-1;y>=0;--y)for(int x=0;x<r.right;++x)out.write(reinterpret_cast<char*>(pixels.data()+4*(size_t(y)*r.right+x)),3);
        }
    }
    window->close();
}
int main(int argc,char** argv){try{parserTests();transitionTests();if(argc>1)rasterTests(std::string(argv[1])=="--gpu",argc>2?argv[2]:"effects-output");std::cout<<"Effects: parser, CSS replacement, native adapters, retained Reveal, focus, 1000 retargets, policy, bounds passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
