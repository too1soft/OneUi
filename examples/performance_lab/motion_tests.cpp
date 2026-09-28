#include "component_gallery.hpp"
#include "oneui/ui_motion_diagnostics.h"
#include "support/recording_canvas.h"
#include <windows.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <iostream>
using namespace connection_demo;
#define CHECK(x) do{if(!(x))throw std::runtime_error("Failed at "+std::to_string(__LINE__)+": " #x);}while(0)
static double clockMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static void detectorTests(){
    MotionAudit good;good.begin(0,0,true);for(int i=1;i<=10;++i)good.observe(i*16.,i*.1,100+i*10,100+i*10,2);
    CHECK(good.report().continuous() && !good.report().lateFrames);
    MotionAudit bad;bad.begin(0,0,true);bad.observe(60,.5,170,150,40);bad.observe(76,.4,140,140,1);
    CHECK(bad.report().geometryErrors==1 && bad.report().reverseSteps==1 && bad.report().stalls==1);
}
struct Fixed:Widget {void paint(Canvas&)override{}};
static void gapResetTests(){
    auto row=std::make_shared<Stack>();CHECK(row->setEngine(StackEngine::Yoga));row->setGap(24);row->setAlign(StackAlign::Start);
    auto content=std::make_shared<Fixed>();content->setPreferredSize({100,40});auto reveal=std::make_shared<Reveal>(content);row->add(reveal);row->setFlex(reveal,{0,0,40,0});
    auto a=std::make_shared<Fixed>(),b=std::make_shared<Fixed>();a->setPreferredSize({100,40});b->setPreferredSize({100,40});row->add(a);row->add(b);
    oneui::test_support::RecordingCanvas c;row->setFrame({0,0,150,300});row->paint(c);
    row->clearChildren();row->add(a);row->add(b);row->paint(c);CHECK(std::abs(b->frame().y-a->frame().y-64)<.01);
    row->add(reveal);row->paint(c);row->setDirection(StackDirection::Row);CHECK(row->setWrap(true));row->paint(c);CHECK(std::abs(b->frame().y-a->frame().y-64)<.01);
}
static void geometryTests(bool reportOnly){
    for(auto engine:{StackEngine::Legacy,StackEngine::Yoga})for(int position:{0,1,2}){
        auto content=std::make_shared<Fixed>();content->setPreferredSize({200,100});auto reveal=std::make_shared<Reveal>(content);
        auto stack=std::make_shared<Stack>();CHECK(stack->setEngine(engine));stack->setGap(24);
        for(int i=0;i<3;++i){auto child=i==position?std::shared_ptr<Widget>(reveal):std::make_shared<Fixed>();if(i!=position)child->setPreferredSize({200,40});stack->add(child);StackFlex flex;flex.contentBasis=true;stack->setFlex(child,flex);}
        stack->setAnimationScheduler([]{});oneui::test_support::RecordingCanvas canvas;
        auto draw=[&]{auto size=stack->measure({400,INFINITY});stack->setFrame({0,0,400,size.height});stack->paint(canvas);return size.height;};
        reveal->setReducedMotion(true);reveal->setOpen(false);const float closed=draw();reveal->setOpen(true);const float opened=draw();reveal->setReducedMotion(false);
        MotionAudit audit;
        for(bool opening:{false,true}){
            const double t=clockMs();audit.begin(t,reveal->progress(),opening);reveal->setOpen(opening);
            if(!reportOnly)CHECK(std::abs(reveal->progress()-(opening?0.f:1.f))<.0001f);
            audit.observe(t,reveal->progress(),draw(),closed+(opened-closed)*reveal->progress(),0);
            for(int i=0;i<=240;++i){reveal->tickAnimations(t+i);audit.observe(t+i,reveal->progress(),draw(),closed+(opened-closed)*reveal->progress(),0);}
        }
        auto r=audit.report();std::cout<<"geometry engine="<<int(engine)<<" position="<<position<<" errors="<<r.geometryErrors<<" reversal="<<r.reverseSteps<<" max_error="<<r.maxGeometryError<<'\n';
        if(!reportOnly)CHECK(r.continuous());
    }
}
struct Probe:View {
    Mount& ui;std::shared_ptr<Widget> root,section;std::shared_ptr<Reveal> reveal;
    MotionAudit audit;std::ofstream csv;double closed=0,opened=0;bool recording=false;int segment=0;
    Probe(Mount& u,Element e,const std::filesystem::path& file):ui(u),root(e.widget),section(u.find("effectsSection")),reveal(std::dynamic_pointer_cast<Reveal>(u.find("effectsReveal"))),csv(file){
        add(root);csv<<"segment,time_ms,progress,section_height,expected_height,paint_ms,section_y,reveal_y,reveal_height\n";
    }
    void paint(Canvas& canvas)override{
        const double t=clockMs();root->setFrame(frame());View::paint(canvas);const double end=clockMs();
        if(recording){const double expected=closed+(opened-closed)*reveal->progress();audit.observe(end,reveal->progress(),section->frame().height,expected,end-t);csv<<segment<<','<<std::fixed<<end<<','<<reveal->progress()<<','<<section->frame().height<<','<<expected<<','<<end-t<<','<<section->frame().y<<','<<reveal->frame().y<<','<<reveal->frame().height<<'\n';if(!reveal->running())recording=false;}
    }
};
static void nativeTests(bool gpu,const std::filesystem::path& out,bool reportOnly,int width,int height,int duration,double hz,float scale){
    std::filesystem::create_directories(out);SetEnvironmentVariableW(L"ONEUI_ENABLE_GPU",gpu?L"1":L"0");
    UiMailbox mailbox;VM vm(mailbox.sender());Samples samples;Mount ui;auto root=buildGallery(ui,vm,samples);samples.scene.set(3);ui.flush();ui.styles()->replace(declarativeTheme()+styles_Gallery()+samples.effectCss());
    auto probe=std::make_shared<Probe>(ui,root,out/"motion-frames.csv");auto window=Window::create(L"OneUI native motion regression",width,height);window->setDefaultFontFamily(L"Microsoft YaHei UI");window->setContent(probe);window->initialize();window->setContentScale(scale);window->show();probe->audit=MotionAudit(1000./hz);probe->reveal->setTransition({double(duration),EasingCurve::EaseOutCubic});

    samples.reducedMotion.set(true);samples.expanded.set(false);ui.flush();window->prepareLayoutSnapshot();probe->closed=probe->section->frame().height;CHECK(window->rendererInfo().backend==(gpu?RenderBackend::OpenGL:RenderBackend::Software));
    samples.expanded.set(true);ui.flush();window->prepareLayoutSnapshot();probe->opened=probe->section->frame().height;samples.reducedMotion.set(false);ui.flush();CHECK(revealField(root.widget,probe->section.get()));window->prepareLayoutSnapshot();
    const auto source=ui.styles()->sources().at(probe->reveal.get());
    const auto styles=ui.styles()->size(),subscriptions=ui.diagnostics().subscriptions;
    std::thread driver([&]{std::this_thread::sleep_for(std::chrono::milliseconds(500));for(int i=0;i<16;++i){window->post([&,i]{probe->segment=i;probe->audit.begin(clockMs(),probe->reveal->progress(),!samples.expanded.get());probe->recording=true;samples.toggleExpanded.execute();ui.flush();});std::this_thread::sleep_for(std::chrono::milliseconds(i<12?500:50));}std::this_thread::sleep_for(std::chrono::milliseconds(500));window->post([&]{window->close();});});
    window->run();driver.join();const auto r=probe->audit.report();CHECK(ui.styles()->size()==styles && ui.diagnostics().subscriptions==subscriptions);
    std::ofstream report(out/"motion-report.txt");report<<"source="<<source.file<<":"<<source.line<<"\ntarget_hz="<<hz<<"\nduration_ms="<<duration<<"\nwidth="<<width<<"\nscale="<<scale<<"\nbackend="<<(gpu?"gpu":"cpu")<<"\nsamples="<<r.samples<<"\ngeometry_errors="<<r.geometryErrors<<"\nreverse_steps="<<r.reverseSteps<<"\nlate_frames="<<r.lateFrames<<"\nstalls="<<r.stalls<<"\nmax_geometry_error="<<r.maxGeometryError<<"\nmax_interval_ms="<<r.maxIntervalMs<<"\nmax_content_paint_ms="<<r.maxPaintMs<<'\n';
    std::cout<<"native samples="<<r.samples<<" errors="<<r.geometryErrors<<" reversal="<<r.reverseSteps<<" late="<<r.lateFrames<<" stalls="<<r.stalls<<" max_interval="<<r.maxIntervalMs<<" max_error="<<r.maxGeometryError<<'\n';
    if(!reportOnly){CHECK(r.samples>=36);CHECK(r.continuous());CHECK(r.stalls==0);CHECK(r.lateFrames*5<=r.samples);}
}
int main(int argc,char** argv){try{const bool reportOnly=argc>1 && std::string(argv[1])=="--report";detectorTests();gapResetTests();geometryTests(reportOnly);if(argc>2)nativeTests(std::string(argv[2])=="gpu",argc>3?argv[3]:"motion-output",reportOnly,argc>4?std::stoi(argv[4]):1320,argc>5?std::stoi(argv[5]):1000,argc>6?std::stoi(argv[6]):220,argc>7?std::stod(argv[7]):60,argc>8?std::stof(argv[8]):1.f);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
