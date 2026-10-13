#include "views/format/legacy_baseline_controls.h"
void scrollSize(const char* runtimeFile) {
    const auto directory=std::filesystem::path(runtimeFile).parent_path();
    const auto image=load((directory/"ScrollSize.oneview").string().c_str());
    const auto legacy=load((directory/"ScrollSizeV3.oneview").string().c_str());
    const auto& contract=oneview::registry().at("Scroll");
    expect(contract.version==5 && contract.events.size()==1 && contract.events[0].id==1,
           "Scroll v5 combines inset property16 with viewport event1");
    for(const auto& c:legacy.components)if(c.name=="Scroll")
        expect(c.version==5,"real old Scroll v3 normalizes only after exact validation");
    const auto& historical=oneview::legacyBaselineControls();
    const auto scrollV3=std::find_if(historical.begin(),historical.end(),
        [](const auto& value){return value.name=="Scroll" && value.version==3;});
    expect(scrollV3!=historical.end(),"frozen Scroll v3 descriptor exists");
    auto oldContract=*scrollV3;
    for(int mutation=0;mutation<10;++mutation) {
        auto bad=oldContract;
        if(mutation==0)bad.version=2;
        if(mutation==1)bad.properties[0].id=99;
        if(mutation==2)bad.properties[0].name="wrong";
        if(mutation==3)bad.properties[0].tag=4;
        if(mutation==4)bad.properties[0].flags=3;
        if(mutation==5)bad.properties[0].record="unexpected";
        if(mutation==6)bad.events=contract.events;
        if(mutation==7)bad.childRule="none";
        if(mutation==8)bad.modelProperty=1;
        if(mutation==9)bad.properties.pop_back();
        try {oneview::decodeComponents(oneview::encodeComponents({bad}));
            expect(false,"inexact legacy Scroll contract accepted");}
        catch(const oneview::Error&) {}
    }
    auto injected=image;
    for(auto& c:injected.components)if(c.name=="Scroll")c=oldContract;
    try {oneview::readImage(oneview::writeImage(injected));
        expect(false,"v3 zero-event contract admitted a v4 EVNT reference");}
    catch(const oneview::Error&) {}
    // Start with actual old-compiler IR: there is no authored event binding.
    Vm vm(legacy,"Root");const auto visible=id(image,"Root","visible");
    vm.values[visible].integer=1;
    auto runtime=std::make_shared<Runtime>(legacy);
    vm.table.invoke=[](void* data,uint32_t,const OneUiValue* args,size_t count) {
        auto& vm=*static_cast<Vm*>(data);
        if(count!=2 || args[0].tag!=3 || args[1].tag!=3)return int(ONEUI_VIEW_TYPE);
        vm.values[1001]=Value::copy(args[0]);vm.values[1002]=Value::copy(args[1]);
        ++vm.invokes;if(vm.onInvoke)vm.onInvoke();return 0;
    };
    runtime->initialize(&vm.table,&vm);
    auto scroll=[&]{return runtime->root->children[0]->element.as<oneui::ScrollView>();};
    auto root=runtime->root->element.widget;
    oneui::test_support::RecordingCanvas canvas;
    auto paint=[&](oneui::Rect frame){root->setFrame(frame);root->paint(canvas);};
    bool queued=false;int rejected=0,drained=0;
    vm.onInvoke=[&] {
        expect(callbackPhase==Phase::Mutate,"size command uses ordinary mutable event phase");
        const auto frame=scroll()->frame();
        expect(vm.values[1001].number==frame.width && vm.values[1002].number==frame.height,
               "size callback sees newly committed actual viewport frame");
        try {runtime->flush();expect(false,"size command allowed reentrant flush");}
        catch(const Failure& error) {
            expect(error.result==ONEUI_VIEW_REENTRANT,"size command flush rejected as reentrant");++rejected;
        }
        queued=true; // Consumer posts/coalesces work; never measures children here.
    };
    auto drain=[&] {
        expect(callbackPhase==Phase::Idle,"deferred refresh runs after layout and event unwind");
        if(queued){queued=false;runtime->flush();++drained;}
    };
    paint({10,20,640,480});expect(vm.invokes==0,"old v3 IR produces no invented event");
    auto retained=scroll();runtime->reload(image);
    expect(scroll()==retained,"adding size binding retains old native Scroll");
    paint({10,20,641,481});
    expect(vm.invokes==1 && queued && drained==0 && rejected==1,"real paint emits size and defers refresh");
    expect(scroll()->frame().width==641 && scroll()->frame().height==481,"Scroll fills actual parent layout");
    drain();paint({10,20,641,481});paint({30,40,641,481});
    expect(vm.invokes==1,"same size and position-only changes do not emit");
    paint({30,40,700,520});expect(vm.invokes==2,"paint resize emits current viewport dimensions");drain();
    scroll()->setChromeVisible(false);paint({30,40,701,521});
    expect(vm.values[1001].number==701 && vm.values[1002].number==521,"chrome-hidden event matches viewportRect");drain();
    scroll()->setChromeVisible(true);scroll()->setContentHeight(1000);paint({30,40,702,522});
    expect(vm.values[1001].number==702 && vm.values[1002].number==522,"chrome and overflow do not shrink viewportRect event");drain();
    auto calls=vm.invokes;
    vm.values[visible].integer=0;runtime->notify(runtime->rootScope,visible);runtime->flush();
    paint({30,40,720,540});
    expect(vm.invokes==calls+1 && vm.values[1001].number==0 && vm.values[1002].number==0,
           "Yoga hide reports the actual zero viewport instead of retaining stale size");
    drain();calls=vm.invokes;paint({30,40,720,540});
    expect(vm.invokes==calls,"unchanged hidden viewport does not repeat event");
    vm.values[visible].integer=1;runtime->notify(runtime->rootScope,visible);runtime->flush();
    paint({30,40,720,540});expect(vm.invokes==calls+1,"show receives actual new viewport size");drain();
    calls=vm.invokes;runtime->reload(legacy);paint({30,40,721,541});
    expect(vm.invokes==calls && scroll()==retained,"reload to real v3 removes binding without replacing Scroll");
    runtime->reload(image);paint({30,40,722,542});
    expect(vm.invokes==calls+1,"reload restores size binding");drain();
    auto replacement=image;
    replacement.strings.push_back("replacement-viewport");
    replacement.records[oneview::NODE][runtime->root->children[0]->node][11]=uint32_t(replacement.strings.size()-1);
    runtime->reload(replacement);expect(scroll()!=retained,"changed identity replaces Scroll");
    calls=vm.invokes;retained->setFrame({0,0,100,100});
    expect(vm.invokes==calls,"retired Scroll weak dispatcher cannot invoke new VM");
    paint({30,40,723,543});expect(vm.invokes==calls+1,"replacement Scroll emits size");drain();
    auto afterClose=scroll();runtime->close();calls=vm.invokes;
    afterClose->setFrame({0,0,100,100});
    expect(vm.invokes==calls && vm.retains==vm.releases,"closed View suppresses callbacks and releases VM");

    // Initially hidden is distinct from hiding an already-laid-out viewport.
    Vm hidden(image,"Root");auto hiddenRuntime=std::make_shared<Runtime>(image);
    hiddenRuntime->initialize(&hidden.table,&hidden);
    auto hiddenRoot=hiddenRuntime->root->element.widget;
    hiddenRoot->setFrame({0,0,640,480});hiddenRoot->paint(canvas);
    expect(hidden.invokes==0,"initial hidden Scroll emits no geometry");
    hidden.values[visible].integer=1;hiddenRuntime->notify(hiddenRuntime->rootScope,visible);hiddenRuntime->flush();
    hiddenRoot->paint(canvas);expect(hidden.invokes==1,"first visible layout emits size");
    auto closingWidget=hiddenRuntime->root->children[0]->element.widget;
    hidden.onInvoke=[&]{hiddenRuntime->close();};
    hiddenRoot->setFrame({0,0,650,490});hiddenRoot->paint(canvas);
    expect(hiddenRuntime->closed && hidden.retains==hidden.releases,"close inside size event unwinds safely");
    calls=hidden.invokes;closingWidget->setFrame({0,0,100,100});
    expect(hidden.invokes==calls,"closed-in-event dispatcher stays inert");
}
