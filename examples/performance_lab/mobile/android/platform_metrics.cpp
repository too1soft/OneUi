#include "platform_metrics.hpp"
#include <fstream>
#include <time.h>
#include <unistd.h>

double labProcessCpuMs() {
    timespec time{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time) != 0)
        throw std::runtime_error("Android process CPU clock unavailable");
    return time.tv_sec * 1000. + time.tv_nsec / 1000000.;
}
LabProcessMemory labProcessMemory() {
    std::ifstream stat("/proc/self/statm");
    unsigned long size = 0, resident = 0;
    const auto page = sysconf(_SC_PAGESIZE);
    if (!(stat >> size >> resident) || page <= 0)
        throw std::runtime_error("Android resident memory unavailable");
    LabProcessMemory result;
    result.WorkingSetSize = double(resident) * double(page);
    return result; // Private commit is not available from statm; preserve NaN.
}
