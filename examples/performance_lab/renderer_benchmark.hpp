#pragma once
#include <oneui/platform/window.h>
#include "telemetry.hpp"

inline const char* backendName(oneui::RenderBackend backend) {
  switch (backend) {
  case oneui::RenderBackend::Metal: return "metal";
  case oneui::RenderBackend::OpenGL: return "opengl";
  case oneui::RenderBackend::Software: return "raster";
  default: return "unknown";
  }
}
struct RendererBenchmark {
  bool active = false;
  Clock::time_point started;
  double cpuStart = 0;
  oneui::RendererInfo initial;
  std::vector<double> contentSamples, frameIntervals;
  Clock::time_point previousPaint{};
  std::uint64_t initialMeshDraws=0, initialMeshFallbacks=0, initialMeshVertices=0;
  void begin(oneui::Window& window, std::uint64_t meshDraws, std::uint64_t meshFallbacks, std::uint64_t meshVertices) {
    initialMeshDraws=meshDraws;initialMeshFallbacks=meshFallbacks;initialMeshVertices=meshVertices;
    initial = window.rendererInfo();
    cpuStart = labProcessCpuMs();
    started = Clock::now();
    contentSamples.clear();
    frameIntervals.clear(); previousPaint={};
    active = true;
  }
  void finish(oneui::Window& window, const std::filesystem::path& output,
              const std::string& requested, const std::string& scene, const char* particleMode,
              std::uint64_t meshDraws, std::uint64_t meshFallbacks, std::uint64_t meshVertices, const char* meshReason,
              const char* meshLastFallback) {
    const double elapsed = ms(Clock::now() - started);
    const double cpu = labProcessCpuMs() - cpuStart;
    active = false;
    const auto info = window.rendererInfo();
    const auto paints = info.paints - initial.paints;
    const double divisor = std::max<std::uint64_t>(1, paints);
    const auto memory = labProcessMemory();
    const auto caches = window.rendererMemoryInfo();
    std::filesystem::create_directories(output);
    std::ofstream out(output / "renderer-performance.txt");
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << std::setprecision(8)
        << "requested=" << requested << "\nscene=" << scene
        << "\nparticle_mode=" << particleMode
        << "\nmesh_draws_total=" << meshDraws << "\nmesh_fallbacks_total=" << meshFallbacks
        << "\nmesh_vertices_total=" << meshVertices << "\nmesh_reason=" << meshReason
        << "\nmesh_draws_sample=" << meshDraws-initialMeshDraws
        << "\nmesh_fallbacks_sample=" << meshFallbacks-initialMeshFallbacks
        << "\nmesh_vertices_sample=" << meshVertices-initialMeshVertices
        << "\nmesh_last_fallback=" << meshLastFallback
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
        << "\ncontent_p99_ms=" << summarize(contentSamples).p99
        << "\nframe_interval_mean_ms=" << summarize(frameIntervals).mean
        << "\nframe_interval_p95_ms=" << summarize(frameIntervals).p95
        << "\nframe_interval_p99_ms=" << summarize(frameIntervals).p99
        << "\nframe_callback_hz=" << summarize(frameIntervals).fps
        << "\nphysical_width=" << window.clientSize().width * window.dpiScale()
        << "\nphysical_height=" << window.clientSize().height * window.dpiScale()
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
    explicit PaintSample(RendererBenchmark& value) : owner(value), enabled(value.active), start(enabled ? Clock::now() : Clock::time_point{}) {
      if (enabled) {
        if (owner.previousPaint != Clock::time_point{}) owner.frameIntervals.push_back(ms(start-owner.previousPaint));
        owner.previousPaint=start;
      }
    }
    ~PaintSample() { if (enabled) owner.contentSamples.push_back(ms(Clock::now() - start)); }
  };
};
