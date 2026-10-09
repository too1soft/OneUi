#include "platform_metrics.hpp"
#include <fstream>
#include <sys/resource.h>
#include <unistd.h>

double labProcessCpuMs() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) throw std::runtime_error("getrusage failed");
    return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000.0 +
           (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000.0;
}
LabProcessMemory labProcessMemory() {
    std::ifstream status("/proc/self/statm");
    unsigned long virtualPages = 0, residentPages = 0;
    const auto pageSize = sysconf(_SC_PAGESIZE);
    if (!(status >> virtualPages >> residentPages) || pageSize <= 0)
        throw std::runtime_error("Process memory counters unavailable");
    LabProcessMemory result;
    result.WorkingSetSize = double(residentPages) * pageSize;
    return result;
}
