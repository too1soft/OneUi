#pragma once
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <psapi.h>
#include <sstream>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;
inline double ms(Clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}
struct Summary {
  double fps = 0, mean = 0, p95 = 0, p99 = 0, max = 0;
};
inline Summary summarize(std::vector<double> values) {
  if (values.empty())
    return {};
  Summary s;
  s.mean = std::accumulate(values.begin(), values.end(), 0.) / values.size();
  s.fps = s.mean > 0 ? 1000. / s.mean : 0.;
  std::sort(values.begin(), values.end());
  s.p95 = values[static_cast<size_t>(std::ceil(values.size() * 0.95)) - 1];
  s.p99 = values[static_cast<size_t>(std::ceil(values.size() * 0.99)) - 1];
  s.max = values.back();
  return s;
}
struct Monitor {
  std::atomic<bool> stop{false};
  std::atomic<double> cpu{0}, memory{0};
  std::thread worker;
  static unsigned long long ticks(FILETIME t) {
    return (static_cast<unsigned long long>(t.dwHighDateTime) << 32) |
           t.dwLowDateTime;
  }
  Monitor()
      : worker([this] {
          unsigned long long previous = 0;
          auto previousTime = Clock::now();
          bool first = true;
          const double cores =
              std::max(1u, std::thread::hardware_concurrency());
          while (!stop.load()) {
            FILETIME created, exit, kernel, user;
            const auto now = Clock::now();
            if (GetProcessTimes(GetCurrentProcess(), &created, &exit, &kernel,
                                &user)) {
              auto total = ticks(kernel) + ticks(user);
              if (!first)
                cpu = (total - previous) / 10000. / ms(now - previousTime) /
                      cores * 100.;
              previous = total;
              previousTime = now;
              first = false;
            }
            PROCESS_MEMORY_COUNTERS info{};
            info.cb = sizeof(info);
            if (GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)))
              memory = info.WorkingSetSize / 1048576.;
            for (int i = 0; i < 10 && !stop.load(); ++i)
              std::this_thread::sleep_for(std::chrono::milliseconds(100));
          }
        }) {}
  ~Monitor() {
    stop = true;
    if (worker.joinable())
      worker.join();
  }
};
struct Sample {
  double dt, cpu, memory;
};
struct Telemetry {
  Monitor monitor;
  std::deque<double> recent;
  std::vector<Sample> capture;
  Clock::time_point previous{}, started = Clock::now(), recordStart{};
  bool hasPrevious = false, recording = false;
  double seconds = 10.;
  void reset() {
    recent.clear();
    hasPrevious = false;
  }
  double frame(bool running) {
    const auto now = Clock::now();
    if (!running) {
      hasPrevious = false;
      return 0.;
    }
    double dt = hasPrevious ? ms(now - previous) : 0.;
    previous = now;
    if (hasPrevious) {
      if (recent.size() == 240)
        recent.pop_front();
      recent.push_back(dt);
      if (recording)
        capture.push_back({dt, monitor.cpu, monitor.memory});
    }
    hasPrevious = true;
    return dt;
  }
  Summary current() const { return summarize({recent.begin(), recent.end()}); }
  void record(double duration) {
    seconds = duration;
    capture.clear();
    recordStart = Clock::now();
    recording = true;
    hasPrevious = false;
  }
  bool finished() const {
    return recording && ms(Clock::now() - recordStart) >= seconds * 1000.;
  }
  double remaining() const {
    return std::max(0., seconds - ms(Clock::now() - recordStart) / 1000.);
  }
  std::filesystem::path save(const std::filesystem::path &output, int load,
                             int view, int particles, int points, float speed,
                             float width, float height, float dpi) {
    std::filesystem::create_directories(output);
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    auto csv = output / ("oneui-" + std::to_string(stamp) + ".csv");
    std::ofstream f(csv);
    f.exceptions(std::ios::failbit | std::ios::badbit);
    f << "frame,frame_interval_ms,process_cpu_percent_total_machine,working_"
         "set_mb\n"
      << std::fixed << std::setprecision(4);
    std::vector<double> times;
    double cpu = 0, memory = 0;
    size_t longFrames = 0;
    for (size_t i = 0; i < capture.size(); ++i) {
      const auto &r = capture[i];
      f << i << ',' << r.dt << ',' << r.cpu << ',' << r.memory << '\n';
      times.push_back(r.dt);
      cpu += r.cpu;
      memory = std::max(memory, r.memory);
      if (r.dt > 33.334)
        ++longFrames;
    }
    const auto s = summarize(times);
    auto summary = csv;
    summary.replace_extension(".txt");
    std::ofstream out(summary);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << std::fixed << std::setprecision(3)
        << "OneUI Performance Lab\nbuild=Release\nload=" << load
        << "\nview=" << view << "\nparticles=" << particles
        << "\npoints_per_series=" << points << "\nanimation_speed=" << speed
        << "\nclient_width=" << width << "\nclient_height=" << height
        << "\ndpi_scale=" << dpi << "\nframes=" << capture.size()
        << "\nduration_s=" << ms(Clock::now() - recordStart) / 1000.
        << "\nfps=" << s.fps << "\nmean_ms=" << s.mean << "\np95_ms=" << s.p95
        << "\np99_ms=" << s.p99 << "\nmax_ms=" << s.max
        << "\nframes_over_33ms=" << longFrames
        << "\nmean_process_cpu_percent_total_machine="
        << cpu / std::max(size_t(1), capture.size())
        << "\npeak_working_set_mb=" << memory
        << "\n\nUI root paint callback intervals; not GPU duration or "
           "displayed-frame count.\nNative OneUI animation scheduler and "
           "default GPU/raster selection. Consult runtime.log for actual "
           "renderer.\nNative VirtualList owns item strings; GPUI generates "
           "visible rows. Memory includes this data-model difference.\n";
    return csv;
  }
};
