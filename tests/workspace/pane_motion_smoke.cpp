#include "oneui/platform/window.h"
#include "oneui/layout/pane_grid.h"
#include "oneui/controls/terminal_view.h"
#include "oneui/ui_native_host.h"
#include "internal/workspace_frame_probe.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
using namespace oneui;
namespace {
double clockMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Root:View {
    std::shared_ptr<PaneGrid> grid=std::make_shared<PaneGrid>();
    double updateMs=0;
    Root(){add(grid);}
    bool tickAnimations(double now) override {
        auto start=clockMs();bool active=View::tickAnimations(now);updateMs+=clockMs()-start;return active;
    }
    void layoutChildren() override {grid->setFrame(frame());}
};
struct Sample {int scene,phase;double work,present,interval;};
struct Bench {
    std::unique_ptr<Window> window;
    std::shared_ptr<Root> root=std::make_shared<Root>();
    std::vector<std::shared_ptr<PaneView>> panes;
    PaneGridSnapshot snapshot;
    std::vector<Sample> samples;
    int frames=0,scene=0,visible=1,resizes=0,creations=0;
    bool early=false,queued=false;
    double previous=0;
    Bench(bool e):early(e) {
        samples.reserve(4096);snapshot.activeGroup="g";snapshot.activePane="0";
        snapshot.nodes={{"n0","0"}};snapshot.groups={{"g","n0"}};
        for(int i=0;i<16;++i) {
            auto pane=std::make_shared<PaneView>();pane->key=std::to_string(i);
            auto terminal=std::make_shared<TerminalView>();++creations;
            terminal->setCursorBlinking(false);terminal->setFontFamily(L"Consolas");terminal->setFontSize(13);
            std::vector<TerminalCell> cells(80*24);
            for(size_t j=0;j<cells.size();++j)cells[j].text=std::wstring(1,L"oneui workspace 0123456789"[j%24]);
            terminal->setGrid(24,80,std::move(cells));
            terminal->setOnViewportChanged([this](auto){++resizes;});
            auto host=std::make_shared<ui::NativeHostView>(terminal);host->setSettledViewport(true);pane->add(host);panes.push_back(pane);
        }
        WindowOptions options;options.title=L"OneUI workspace motion smoke";options.width=1600;options.height=1000;options.visible=false;
        window=Window::create(options);window->initialize();window->setContent(root);
        window->setDeveloperTools({true,false,16});
        root->grid->setSnapshot(snapshot,panes);
        internal::workspaceFrameProbe=[this](auto timing){frame(timing);};
    }
    ~Bench(){internal::workspaceFrameProbe={};}
    void advance() {
        queued=false;
        if(scene==6){window->close();return;}
        ++scene;
        if(scene<=4) {
            for(int i=0;i<visible;++i) {
                const auto pane=std::to_string(i);auto leaf=std::find_if(snapshot.nodes.begin(),snapshot.nodes.end(),[&](auto& n){return n.pane==pane;});
                auto old=*leaf;auto added=std::to_string(i+visible);leaf->pane.clear();leaf->first=old.id+"a";leaf->second="n"+added;
                leaf->axis=scene%2?DockAxis::Horizontal:DockAxis::Vertical;
                old.id=leaf->first;snapshot.nodes.push_back(old);snapshot.nodes.push_back({"n"+added,added});
            }
            visible*=2;
        } else snapshot.maximized=scene==5?"0":"";
        ++snapshot.revision;root->grid->setSnapshot(snapshot,panes);window->requestRedraw();
    }
    void frame(internal::WorkspaceFrameTiming timing) {
        auto now=clockMs();++frames;auto phase=root->grid->motionPhase();
        if(scene)samples.push_back({scene,int(phase),timing.paintMs-timing.presentMs+root->updateMs,timing.presentMs,previous?now-previous:0});
        previous=now;root->updateMs=0;
        if(!queued && ((!scene && frames>=(early?1:16)) || (scene && phase==PaneMotionPhase::Idle))) {
            queued=true;window->post([this]{advance();});
        } else if(!scene)window->requestAnimationFrame([this](double){window->requestRedraw();});
    }
    void report(const char* path) {
        std::ofstream out(path);out<<"scene,phase,work_submit_ms,present_ms,interval_ms\n";
        for(auto& s:samples)out<<s.scene<<','<<s.phase<<','<<s.work<<','<<s.present<<','<<s.interval<<'\n';
        auto renderer=window->rendererInfo();
        std::cout<<"renderer="<<int(renderer.backend)<<" device="<<renderer.device<<" frames="<<frames<<" creates="<<creations<<" resizes="<<resizes<<'\n';
        for(int i=1;i<=6;++i){std::vector<double> values;for(auto& s:samples)if(s.scene==i && s.phase!=int(PaneMotionPhase::WarmFrame))values.push_back(s.work);
            if(values.empty())continue;std::sort(values.begin(),values.end());
            std::cout<<"scene="<<i<<" frames="<<values.size()<<" max="<<values.back()<<" p95="<<values[size_t((values.size()-1)*.95)]<<" slow="<<std::count_if(values.begin(),values.end(),[](double v){return v>16;})<<'\n';}
    }
};
}
int main(int argc,char** argv){try {Bench bench(argc>2 && std::string(argv[2])=="early");bench.window->show();bench.window->run();bench.report(argc>1?argv[1]:"workspace-smoke.csv");return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
