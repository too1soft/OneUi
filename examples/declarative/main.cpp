#include <windows.h>
#include <psapi.h>
#include "oneui/ui_declarative_app.h"
#include "oneui/ui_theme.h"
#include "oneui/ui_dev.h"
#include "oneui/ui_layout_diagnostics.h"
#include "manual.h"
#include "Demo.g.h"
#include <iostream>
#include <numeric>

struct PaintProbe : View {
    std::vector<double> paints;
    bool record=false;
    std::function<void()> diagnostics;
    explicit PaintProbe(std::shared_ptr<Widget> root){add(std::move(root));}
    void layoutChildren() override {children().front()->setFrame(frame());}
    void paint(Canvas& canvas) override {
        const auto begin=std::chrono::steady_clock::now();View::paint(canvas);
        if(record)paints.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
        if(diagnostics)diagnostics();
    }
};

static double cpuMs() {
    FILETIME created,exit,kernel,user; GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user);
    ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
    return double(k.QuadPart+u.QuadPart)/10000.0;
}
int main(int argc,char** argv) {
    try {
        bool code=false,dev=false,dark=false,list=false,detail=false;int width=1100,height=860;float scale=1;
        std::string capture,output,layoutReport,scenario="idle",state;double seconds=0;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];auto next=[&]{if(i+1>=argc)throw std::invalid_argument("Missing argument value");return std::string(argv[++i]);};
            if(arg=="--code")code=true;else if(arg=="--dev")dev=true;else if(arg=="--dark")dark=true;
            else if(arg=="--page") {auto page=next();if(page!="settings" && page!="list" && page!="detail")throw std::invalid_argument("Invalid page");list=page=="list";detail=page=="detail";}
            else if(arg=="--width")width=std::max(640,std::stoi(next()));else if(arg=="--height")height=std::max(560,std::stoi(next()));
            else if(arg=="--scale")scale=std::stof(next());else if(arg=="--capture")capture=next();
            else if(arg=="--output")output=next();else if(arg=="--benchmark-seconds")seconds=std::stod(next());else if(arg=="--scenario")scenario=next();
            else if(arg=="--state")state=next();
            else if(arg=="--layout-report")layoutReport=next();
            else throw std::invalid_argument("Unknown argument: "+arg);
        }
        if(scale<1 || scale>2 || seconds<0 || seconds>3600 || (scenario!="idle" && scenario!="updates"))throw std::invalid_argument("Invalid scale, duration or scenario");
        DeclarativeApp app(code?L"OneUI · C++ 声明式示例":L"OneUI · .one 模板示例",width,height);auto& window=app.window();
        window.setMinimumClientSize({640,560});window.setContentScale(scale);
        DemoVM vm(app.dispatcher());vm.settings.set(!list && !detail);vm.theme.set(dark?1:0);if(detail){vm.selectedKey.set(L"1");vm.details.set(true);}
        if(state=="dirty")vm.name.set(L"新工作空间");
        else if(state=="saving"){vm.name.set(L"正在保存的工作空间");vm.save.execute();}
        else if(state=="error"){vm.interval.set(L"无效间隔");vm.save.error.set(L"模拟保存失败。修正输入后可重试。");}
        else if(state=="empty"){vm.settings.set(false);vm.query.set(L"没有匹配的名称");}
        else if(state=="loading"){vm.settings.set(false);vm.details.set(false);vm.reload.execute();}
        else if(state=="long")vm.name.set(L"用于检查中文排版和文本滚动的超长工作空间名称 · 上海研发与设计团队");
        else if(!state.empty() && state!="focus")throw std::invalid_argument("Invalid preview state");
        auto& mount=app.mount();
        const auto source=std::filesystem::path(DEMO_SOURCE_DIR);
        auto styles=[&] {
            try {
                std::string css=declarativeTheme(vm.theme.get()==1);
                if(dev) {for(auto& item:sources_Demo())css+=inlineStyles(readStyleFile(item.first),item.second,item.first);}
                else css+=styles_Demo();
                css+=syntax::css(readStyleFile(source/"theme.css"),{},(source/"theme.css").string());
                mount.styles()->replace(css);vm.styleError.set({});
            }catch(const std::exception& e){vm.styleError.set(wide(e.what()));std::cerr<<e.what()<<"\n";}
        };
        styles();auto root=code?buildManual(vm,mount):build_Demo(vm,mount);styles();
        auto probe=std::make_shared<PaintProbe>(root.widget);if(seconds>0){probe->record=true;probe->paints.reserve(65536);}
        std::string lastLayout;std::uint64_t lastRevision=~std::uint64_t{0};Rect lastFrame{};
        if(dev || !layoutReport.empty())probe->diagnostics=[&] {
            auto frame=root.widget->frame();auto revision=root.widget->measureRevision();
            if(revision==lastRevision && frame.x==lastFrame.x && frame.y==lastFrame.y && frame.width==lastFrame.width && frame.height==lastFrame.height)return;
            lastRevision=revision;lastFrame=frame;
            auto report=formatLayoutIssues(inspectLayout(root.widget,*mount.styles()));
            if(report==lastLayout)return;lastLayout=report;
            if(dev)std::cerr<<"[OneUI layout] "<<report;
            if(!layoutReport.empty()){std::ofstream f(layoutReport);f<<report;}
        };
        auto themeSubscription=vm.theme.subscribeScoped([&](int){styles();});
        std::unique_ptr<StyleWatcher> watcher;
        if(dev) {std::vector<std::string> files{(source/"theme.css").string()};for(auto& item:sources_Demo())files.push_back(item.first);watcher=std::make_unique<StyleWatcher>(app.dispatcher(),files,styles);}
        std::vector<double> frameWork,intervals;double first=0,last=0,startCpu=0;int frames=0,result=0;
        const auto initialWidgets=mount.styles()->size();std::function<void(double)> benchmark;
        auto summary=[](std::vector<double> values){std::sort(values.begin(),values.end());return std::pair<double,double>{std::accumulate(values.begin(),values.end(),0.0)/std::max<std::size_t>(1,values.size()),values.empty()?0:values[std::min(values.size()-1,std::size_t(values.size()*.95))]};};
        auto report=[&](double elapsed) {
            PROCESS_MEMORY_COUNTERS_EX mem{};mem.cb=sizeof(mem);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem),sizeof(mem));
            auto work=summary(frameWork),frame=summary(intervals),paint=summary(probe->paints);
            std::ostringstream report;report<<"{\"entry\":\""<<(code?"code":"template")<<"\",\"scenario\":\""<<scenario<<"\",\"elapsed_ms\":"<<elapsed<<",\"frames\":"<<frames<<",\"cpu_ms\":"<<cpuMs()-startCpu<<",\"private_mb\":"<<double(mem.PrivateUsage)/1048576<<",\"working_set_mb\":"<<double(mem.WorkingSetSize)/1048576<<",\"work_mean_ms\":"<<work.first<<",\"work_p95_ms\":"<<work.second<<",\"callback_mean_ms\":"<<frame.first<<",\"callback_p95_ms\":"<<frame.second<<",\"paint_count\":"<<probe->paints.size()<<",\"paint_mean_ms\":"<<paint.first<<",\"paint_p95_ms\":"<<paint.second<<",\"initial_widgets\":"<<initialWidgets<<",\"final_widgets\":"<<mount.styles()->size()<<"}";
            if(!output.empty()){std::ofstream f(output);f<<report.str();}std::cout<<report.str()<<"\n";window.close();
        };
        benchmark=[&](double now) {
            if(!first){first=last=now;startCpu=cpuMs();}
            if(now>last)intervals.push_back(now-last);last=now;
            auto begin=std::chrono::steady_clock::now();
            if(scenario=="updates") {Batch batch;vm.query.set((frames/30)%2?L"节点 1":L"");vm.name.set(L"工作空间 "+std::to_wstring(frames));}
            mount.flush();
            frameWork.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());++frames;
            if(now-first<seconds*1000) {window.requestAnimationFrame(benchmark);return;}
            report(now-first);
        };
        const int exit=app.run(Element(Node(probe),"Page"),[&] {
            mount.flush();
            if(state=="focus") {
                std::function<void(std::shared_ptr<Widget>)> focus=[&](auto w) {if(w->accessibleName()==L"工作空间名称")window.requestFocus(w.get());if(auto v=std::dynamic_pointer_cast<View>(w))for(auto& c:v->children())focus(c);};focus(root.widget);
            }
            if(!capture.empty())window.requestAnimationFrame([&](double){window.post([&]{window.prepareLayoutSnapshot();if(!window.captureFramePng(wide(capture)))result=3;window.close();});});
            else if(seconds>0) {
                auto start=[&] {
                window.prepareLayoutSnapshot();probe->paints.clear();
                if(scenario=="updates")window.requestAnimationFrame(benchmark);
                else {
                    startCpu=cpuMs();auto sender=app.dispatcher();auto begin=std::chrono::steady_clock::now();
                    std::thread([sender,seconds,begin,report] {
                        auto end=begin+std::chrono::duration<double>(seconds);
                        while(!sender.cancelled() && std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(25));
                        sender.post([begin,report]{report(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());});
                    }).detach();
                }
                };
                auto sender=app.dispatcher();std::thread([sender,start] {
                    for(int i=0;i<20 && !sender.cancelled();++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    sender.post(start);
                }).detach();
            }
        });
        watcher.reset();return result?result:exit;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
