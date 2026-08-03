#include "internal/scroll_trace.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <locale>
#include <mutex>
#include <string>
#include <thread>

namespace oneui::internal {
namespace {

constexpr std::uint64_t kFlushRecordInterval = 8;
constexpr double kFlushTimeIntervalMs = 100.0;

class ScrollTraceWriter final {
public:
    ScrollTraceWriter()
        : origin_(std::chrono::steady_clock::now()) {
        const char* path = std::getenv("ONEUI_SCROLL_TRACE");
        if (!path || path[0] == '\0') {
            return;
        }

        stream_.rdbuf()->pubsetbuf(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        stream_.open(std::filesystem::u8path(path), std::ios::out | std::ios::trunc);
        if (!stream_.is_open()) {
            return;
        }

        stream_.imbue(std::locale::classic());
        stream_ << "sequence,time_ms,thread,component,phase,object,input_delta,offset,target,velocity,interval_ms,max_offset,duration_ms,detail_a,detail_b\n";
        stream_.flush();
        enabled_ = true;
    }

    ~ScrollTraceWriter() {
        if (stream_.is_open()) {
            stream_.flush();
        }
    }

    bool enabled() const {
        return enabled_;
    }

    double nowMs() const {
        const auto elapsed = std::chrono::steady_clock::now() - origin_;
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    void write(const ScrollTraceEvent& event) {
        if (!enabled_) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const double timestampMs = nowMs();
        const std::uint64_t threadId = static_cast<std::uint64_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()));
        stream_ << sequence_++ << ','
                << std::fixed << std::setprecision(3) << timestampMs << ','
                << threadId << ','
                << event.component << ','
                << event.phase << ','
                << static_cast<std::uint64_t>(event.object) << ','
                << event.inputDelta << ','
                << event.offset << ','
                << event.target << ','
                << event.velocity << ','
                << event.intervalMs << ','
                << event.maxOffset << ','
                << event.durationMs << ','
                << event.detailA << ','
                << event.detailB << '\n';

        ++recordsSinceFlush_;
        if (recordsSinceFlush_ >= kFlushRecordInterval
            || timestampMs - lastFlushMs_ >= kFlushTimeIntervalMs) {
            stream_.flush();
            recordsSinceFlush_ = 0;
            lastFlushMs_ = timestampMs;
        }
    }

private:
    std::chrono::steady_clock::time_point origin_;
    std::array<char, 64 * 1024> buffer_{};
    std::ofstream stream_;
    std::mutex mutex_;
    std::uint64_t sequence_ = 1;
    std::uint64_t recordsSinceFlush_ = 0;
    double lastFlushMs_ = 0.0;
    bool enabled_ = false;
};

ScrollTraceWriter& writer() {
    static ScrollTraceWriter instance;
    return instance;
}

} // namespace

bool scrollTraceEnabled() {
    return writer().enabled();
}

double scrollTraceNowMs() {
    return writer().nowMs();
}

void writeScrollTrace(const ScrollTraceEvent& event) {
    writer().write(event);
}

} // namespace oneui::internal
