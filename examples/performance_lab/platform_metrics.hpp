#pragma once
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

struct LabProcessMemory {
  double WorkingSetSize = std::numeric_limits<double>::quiet_NaN();
  // Windows private commit has no equivalent in the Mac sampler. Report NaN.
  double PrivateUsage = std::numeric_limits<double>::quiet_NaN();
};
#ifdef _WIN32
inline double labProcessCpuMs() {
  FILETIME created{}, ended{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user))
    throw std::runtime_error("GetProcessTimes failed");
  const auto ticks = [](FILETIME t) {
    return (static_cast<unsigned long long>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
  };
  return (ticks(kernel) + ticks(user)) / 10000.0;
}
inline LabProcessMemory labProcessMemory() {
  PROCESS_MEMORY_COUNTERS_EX memory{};
  memory.cb = sizeof(memory);
  if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
    throw std::runtime_error("GetProcessMemoryInfo failed");
  return {double(memory.WorkingSetSize), double(memory.PrivateUsage)};
}
#else
double labProcessCpuMs();
LabProcessMemory labProcessMemory();
void labSetClientSize(void* nativeWindow, double width, double height);
#endif
