#include "performance_page.hpp"
#include "lab_process.hpp"
#include "demo_shell.hpp"
#include "desktop_integration.hpp"
#include <iostream>
#ifdef _WIN32
#include <shellapi.h>
#else
#include <codecvt>
#include <locale>
#include <cstdlib>
#endif

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
#else
int main(int argc, char** utf8argv) {
#endif
  try {
#ifdef _WIN32
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) throw std::runtime_error("Unable to read arguments");
    std::vector<std::wstring> arguments(argv,argv+argc);
    LocalFree(argv);
#else
    std::vector<std::wstring> arguments;
    for(int i=0;i<argc;++i) arguments.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(utf8argv[i]));
#endif
    if(auto command=lab::desktopIntegrationCommand(arguments))return *command;
    bool selfTest=false;
    Options o=parseLabOptions(arguments,selfTest);
    lab::setResultsDirectory(o.output);
    if (selfTest) {
      std::vector<double> values(94, 10);
      values.insert(values.end(), 6, 50);
      auto s = summarize(values);
      bool ok = std::abs(s.mean - 12.4) < 1e-9 && s.p95 == 50 && s.p99 == 50 &&
                s.max == 50 && summarize({}).fps == 0;
      // The root reapplies its inherited text environment during paint. A
      // second identical application must not dirty hidden metric labels.
      auto probeWindow = Window::create(L"Font propagation test", 1050, 720);
      {
        Lab probe(*probeWindow, Options{});
        int invalidations = 0;
        probe.setInvalidator([&] { ++invalidations; });
        probe.setTextEnvironment(L"OneUI test inherited font", 2.f);
        invalidations = 0;
        probe.setTextEnvironment(L"OneUI test inherited font", 2.f);
        ok = ok && invalidations == 0;
        probe.setInvalidator({});
      }
      std::cout << (ok ? "Telemetry tests passed\n"
                       : "Telemetry tests failed\n");
      return ok ? 0 : 1;
    }
    // Auto inherits the process environment. Explicit modes override it before initialization.
#ifdef _WIN32
    if(o.renderer!="auto" && !SetEnvironmentVariableW(L"ONEUI_ENABLE_GPU",o.renderer=="cpu"?L"0":L"1"))throw std::runtime_error("Unable to select renderer");
#else
    if(o.renderer!="auto" && setenv("ONEUI_ENABLE_GPU",o.renderer=="cpu"?"0":"1",1)!=0)throw std::runtime_error("Unable to select renderer");
#endif
    std::unique_ptr<Application> application;
    if(!o.legacy){application=std::make_unique<Application>(lab::applicationIdentity());if(!application->isPrimaryInstance())return 0;}
    WindowOptions wo;
    wo.title = std::wstring(L"OneUI 试验台 · ")+(o.templateEntry?L".one":L"C++")+(o.dev?L" · 样式预览":L"");
    wo.width = o.width;
    wo.height = o.height;
    auto window = Window::create(wo);
    if(o.devtools)window->enableDeveloperTools();
#ifdef __APPLE__
    window->setDefaultFontFamily(L"PingFang SC");
#elif defined(_WIN32)
    window->setDefaultFontFamily(L"Microsoft YaHei UI");
#else
    window->setDefaultFontFamily(L"Noto Sans CJK SC");
#endif
    window->setMinimumClientSize((!o.legacy||o.editor)?Size{640,560}:Size{1050,720});window->setContentScale(o.scale);
    if(!o.legacy) {
      auto shell=std::make_shared<lab::DemoShell>(*window);
      window->setContent(shell);
      shell->setTheme(!o.editorLight,o.compact);
      if(!shell->navigate(o.page))throw std::runtime_error("Unknown page ID");
      window->setRawKeyHandler([shell](const KeyEvent& event){return shell->key(event);});
      application->addWindow(*window,true);
      application->setOnActivation([weak=std::weak_ptr<lab::DemoShell>(shell)](const ApplicationActivation& value){if(auto live=weak.lock())live->applicationActivated(value);});
      window->initialize();window->centerOnActiveMonitor();window->show();window->activate();
      int captureResult=0;
      if(!o.snapshot.empty())window->post([&]{
        if(!o.snapshot.parent_path().empty())std::filesystem::create_directories(o.snapshot.parent_path());
        window->prepareLayoutSnapshot();
        if(!window->captureFramePng(o.snapshot.wstring()))captureResult=1;
        if(o.snapshotExit)application->requestQuit();
      });
      auto code=application->run();
      application->setOnActivation({});window->setRawKeyHandler({});window->setContent({});shell.reset();
      return captureResult?captureResult:code;
    }
    auto lab = std::make_shared<Lab>(*window, o);
    window->setContent(lab);
    if(o.desktopMenus)lab->enableDesktopMenus();
    window->setRawKeyHandler([lab](const KeyEvent &e) { return lab->key(e); });
    window->setOnCloseRequested([lab](bool forced){lab->requestClose(forced);});
    window->initialize();
    window->centerOnActiveMonitor();
    window->show();
    window->activate();
    lab->begin();
    int code = window->run();
    lab->diagnosticStop=true;lab->closeEditor();window->setRawKeyHandler({});window->setOnCloseRequested({});
    return lab->exitCode() ? lab->exitCode() : code;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
