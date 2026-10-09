#include "platform_metrics.hpp"
#include <sys/resource.h>
#include <mach/mach.h>
#import <AppKit/AppKit.h>

double labProcessCpuMs() {
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0)
    throw std::runtime_error("getrusage failed");
  return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000.0 +
         (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000.0;
}

LabProcessMemory labProcessMemory() {
  mach_task_basic_info_data_t info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
    throw std::runtime_error("task_info failed");
  LabProcessMemory result;
  result.WorkingSetSize = double(info.resident_size);
  return result;
}

void labSetClientSize(void* nativeWindow, double width, double height) {
  NSWindow* window = (__bridge NSWindow*)nativeWindow;
  [window setContentSize:NSMakeSize(width, height)];
}
