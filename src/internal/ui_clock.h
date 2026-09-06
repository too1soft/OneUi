#pragma once
#include <chrono>
#include <functional>
#include <utility>

namespace oneui::internal {
// UI-thread-local injection for deterministic tests; production uses monotonic time.
inline thread_local std::function<double()> uiClock;
inline double uiTimeMs() {
    if (uiClock) return uiClock();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
class ScopedUiClock {
public:
    explicit ScopedUiClock(std::function<double()> clock) : previous_(std::move(uiClock)) { uiClock = std::move(clock); }
    ~ScopedUiClock() { uiClock = std::move(previous_); }
    ScopedUiClock(const ScopedUiClock&) = delete;
    ScopedUiClock& operator=(const ScopedUiClock&) = delete;
private:
    std::function<double()> previous_;
};
}
