#pragma once
#include <cstdint>

namespace oneui::internal {
struct InteractionSnapshot {
    std::uintptr_t window = 0;
    std::uint64_t operation = 0, generation = 0, paintSequence = 0;
    bool ready = false;
};
class InteractionInputScope {
public:
    InteractionInputScope(std::uintptr_t window, unsigned message, unsigned messageTime);
    ~InteractionInputScope();
private:
    std::uint64_t previous_ = 0;
    bool active_ = false;
};
InteractionSnapshot beginInteractionPaint(std::uintptr_t window);
void traceInteractionPaint(const InteractionSnapshot&, const char* phase,
                           const char* backend = "unknown", int success = -1);
void forgetInteractionWindow(std::uintptr_t window);
} // namespace oneui::internal
