#include "oneui/frame_paint_trace.h"
#include <atomic>

namespace oneui {
namespace {
thread_local FramePaintTrace* current = nullptr;
std::atomic<std::uint64_t> nextSource{0};
}

void FramePaintTrace::note(std::uint64_t source, std::uint64_t frame) {
    if (!source || !frame) return;
    for (std::size_t index = 0; index < size; ++index) {
        if (stamps[index].source == source) {
            stamps[index].frame = frame;
            return;
        }
    }
    if (size == stamps.size()) { overflow = true; return; }
    stamps[size++] = {source, frame};
}

FramePaintScope::FramePaintScope(FramePaintTrace& trace) : previous_(current) { current = &trace; }
FramePaintScope::~FramePaintScope() { current = previous_; }
std::uint64_t nextFramePaintSource() { return ++nextSource; }
void noteFramePaint(std::uint64_t source, std::uint64_t frame) {
    if (current) current->note(source, frame);
}

} // namespace oneui
