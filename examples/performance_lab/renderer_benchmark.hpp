#pragma once
#include <oneui/platform/window.h>
#include "telemetry.hpp"

inline const char* backendName(oneui::RenderBackend backend) {
  switch (backend) {
  case oneui::RenderBackend::OpenGL: return "opengl";
  case oneui::RenderBackend::Software: return "raster";
  default: return "unknown";
  }
}
inline double labProcessCpuMs() {
  FILETIME created{}, ended{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user))
    throw std::runtime_error("GetProcessTimes failed");
  return (Monitor::ticks(kernel) + Monitor::ticks(user)) / 10000.0;
}
struct RendererBenchmark {
  bool active = false;
  Clock::time_point started;
  double cpuStart = 0;
  oneui::RendererInfo initial;
  std::vector<double> contentSamples;
  void begin(oneui::Window& window) {
    initial = window.rendererInfo();
    cpuStart = labProcessCpuMs();
    started = Clock::now();
    contentSamples.clear();
    active = true;
  }
  void finish(oneui::Window& window, const std::filesystem::path& output,
              const std::string& requested, const std::string& scene, const char* particleMode) {
    const double elapsed = ms(Clock::now() - started);
    const double cpu = labProcessCpuMs() - cpuStart;
    active = false;
    const auto info = window.rendererInfo();
    const auto paints = info.paints - initial.paints;
    const double divisor = std::max<std::uint64_t>(1, paints);
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
      throw std::runtime_error("GetProcessMemoryInfo failed");
    const auto caches = window.rendererMemoryInfo();
    std::filesystem::create_directories(output);
    std::ofstream out(output / "renderer-performance.txt");
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << std::setprecision(8)
        << "requested=" << requested << "\nscene=" << scene
        << "\nparticle_mode=" << particleMode
        << "\nbackend=" << backendName(info.backend)
        << "\ndevice=" << info.device << "\nreason=" << info.reason
        << "\nvsync=" << info.vsync
        << "\nbackend_changes=" << info.backendChanges - initial.backendChanges
        << "\nelapsed_ms=" << elapsed << "\ncpu_ms=" << cpu
        << "\ncpu_percent=" << 100 * cpu / elapsed / std::max(1u, std::thread::hardware_concurrency())
        << "\nworking_set_mib=" << memory.WorkingSetSize / 1048576.0
        << "\nprivate_mib=" << memory.PrivateUsage / 1048576.0
        << "\ncpu_caches_available=" << caches.cpuCachesAvailable
        << "\ngpu_cache_available=" << caches.gpuCacheAvailable
        << "\nskia_cpu_font_cache_mib=" << caches.cpuFontCacheBytes / 1048576.0
        << "\nskia_cpu_resource_cache_mib=" << caches.cpuResourceCacheBytes / 1048576.0
        << "\nskia_gpu_cache_mib=" << caches.gpuCacheBytes / 1048576.0
        << "\nskia_gpu_purgeable_mib=" << caches.gpuPurgeableBytes / 1048576.0
        << "\nskia_gpu_cache_limit_mib=" << caches.gpuCacheLimitBytes / 1048576.0
        << "\nskia_gpu_resource_count=" << caches.gpuResourceCount
        << "\nretained_surface_estimate_mib=" << caches.retainedSurfaceBytes / 1048576.0
        << "\npaint_count=" << paints
        << "\npaint_mean_ms=" << (info.paintMs - initial.paintMs) / divisor
        << "\ncontent_mean_ms=" << (info.contentMs - initial.contentMs) / divisor
        << "\ncontent_p95_ms=" << summarize(contentSamples).p95
        << "\nsubmit_mean_ms=" << (info.submitMs - initial.submitMs) / divisor
        << "\nblit_mean_ms=" << (info.blitMs - initial.blitMs) / divisor
        << "\nwidth=" << window.clientSize().width << "\nheight=" << window.clientSize().height
        << "\ndpi_scale=" << window.dpiScale() << '\n';
    std::ofstream raw(output / "content-paint.csv");
    raw.exceptions(std::ios::failbit | std::ios::badbit);
    raw << "content_ms\n";
    for (const auto sample : contentSamples) raw << sample << '\n';
  }
  struct PaintSample {
    RendererBenchmark& owner;
    bool enabled;
    Clock::time_point start;
    explicit PaintSample(RendererBenchmark& value) : owner(value), enabled(value.active), start(enabled ? Clock::now() : Clock::time_point{}) {}
    ~PaintSample() { if (enabled) owner.contentSamples.push_back(ms(Clock::now() - start)); }
  };
};
