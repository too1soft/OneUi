#include "workbench.h"
#include "TerminalWorkbench.g.h"
#include <oneui/native_interop.h>
#include <oneui/ui_declarative_app.h>
#include <oneui/ui_compose.h>
#include <oneui/ui_theme.h>
#include <oneui/controls/tabs.h>
#include <oneui/controls/terminal_view.h>
#include <oneui/controls/text_field.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace oneui;
using namespace oneui::ui;
#define CHECK(x) do { if(!(x))throw std::runtime_error("Failed: " #x); } while(0)

int main(int argc,char** argv) {try {
    bool test=false,dark=false,narrow=false;std::wstring capture;std::string scene;
    for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--test")test=true;else if(a=="--dark")dark=true;else if(a=="--narrow")narrow=true;else if(a=="--scene" && i+1<argc)scene=argv[++i];else if(a=="--capture" && i+1<argc)capture=std::filesystem::path(argv[++i]).wstring();}
    DeclarativeApp app(L"OneUI · 连接工作区",narrow?640:1360,narrow?820:900);
    app.window().setDeveloperTools({true,false,1000.0/60.0});
    Workbench workbench(app,dark);auto& vm=workbench.vm;auto& mount=app.mount();
    workbench.resize(narrow?640:1360);
    auto root=build_TerminalWorkbench(vm,mount);workbench.mounted();workbench.start();
    auto first=workbench.terminals[0],second=workbench.terminals[1];auto input=workbench.input;auto tabs=workbench.tabs;
    auto& selected=workbench.selected;
    auto select=[&](int i,bool focus){workbench.select(i,focus);};
    auto closeTab=[&](int i){workbench.closeTab(i);};
    const auto initialSeries=vm.cpuSeries.get();
    bool passed=!test;std::string failure;std::thread captureWorker,idleWorker;std::uint64_t idleFrames=0;
    const int result=app.run(root,[&]{
        if(scene=="split")vm.toggleSplit.execute();
        if(scene=="files")vm.showFiles.execute();
        if(scene=="monitor")vm.showMonitor.execute();
        if(scene=="processes")vm.showProcesses.execute();
        if(scene=="collapsed")vm.collapseFiles.execute();
        if(scene=="search"){vm.toggleSearch.execute();vm.terminalQuery.set(L"healthy");}
        if(test)vm.commandVisible.set(true);
        mount.flush();app.window().prepareLayoutSnapshot();app.window().requestFocus(first.get(),false);
        if(!capture.empty() && !test){
            captureWorker=std::thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(350));app.window().post([&]{
                if(!app.window().captureFramePng(capture)){passed=false;failure="Capture failed";}app.window().close();
            });});
        }
        if(!test)return;
        vm.toggleSampling.execute();
        captureWorker=std::thread([&]{
        std::this_thread::sleep_for(std::chrono::milliseconds(1300));
        app.window().post([&]{try {
            CHECK(!(initialSeries==vm.cpuSeries.get()));vm.toggleSampling.execute();
            if(const char* gpu=std::getenv("ONEUI_ENABLE_GPU")) {
                if(std::string(gpu)=="1")CHECK(app.window().rendererInfo().backend==RenderBackend::OpenGL);
                if(std::string(gpu)=="0")CHECK(app.window().rendererInfo().backend==RenderBackend::Software);
            }
            CHECK(first->frame().width>400 && first->frame().height>120);
            CHECK(std::abs(tabs->frame().height-42)<0.01f);
            // Rust callbacks receive committed text through the native window route.
            const HWND hwnd=static_cast<HWND>(app.window().nativeHandle());
            SendMessageW(hwnd,WM_CHAR,L'中',0);SendMessageW(hwnd,WM_CHAR,L'A',0);
            first->selectAll();const auto terminalText=first->selectedText();CHECK(terminalText.find(L"中A")!=std::wstring::npos);
            const auto tabRect=tabs->itemFrame(1);
            const auto point=MAKELPARAM(int(tabRect.x+30),int(tabRect.y+tabRect.height/2));
            SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageW(hwnd,WM_LBUTTONUP,0,point);
            CHECK(selected==1);SendMessageW(hwnd,WM_CHAR,L'B',0);second->selectAll();CHECK(second->selectedText().find(L"> B")!=std::wstring::npos);
            second->onTextInputText(std::wstring(100,L'\n')+L"TAIL");second->selectAll();CHECK(second->selectedText().find(L"TAIL")!=std::wstring::npos);
            tabs->setSelectedIndex(0);select(0,false);
            input->setText(L"输入状态保留 ABC");input->setSelectionRange(1,4);
            CHECK(app.window().requestFocus(input.get(),true));input->setTextComposition(L"中文",1);
            vm.theme.execute();mount.flush();CHECK(input->focused() && input->hasTextComposition());
            input->setTextComposition({},0);input->setSelectionRange(1,4);
            const auto before=mount.diagnostics();const auto styleCount=mount.styles()->size();
            for(int i=0;i<100;++i){select(1,false);vm.theme.execute();select(0,false);closeTab(0);vm.restore.execute();app.window().prepareLayoutSnapshot();}
            mount.flush();app.window().prepareLayoutSnapshot();
            CHECK(mount.diagnostics().subscriptions==before.subscriptions);CHECK(mount.styles()->size()==styleCount);
            CHECK(input->text()==L"输入状态保留 ABC");CHECK(input->selectionStart()==1 && input->selectionEnd()==4);
            CHECK(first->selectedText()==terminalText);
            CHECK(std::dynamic_pointer_cast<View>(mount.find("firstHost"))->children().front()==first);
            closeTab(1);closeTab(0);CHECK(vm.empty.get());vm.restore.execute();CHECK(!vm.empty.get());
            // Missing names, duplicate registrations and duplicate attachment fail explicitly.
            bool rejected=false;try{mount.nativeHost("terminal0");}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
            rejected=false;try{mount.nativeHost("missing");}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
            rejected=false;try{mount.registerNative("input",input);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
            std::shared_ptr<TerminalView> retained;std::shared_ptr<Widget> host;
            std::weak_ptr<void> ownerLife;
            {
                Mount child;auto owner=std::shared_ptr<void>(terminal_demo_create_v1(),terminal_demo_destroy_v1);CHECK(owner);ownerLife=owner;
                retained=std::dynamic_pointer_cast<TerminalView>(retainNativeWidget(terminal_demo_widget_v1(owner.get(),0)));
                child.registerNative("terminal",retained,owner);host=child.nativeHost("terminal").widget;
            }
            CHECK(!ownerLife.expired());retained->onTextInput(L'B');
            host.reset();CHECK(ownerLife.expired());CHECK(!retained->hasCompositionOwner());
            retained->onTextInput(L'C'); // Rust Drop removed callbacks although C++ still retains native pixels.
            {Mount child;child.registerNative("terminal",retained);auto reattached=child.nativeHost("terminal");CHECK(reattached.widget);}
            {Mount child;child.registerNative("terminal",first);rejected=false;try{child.nativeHost("terminal");}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);}
            // The resize notification is frame-coalesced. This synchronous block
            // applies the same policy explicitly; the frame loop is checked below.
            auto settleResize=[&](bool expectedNarrow){
                workbench.resize(app.window().clientSize().width);
                CHECK(vm.narrow.get()==expectedNarrow);mount.flush();app.window().prepareLayoutSnapshot();
            };
            // Resize the OS window: no application child setFrame calls.
            RECT bounds{};GetWindowRect(hwnd,&bounds);
            SetWindowPos(hwnd,nullptr,0,0,640,600,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            settleResize(true);CHECK(first->frame().width>400 && first->frame().width<640);CHECK(first->frame().height>80);
            SetWindowPos(hwnd,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            settleResize(narrow);CHECK(std::abs(tabs->frame().height-42)<0.01f);
            // Full-workspace business interactions retain the same controls and stable IDs.
            workbench.openSession(2);CHECK(selected==2 && vm.tabs.get().size()==3);
            workbench.openSession(3);CHECK(selected==3 && vm.tabs.get().size()==4);
            vm.closeSession(L"build");CHECK(selected==3 && vm.tabs.get().size()==3);
            vm.connectionQuery.set(L"postgres");CHECK(workbench.connections->items().size()==1);
            vm.connectionQuery.set(L"");CHECK(workbench.connections->items().size()==4);
            workbench.openSession(0);input->setText(L"help");vm.send.execute();
            first->selectAll();CHECK(first->selectedText().find(L"status")!=std::wstring::npos && input->text().empty());
            vm.newFolder.execute();CHECK(workbench.files->selectedIndex()==0 && vm.removeFile.canExecute.get());
            vm.fileQuery.set(L"new-folder");CHECK(workbench.files->rows().size()==1 && workbench.files->selectedIndex()==0);
            vm.removeFile.execute();CHECK(workbench.files->rows().empty());
            vm.fileQuery.set(L"");CHECK(workbench.files->rows().size()==18);
            workbench.files->setSelectedIndices({0,1});CHECK(workbench.files->selectedIndices().size()==2);
            vm.fileQuery.set(L"config");CHECK(workbench.files->selectedIndices().size()==1);
            vm.fileQuery.set(L"");CHECK(workbench.files->selectedIndices().size()==1 && workbench.files->rows()[workbench.files->selectedIndex()][0]==L"config");
            const auto series=vm.cpuSeries.get();const auto counts=mount.diagnostics();
            workbench.sample();mount.flush();CHECK(!(series==vm.cpuSeries.get()));
            CHECK(counts.ownedObjects==mount.diagnostics().ownedObjects && counts.subscriptions==mount.diagnostics().subscriptions);
            if(!narrow){
                vm.toggleFiles.execute();mount.flush();app.window().prepareLayoutSnapshot();const float expanded=first->frame().height;
                vm.toggleFiles.execute();mount.flush();app.window().prepareLayoutSnapshot();CHECK(expanded>first->frame().height+80);
                auto split=std::dynamic_pointer_cast<SplitView>(mount.find("fileSplit"));const auto top=split->children().front()->frame();
                const auto down=MAKELPARAM(int(top.x+top.width/2),int(top.y+top.height+2));const auto moved=MAKELPARAM(int(top.x+top.width/2),int(top.y+top.height-40));const auto oldRatio=vm.fileRatio.get();
                SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,down);SendMessageW(hwnd,WM_MOUSEMOVE,MK_LBUTTON,moved);SendMessageW(hwnd,WM_LBUTTONUP,0,moved);mount.flush();CHECK(vm.fileRatio.get()<oldRatio-0.01f);
                vm.collapseFiles.execute();mount.flush();app.window().prepareLayoutSnapshot();CHECK(!vm.filesExpanded.get());CHECK(std::abs(split->children().back()->frame().height-40)<0.1f);
                const auto collapsedRatio=vm.fileRatio.get();vm.collapseFiles.execute();mount.flush();app.window().prepareLayoutSnapshot();CHECK(vm.filesExpanded.get() && vm.fileRatio.get()==collapsedRatio);
            } else {
                vm.showFiles.execute();mount.flush();app.window().prepareLayoutSnapshot();CHECK(vm.filesVisible.get() && !vm.terminalVisible.get());CHECK(workbench.files->frame().width>500);
                vm.showMonitor.execute();mount.flush();app.window().prepareLayoutSnapshot();CHECK(vm.monitorVisible.get() && !vm.mainVisible.get());
                vm.showTerminal.execute();mount.flush();app.window().prepareLayoutSnapshot();CHECK(vm.terminalVisible.get() && !vm.monitorVisible.get());
            }
            vm.showProcesses.execute();CHECK(vm.processVisible.get() && !vm.resourceVisible.get());
            vm.monitorTab.set(L"connection");CHECK(vm.connectionVisible.get());vm.monitorTab.set(L"resources");
            vm.showTerminal.execute();vm.toggleSplit.execute();mount.flush();app.window().prepareLayoutSnapshot();CHECK(vm.pane0.get());
            auto retainedSecond=workbench.secondary[0];CHECK(retainedSecond->frame().width>150);retainedSecond->onTextInputText(L"PANE-RETAINED");
            vm.toggleSplit.execute();vm.toggleSplit.execute();mount.flush();app.window().prepareLayoutSnapshot();retainedSecond->selectAll();CHECK(retainedSecond->selectedText().find(L"PANE-RETAINED")!=std::wstring::npos);vm.toggleSplit.execute();
            vm.terminalQuery.set(L"healthy");CHECK(vm.searchResult.get()!=L"无匹配");vm.searchNext.execute();CHECK(first->hasSelection());
            vm.upload.execute();CHECK(workbench.files->rows().size()==19);vm.removeFile.execute();CHECK(workbench.files->rows().size()==18);
            vm.restore.execute();mount.flush();app.window().prepareLayoutSnapshot();
            { // C++ and template entry points use the same adapters.
                Mount isolated;Compose ui(isolated);State<float> ratio{0.5f};State<std::wstring> key{L"b"};
                State<std::vector<TabItem>> items{{{L"a",L"A"},{L"b",L"B"}}};
                auto keyed=ui.tabs(items,key).closable(true);auto original=keyed.element().widget;
                items.set({{L"b",L"B updated"},{L"a",L"A"}});isolated.flush();
                CHECK(key.get()==L"b" && keyed.element().as<Tabs>()->selectedIndex()==0);
                bool duplicate=false;try{keyed.element().tabs->update({{L"b",L"B"},{L"b",L"Duplicate"}});}catch(const std::invalid_argument&){duplicate=true;}
                CHECK(duplicate && keyed.element().widget==original && keyed.element().tabs->key(1)==L"a");
                auto pane=ui.split(ui.column({}),ui.column({}),ratio).orientation(L"vertical");
                ratio.set(0.3f);isolated.flush();CHECK(std::abs(pane.element().as<SplitView>()->splitRatio()-0.3f)<0.001f);
                auto graph=ui.chart(vm.cpuSeries);CHECK(graph.element().as<TimeSeriesChart>());
            }
            // Safe mailbox rejects worker results after page closure and drops queued jobs.
            bool applied=false;auto sender=app.dispatcher();CHECK(sender.post([&]{applied=true;}));app.close();
            std::atomic<bool> accepted{true};std::thread worker([&]{accepted=sender.post([&]{applied=true;});});worker.join();
            CHECK(!accepted && !applied);
            app.window().requestFocus(first.get(),false);
            idleWorker=std::thread([&]{
                std::this_thread::sleep_for(std::chrono::milliseconds(350));
                app.window().post([&]{idleFrames=app.window().developerSnapshot().frames;});
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                app.window().post([&]{try{CHECK(app.window().developerSnapshot().frames==idleFrames);passed=true;}catch(const std::exception& e){failure=e.what();}app.window().close();});
            });
        }catch(const std::exception& e){failure=e.what();app.window().close();}});
        });
    });
    if(captureWorker.joinable())captureWorker.join();
    if(idleWorker.joinable())idleWorker.join();
    workbench.stop();
    if(!passed)throw std::runtime_error(failure);
    if(test){std::cout<<"PASS: full workspace (4 sessions, search, keyed tabs/duplicates, files, split drag, charts, panels), Rust-owned terminal/input, native text route, 100 switch/theme/close/restore cycles, identity/selection/subscription retention, native tab click, composition/focus retention, resize, 42px tabs, exclusive mount, owner lifetime, post-close worker rejection, idle redraw=0.\n";}
    if(!failure.empty())throw std::runtime_error(failure);
    return result;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
