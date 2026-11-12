#pragma once

#include "oneui/export.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace oneui {

struct FramePaintStamp {
    std::uint64_t source = 0;
    std::uint64_t frame = 0;
};

// Drawn source identities, not producer/consumer counters or proof of display.
// Frame ID zero opts out (e.g. static images). At most 16 sources are tracked
// in one picture; overflow is explicit.
struct ONEUI_API FramePaintTrace {
    std::array<FramePaintStamp, 16> stamps{};
    std::size_t size = 0;
    bool overflow = false;
    void note(std::uint64_t source, std::uint64_t frame);
};

class ONEUI_API FramePaintScope final {
public:
    explicit FramePaintScope(FramePaintTrace& trace);
    ~FramePaintScope();
    FramePaintScope(const FramePaintScope&) = delete;
    FramePaintScope& operator=(const FramePaintScope&) = delete;
private:
    FramePaintTrace* previous_;
};

ONEUI_API std::uint64_t nextFramePaintSource();
ONEUI_API void noteFramePaint(std::uint64_t source, std::uint64_t frame);

} // namespace oneui
