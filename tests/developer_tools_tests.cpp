#include "internal/developer_tools.h"
#include "oneui/controls/reveal.h"
#include "support/recording_canvas.h"
#include <iostream>
#include <stdexcept>
using namespace oneui;
using namespace oneui::internal;
#define CHECK(x) do{if(!(x))throw std::runtime_error("Failed: " #x);}while(0)
struct Probe:Widget {void paint(Canvas&)override{}};
int main(){try {
    Probe widget;widget.setDiagnosticSource({"Slow\"Widget","fixture\n.one",17});
    DeveloperRecorder recorder({});
    recorder.beginFrame(0);recorder.observePaint(widget,40);recorder.finishFrame(45,false,{});
    auto s=recorder.snapshot();CHECK(s.slowPaints==1 && s.issues.back().source.line==17);
    CHECK(s.last.hottestSelfMs==40);
    // Idle time cannot become an animation stall. Pending animation can.
    recorder.beginFrame(2000);recorder.finishFrame(2001,true,{});CHECK(recorder.snapshot().animationDelays==0);
    recorder.beginFrame(2055);recorder.finishFrame(2056,false,{});CHECK(recorder.snapshot().animationDelays==1);
    CHECK(recorder.snapshot().issues.back().source.component=="Window.animationCadence");
    DeveloperFrame submission;submission.contentMs=2;submission.submitMs=40;
    recorder.beginFrame(2100);recorder.observePaint(widget,1);recorder.finishFrame(2145,false,submission);
    CHECK(recorder.snapshot().issues.back().source.component=="Window.rendererSubmit");
    recorder.resetCadence();recorder.beginFrame(3000);recorder.finishFrame(3001,false,{});CHECK(recorder.snapshot().animationDelays==1);
    recorder.observeMotion(widget,1,.8f,true);recorder.observeMotion(widget,1,.6f,true);
    CHECK(recorder.snapshot().motionReversals==1);
    recorder.observeMotion(widget,2,.5f,false);CHECK(recorder.snapshot().motionReversals==1);
    // Ring capacity remains fixed; old reports own their source strings.
    for(int i=0;i<300;++i){recorder.beginFrame(i*100.+4000);recorder.observePaint(widget,40);recorder.finishFrame(i*100.+4040,false,{});}
    s=recorder.snapshot();CHECK(s.issues.size()==128 && s.discardedIssues>170);
    CHECK(developerReportJson(s).find("Slow\\\"Widget")!=std::string::npos);
    CHECK(developerReportJson(s).find("fixture\\u000a.one")!=std::string::npos);
    widget.setDiagnosticSource({"changed",{},0});CHECK(s.issues.back().source.line==17);
    // Nested windows/sessions restore the previous per-thread recorder.
    CHECK(!activeDeveloperRecorder());
    {DiagnosticSession outer(&recorder);CHECK(activeDeveloperRecorder()==&recorder);
      {DiagnosticSession inner(nullptr);CHECK(!activeDeveloperRecorder());}
      CHECK(activeDeveloperRecorder()==&recorder);
    }CHECK(!activeDeveloperRecorder());
    // Built-in Reveal emits samples without application-side observe calls.
    auto child=std::make_shared<Probe>();child->setPreferredSize({200,80});Reveal reveal(child);
    reveal.setAnimationScheduler([]{});reveal.setFrame({0,0,200,80});reveal.setOpen(false);
    oneui::test_support::RecordingCanvas canvas;
    const auto motionSamples=recorder.snapshot().motionSamples;
    {DiagnosticSession session(&recorder);reveal.paint(canvas);}
    CHECK(recorder.snapshot().motionSamples==motionSamples+1);
    std::vector<std::unique_ptr<Probe>> many;
    for(int i=0;i<300;++i){many.push_back(std::make_unique<Probe>());recorder.observeMotion(*many.back(),1,0,true);}
    CHECK(recorder.snapshot().motionEvictions>0);
    // Removed widgets do not stay alive in the collector.
    auto transient=std::make_unique<Probe>();recorder.observeMotion(*transient,1,.1,true);transient.reset();
    bool rejected=false;try{DeveloperRecorder invalid({true,true,0});}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
    std::cout<<"Developer tools: frame budgets, idle boundaries, source ownership, reversal, bounded history and sessions passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
