#include "oneui/platform/window.h"
#include "oneui/ui_compose.h"
#include <windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace oneui;
#define CHECK(x) do{if(!(x))throw std::runtime_error("Failed: " #x);}while(0)
struct SlowWidget:Widget {
    bool slow=true;
    void paint(Canvas& canvas)override {
        if(slow){std::this_thread::sleep_for(std::chrono::milliseconds(40));slow=false;}
        canvas.fillRect(frame(),Color{230,240,233});
    }
};
int main(int argc,char** argv){try {
    SetEnvironmentVariableW(L"ONEUI_ENABLE_GPU",argc>1 && std::string(argv[1])=="gpu"?L"1":L"0");
    auto window=Window::create(L"OneUI built-in developer tools acceptance",700,500);
    CHECK(window->developerSnapshot().supported && !window->developerSnapshot().enabled);
    CHECK(window->setDeveloperTools({true,false,1000./60}));
    auto root=std::make_shared<View>();auto slow=std::make_shared<SlowWidget>();
    slow->setDiagnosticSource({"SlowFixture","fixtures/中文\".one",17});slow->setFrame({20,20,200,70});root->add(slow);
    auto content=std::make_shared<SlowWidget>();content->slow=false;content->setPreferredSize({200,60});
    auto reveal=std::make_shared<Reveal>(content);reveal->setFrame({20,110,200,60});reveal->setDiagnosticSource({"Reveal","fixtures/native.one",24});root->add(reveal);
    window->setContent(root);window->initialize();window->show();window->prepareLayoutSnapshot();
    auto snapshot=window->developerSnapshot();CHECK(snapshot.enabled && snapshot.slowPaints>=1);
    bool attributed=false;for(const auto& issue:snapshot.issues)if(issue.source.component=="SlowFixture" && issue.source.line==17)attributed=true;CHECK(attributed);
    CHECK(window->exportDeveloperReport(argc>2?std::filesystem::path(argv[2]).wstring():L"devtools-test.json"));
    CHECK(window->rendererInfo().backend==(argc>1 && std::string(argv[1])=="gpu"?RenderBackend::OpenGL:RenderBackend::Software));
    bool passed=false;std::string failure;std::uint64_t idleFrames=0;
    std::thread worker([&]{
        std::this_thread::sleep_for(std::chrono::milliseconds(250));window->post([&]{reveal->setOpen(false);});
        std::this_thread::sleep_for(std::chrono::milliseconds(450));window->post([&]{reveal->setOpen(true);});
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
        window->post([&]{idleFrames=window->developerSnapshot().frames;});
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        window->post([&]{
            try {
                CHECK(window->developerSnapshot().frames==idleFrames);
                auto s=window->developerSnapshot();CHECK(s.motionReversals==0);CHECK(s.frames>5);CHECK(s.motionSamples>5);
                // Toggle overlay through the built-in window key route.
                SendMessageW(static_cast<HWND>(window->nativeHandle()),WM_KEYDOWN,VK_F12,0);
                CHECK(window->developerSnapshot().overlay);
                SendMessageW(static_cast<HWND>(window->nativeHandle()),WM_KEYDOWN,VK_F12,0);
                CHECK(!window->developerSnapshot().overlay);
                CHECK(window->setDeveloperTools({false,false,1000./60}));
                CHECK(!window->developerSnapshot().enabled);
                CHECK(window->enableDeveloperTools());CHECK(window->developerSnapshot().frames==0);
                CHECK(window->setDeveloperTools({false,false,1000./60}));
                passed=true;
            }catch(const std::exception& e){failure=e.what();}
            window->close();
        });
    });
    window->run();worker.join();if(!passed)throw std::runtime_error(failure);
    std::cout<<"Native developer tools: one-call activation, slow widget attribution, report export, Reveal, F12, disable/re-enable and close passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
