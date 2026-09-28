#include "plots.hpp"
#include <iostream>

static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Resources {
  DWORD handles=0,gdi=0,user=0;
  static Resources read(){Resources r;require(GetProcessHandleCount(GetCurrentProcess(),&r.handles),"Handle counter failed");
    r.gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);r.user=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);return r;}
};
int main(int argc,char** argv) {
  try {
    const std::string mode=argc>1?argv[1]:"gpu";
    require(mode=="gpu" || mode=="cpu" || mode=="failed-gpu","Invalid lifecycle mode");
    SetEnvironmentVariableW(L"ONEUI_ENABLE_GPU",mode=="cpu"?L"0":L"1");
    SetEnvironmentVariableW(L"ONEUI_LAB_FAIL_GPU_INIT",mode=="failed-gpu"?L"1":nullptr);
    Resources warm;
    for(int cycle=0;cycle<=30;++cycle) {
      Model model;require(model.particleMode==ParticleMode::Mesh,"Demo did not default to mesh");
      model.load=1;
      auto window=Window::create(L"Particle window lifecycle",640,600);
      auto plot=std::make_shared<Plot>(model,PlotKind::Particles);
      window->setContent(plot);window->initialize();window->show();
      window->setContentScale(cycle%2?1.25f:1.5f);
      window->requestRedraw();UpdateWindow(static_cast<HWND>(window->nativeHandle()));
      const auto info=window->rendererInfo();
      if(mode=="gpu")require(info.backend==RenderBackend::OpenGL && model.meshDraws>0 &&
        std::string(model.meshReason)=="mesh","GPU lifecycle used fallback");
      else require(info.backend==RenderBackend::Software && model.meshDraws==0 && model.meshFallbacks>0 &&
        info.reason==(mode=="cpu"?"disabled-by-environment":"opengl-context-failed"),"Raster fallback mismatch");
      auto token=std::make_shared<int>(7);std::weak_ptr<int> pending=token;
      require(window->post([token]{}),"Open window refused callback");token.reset();
      window->close();require(!window->post([]{}),"Closed window accepted callback");
      window.reset();plot.reset();
      require(pending.expired(),"Closed window retained queued callback");
      if(cycle==0)warm=Resources::read();
    }
    const auto end=Resources::read();
    std::cout<<"mode="<<mode<<" cycles=30 warm_handles="<<warm.handles<<" end_handles="<<end.handles
      <<" warm_gdi="<<warm.gdi<<" end_gdi="<<end.gdi<<" warm_user="<<warm.user<<" end_user="<<end.user<<'\n';
    require(end.handles<=warm.handles+4 && end.gdi<=warm.gdi+2 && end.user<=warm.user+2,"Window resources grew after warmup");
    std::cout<<"Default mode, fallback, close and pending callback lifecycle passed\n";return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
