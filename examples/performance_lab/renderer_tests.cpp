#include <oneui/oneui.h>
#include <iostream>
#include <windows.h>

int main() {
  using namespace oneui;
  SetEnvironmentVariableW(L"ONEUI_ENABLE_GPU",L"0");
  auto window=Window::create(L"Renderer diagnostics test",640,480);
  if(window->rendererInfo().backend!=RenderBackend::Unknown)return 1;
  const auto initialCaches=window->rendererMemoryInfo();
  if(initialCaches.gpuCacheAvailable || initialCaches.retainedSurfaceBytes ||
      window->rendererInfo().backend!=RenderBackend::Unknown || window->rendererInfo().paints) return 7;
  auto content=std::make_shared<Label>(L"Native software renderer");
  window->setContent(content);
  window->initialize();
  window->show();
  window->prepareLayoutSnapshot();
  auto info=window->rendererInfo();
  if(info.backend!=RenderBackend::Software || info.reason!="disabled-by-environment" || info.vsync || !info.paints || info.paintMs<=0 || info.contentMs<0 || info.submitMs!=0){
    std::cerr<<"backend="<<static_cast<int>(info.backend)<<" reason="<<info.reason<<" paints="<<info.paints<<" paintMs="<<info.paintMs<<'\n';return 2;
  }
  // Reading diagnostics must not allocate a surface, repaint or reinitialize GPU.
  const auto unchanged=window->rendererInfo();
  const auto caches=window->rendererMemoryInfo();
  if(!caches.cpuCachesAvailable || caches.gpuCacheAvailable || caches.gpuCacheBytes ||
      caches.gpuPurgeableBytes || caches.gpuResourceCount || caches.gpuCacheLimitBytes || !caches.retainedSurfaceBytes) return 5;
  const auto afterCaches=window->rendererInfo();
  if(afterCaches.paints!=unchanged.paints || afterCaches.backendChanges!=unchanged.backendChanges ||
      afterCaches.backend!=unchanged.backend) return 6;
  if(unchanged.paints!=info.paints || unchanged.backendChanges!=info.backendChanges)return 3;
  content->setText(L"Changed content");window->requestRedraw();
  UpdateWindow(static_cast<HWND>(window->nativeHandle()));
  const auto updated=window->rendererInfo();
  if(updated.paints<=info.paints || updated.paintMs<info.paintMs || updated.backendChanges!=info.backendChanges)return 4;
  window->close();
  std::cout<<"Renderer diagnostics: forced raster, actual surface, monotonic counters and read-only snapshots passed\n";
  return 0;
}
