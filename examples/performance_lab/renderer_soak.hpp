#pragma once
#include "renderer_benchmark.hpp"

// Bounded diagnostics: stream interval aggregates, never retain per-frame samples.
struct RendererSoak {
  std::ofstream csv;
  oneui::RendererInfo previous;
  double previousSeconds=0, previousCpu=0;
  std::uint64_t previousDraws=0, previousFallbacks=0;
  bool active=false;
  void begin(oneui::Window& window,const Model& model,const std::filesystem::path& output) {
    std::filesystem::create_directories(output);
    csv.open(output/"stability.csv");csv.exceptions(std::ios::failbit|std::ios::badbit);
    csv<<"seconds,interval_s,backend,backend_changes,paint_count,paint_mean_ms,content_mean_ms,submit_mean_ms,cpu_percent,working_set_mib,private_mib,gpu_cache_mib,handles,gdi_objects,user_objects,mesh_draws,mesh_fallbacks,width,height,dpi_scale\n";
    previous=window.rendererInfo();previousCpu=labProcessCpuMs();
    previousDraws=model.meshDraws;previousFallbacks=model.meshFallbacks;active=true;
    std::ofstream metadata(output/"stability-info.txt");
    metadata.exceptions(std::ios::failbit|std::ios::badbit);
    metadata<<"particle_mode="<<particleModeName(model.particleMode)<<"\nbackend="<<backendName(previous.backend)
      <<"\ndevice="<<previous.device<<"\nreason="<<previous.reason<<"\nparticles="<<model.particles()
      <<"\nstartup_mesh_fallbacks="<<model.meshFallbacks<<"\n";
  }
  void sample(oneui::Window& window,const Model& model,double seconds) {
    auto info=window.rendererInfo();const double interval=seconds-previousSeconds;
    if(interval<=0)return;
    PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);DWORD handles=0;
    if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)) ||
       !GetProcessHandleCount(GetCurrentProcess(),&handles)) throw std::runtime_error("Stability counters unavailable");
    const auto caches=window.rendererMemoryInfo();const auto paints=info.paints-previous.paints;
    const double divisor=std::max<std::uint64_t>(1,paints),cpu=labProcessCpuMs();
    csv<<std::setprecision(10)<<seconds<<','<<interval<<','<<backendName(info.backend)<<','
      <<info.backendChanges-previous.backendChanges<<','<<paints<<','
      <<(info.paintMs-previous.paintMs)/divisor<<','<<(info.contentMs-previous.contentMs)/divisor<<','
      <<(info.submitMs-previous.submitMs)/divisor<<','
      <<(cpu-previousCpu)/interval/10/std::max(1u,std::thread::hardware_concurrency())<<','
      <<memory.WorkingSetSize/1048576.0<<','<<memory.PrivateUsage/1048576.0<<','<<caches.gpuCacheBytes/1048576.0<<','
      <<handles<<','<<GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<<','<<GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)<<','
      <<model.meshDraws-previousDraws<<','<<model.meshFallbacks-previousFallbacks<<','
      <<window.clientSize().width<<','<<window.clientSize().height<<','<<window.dpiScale()<<'\n';
    csv.flush();previous=info;previousSeconds=seconds;previousCpu=cpu;
    previousDraws=model.meshDraws;previousFallbacks=model.meshFallbacks;
  }
};
