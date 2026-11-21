#pragma once

#include "internal/scroll_trace.h"

namespace oneui::win32 {

// Captures work the message pump may run before delivering the next input.
// Enabled only with ONEUI_SCROLL_TRACE; all timestamps share its clock.
class DispatchTraceScope {
public:
    explicit DispatchTraceScope(const char* phase, std::uintptr_t window = 0, unsigned int message = 0)
        : phase_(phase), window_(window), message_(message),
          started_(internal::scrollTraceEnabled() ? internal::scrollTraceNowMs() : -1.0) {}

    ~DispatchTraceScope() {
        if (started_ < 0.0) return;
        const double elapsed = internal::scrollTraceNowMs() - started_;
        if (elapsed < 1.0) return;
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "win32", phase_, window_, 0, 0, 0, 0, 0, 0, elapsed,
            static_cast<double>(message_), static_cast<double>(dispatched_)});
    }

    DispatchTraceScope(const DispatchTraceScope&) = delete;
    DispatchTraceScope& operator=(const DispatchTraceScope&) = delete;

    void setDispatched(unsigned int count) { dispatched_ = count; }

private:
    const char* phase_;
    std::uintptr_t window_;
    unsigned int message_;
    double started_;
    unsigned int dispatched_ = 0;
};

} // namespace oneui::win32
