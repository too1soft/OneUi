#pragma once

// Opt-in, value-free allocation/cache diagnostics for an owned UI snapshot.
// Heap walking is intentionally excluded from normal rendering and profiling
// timing runs. Counters are heap blocks, not proof of application object liveness.
#include "oneui/platform/window.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace oneui::capi {
inline void appendMemorySnapshot(std::ostream& stream, const oneui::Window& window) {
    const char* enabled = std::getenv("ONEUI_MEMORY_DIAGNOSTICS");
    if (!enabled || std::string_view(enabled) != "1") return;
    const auto memory = window.rendererMemoryInfo();
    stream << ",\"memory\":{\"cpuFontCacheBytes\":" << memory.cpuFontCacheBytes
           << ",\"cpuResourceCacheBytes\":" << memory.cpuResourceCacheBytes
           << ",\"gpuCacheBytes\":" << memory.gpuCacheBytes
           << ",\"gpuPurgeableBytes\":" << memory.gpuPurgeableBytes
           << ",\"gpuCacheLimitBytes\":" << memory.gpuCacheLimitBytes
           << ",\"gpuResourceCount\":" << memory.gpuResourceCount
           << ",\"retainedSurfaceBytes\":" << memory.retainedSurfaceBytes;
#if defined(_WIN32)
    struct HeapCounters {
        std::uint64_t busyBytes = 0, busyBlocks = 0, committedBytes = 0;
        std::uint64_t heaps = 0, failedHeaps = 0;
        std::array<std::uint64_t, 32> bytesByPower{}, blocksByPower{};
    } counters;
    // Fixed storage keeps the walk free of allocations while a heap is locked.
    std::array<HANDLE, 128> heaps{};
    const DWORD count = GetProcessHeaps(static_cast<DWORD>(heaps.size()), heaps.data());
    if (count > heaps.size() || count == 0) counters.failedHeaps++;
    for (DWORD i = 0; i < (std::min)(count, static_cast<DWORD>(heaps.size())); ++i) {
        if (!HeapLock(heaps[i])) { counters.failedHeaps++; continue; }
        PROCESS_HEAP_ENTRY entry{};
        std::uint64_t entries = 0;
        while (HeapWalk(heaps[i], &entry)) {
            if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
                counters.busyBytes += entry.cbData;
                counters.busyBlocks++;
                unsigned power = 0;
                for (std::uint64_t size = entry.cbData; size > 1 && power < 31; size >>= 1) ++power;
                counters.bytesByPower[power] += entry.cbData;
                counters.blocksByPower[power]++;
            } else if (entry.wFlags & PROCESS_HEAP_REGION) {
                counters.committedBytes += entry.Region.dwCommittedSize;
            }
            if (++entries == 10000000) break;
        }
        const DWORD error = GetLastError();
        HeapUnlock(heaps[i]);
        counters.heaps++;
        if (error != ERROR_NO_MORE_ITEMS || entries == 10000000) counters.failedHeaps++;
    }
    stream << ",\"heapBusyBytes\":" << counters.busyBytes
           << ",\"heapBusyBlocks\":" << counters.busyBlocks
           << ",\"heapRegionCommittedBytes\":" << counters.committedBytes
           << ",\"heapsWalked\":" << counters.heaps
           << ",\"failedHeaps\":" << counters.failedHeaps
           << ",\"heapSizeBuckets\":[";
    bool first = true;
    for (unsigned i = 0; i < counters.bytesByPower.size(); ++i) {
        if (!counters.blocksByPower[i]) continue;
        if (!first) stream << ',';
        first = false;
        stream << "{\"power\":" << i << ",\"bytes\":" << counters.bytesByPower[i]
               << ",\"blocks\":" << counters.blocksByPower[i] << '}';
    }
    stream << ']';
#endif
    stream << '}';
}
} // namespace oneui::capi
