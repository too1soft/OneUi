#pragma once

#include "../../internal/scroll_trace.h"
#include <windows.h>

namespace oneui::internal {

struct ViewMountDispatchSample {
    ULONG64 cycles = 0;
    ULONG64 waitReasons = 0;
    DWORD switches = 0;
    DWORD processor = 0;
    bool valid = false;
};

class ViewMountDispatchProfiler {
public:
    ViewMountDispatchProfiler() noexcept {
        if (EnableThreadProfiling(GetCurrentThread(), THREAD_PROFILING_FLAG_DISPATCH, 0, &handle_) !=
            ERROR_SUCCESS)
            handle_ = nullptr;
    }
    ~ViewMountDispatchProfiler() { if (handle_) DisableThreadProfiling(handle_); }
    ViewMountDispatchProfiler(const ViewMountDispatchProfiler&) = delete;
    ViewMountDispatchProfiler& operator=(const ViewMountDispatchProfiler&) = delete;

    ViewMountDispatchSample sample() const noexcept {
        if (!handle_) return {};
        PERFORMANCE_DATA data{};
        data.Size = sizeof(data);
        data.Version = PERFORMANCE_DATA_VERSION;
        if (ReadThreadProfilingData(handle_, READ_THREAD_PROFILING_FLAG_DISPATCHING, &data) !=
            ERROR_SUCCESS)
            return {};
        PROCESSOR_NUMBER processor{};
        GetCurrentProcessorNumberEx(&processor);
        const DWORD processorId = static_cast<DWORD>(processor.Group) * 64 + processor.Number;
        return {data.CycleTime, data.WaitReasonBitMap, data.ContextSwitchCount, processorId, true};
    }

private:
    HANDLE handle_ = nullptr;
};

inline ViewMountDispatchSample sampleViewMountDispatch() noexcept {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_VIEW_MOUNT_DISPATCH_TRACE", value, 2) == 1 &&
            value[0] == L'1';
    }();
    if (!enabled) return {};
    thread_local ViewMountDispatchProfiler profiler;
    return profiler.sample();
}

inline void writeViewMountDispatch(const ViewMountDispatchSample& before,
                                  const ViewMountDispatchSample& after,
                                  std::uintptr_t mount, double elapsedMs) {
    if (!before.valid || !after.valid || after.cycles < before.cycles) return;
    ScrollTraceEvent event;
    event.component = "view_mount_dispatch";
    event.phase = "instantiate_thread";
    event.object = mount;
    event.durationMs = elapsedMs;
    event.detailA = static_cast<double>(after.cycles - before.cycles);
    event.detailB = static_cast<double>(after.switches - before.switches);
    event.inputDelta = before.processor;
    event.offset = after.processor;
    event.target = static_cast<DWORD>(after.waitReasons);
    event.velocity = static_cast<DWORD>(after.waitReasons >> 32);
    writeScrollTrace(event);
}

} // namespace oneui::internal
