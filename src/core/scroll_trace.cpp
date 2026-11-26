#include "internal/scroll_trace.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <locale>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <atomic>
#include <condition_variable>
#include <deque>
#include <windows.h>
#endif

namespace oneui::internal {
namespace {

const auto& traceOrigin() {
    static const auto origin = std::chrono::steady_clock::now();
    return origin;
}

std::uint64_t currentTraceThread() {
    return static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

class TraceFile final {
public:
    bool open(const std::filesystem::path& path) {
        stream_.rdbuf()->pubsetbuf(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        stream_.open(path, std::ios::out | std::ios::trunc);
        if (!stream_) return false;
        stream_.imbue(std::locale::classic());
        stream_ << "sequence,time_ms,thread,component,phase,object,input_delta,offset,target,velocity,interval_ms,max_offset,duration_ms,detail_a,detail_b\n";
        stream_.flush();
        return bool(stream_);
    }

    void append(std::uint64_t sequence, double timestamp, std::uint64_t thread,
                const ScrollTraceEvent& event, const char* component, const char* phase) {
        stream_ << sequence << ','
                << std::fixed << std::setprecision(3) << timestamp << ','
                << thread << ',' << component << ',' << phase << ','
                << static_cast<std::uint64_t>(event.object) << ','
                << event.inputDelta << ',' << event.offset << ',' << event.target << ','
                << event.velocity << ',' << event.intervalMs << ',' << event.maxOffset << ','
                << event.durationMs << ',' << event.detailA << ',' << event.detailB << '\n';
        if (++unflushed_ >= rowsPerFlush_ || timestamp - lastFlushMs_ >= flushIntervalMs_) flush();
    }

    void flush() {
        if (!unflushed_) return;
        stream_.flush();
        unflushed_ = 0;
        lastFlushMs_ = scrollTraceNowMs();
    }

private:
    static constexpr std::size_t rowsPerFlush_ = 8;
    static constexpr double flushIntervalMs_ = 100.0;
    std::array<char, 64 * 1024> buffer_{};
    std::ofstream stream_;
    std::size_t unflushed_ = 0;
    double lastFlushMs_ = 0.0;
};

#ifdef _WIN32
struct TraceRow {
    // Some callers supply temporary labels. Copy them before returning to the caller.
    std::string component;
    std::string phase;
    ScrollTraceEvent event;
    std::uint64_t sequence = 0;
    double timestamp = 0;
    std::uint64_t thread = 0;
};

class ScrollTraceWriter final : public std::enable_shared_from_this<ScrollTraceWriter> {
public:
    explicit ScrollTraceWriter(std::filesystem::path path) : path_(std::move(path)) {}

    bool enabled() const { return accepting_.load(std::memory_order_relaxed); }

    void write(const ScrollTraceEvent& event) {
        if (!enabled()) return;
        TraceRow row{event.component ? event.component : "", event.phase ? event.phase : "", event};
        {
            std::lock_guard lock(mutex_);
            if (!enabled()) return;
            if (rows_.size() >= maxPending_) {
                ++dropped_;
                return;
            }
            stamp(row);
            rows_.push_back(std::move(row));
            if (scheduled_) return;
            scheduled_ = true;
        }
        if (!schedule()) stopOnFailure();
    }

    void flush() {
        std::unique_lock lock(mutex_);
        idle_.wait_for(lock, std::chrono::milliseconds(250), [this] { return !scheduled_; });
    }

private:
    struct Work {
        std::shared_ptr<ScrollTraceWriter> writer;
        HMODULE module = nullptr;
    };

    static void CALLBACK run(PTP_CALLBACK_INSTANCE instance, void* context) noexcept {
        const std::unique_ptr<Work> work(static_cast<Work*>(context));
        FreeLibraryWhenCallbackReturns(instance, work->module);
        try {
            work->writer->drain();
        } catch (...) {
            work->writer->stopOnFailure();
        }
    }

    bool schedule() {
        auto work = std::make_unique<Work>();
        work->writer = shared_from_this();
        // Keep the DLL and writer alive through asynchronous callback return.
        // DLL teardown does not wait for an optional diagnostic writer.
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                reinterpret_cast<LPCWSTR>(&run), &work->module)) return false;
        auto* submitted = work.release();
        if (TrySubmitThreadpoolCallback(run, submitted, nullptr)) return true;
        FreeLibrary(submitted->module);
        delete submitted;
        return false;
    }

    void stamp(TraceRow& row) {
        row.sequence = nextSequence_++;
        row.timestamp = scrollTraceNowMs();
        row.thread = currentTraceThread();
    }

    void drain() {
        if (!opened_) {
            opened_ = file_.open(path_);
            if (!opened_) {
                stopOnFailure();
                return;
            }
        }
        std::array<TraceRow, 64> batch;
        for (;;) {
            std::size_t count = 0;
            {
                std::lock_guard lock(mutex_);
                while (count < batch.size() && !rows_.empty()) {
                    batch[count++] = std::move(rows_.front());
                    rows_.pop_front();
                }
                if (rows_.empty() && dropped_ && count < batch.size()) {
                    auto& notice = batch[count++];
                    notice = {"scroll_trace", "trace_dropped", {}};
                    notice.event.detailA = static_cast<double>(dropped_);
                    stamp(notice);
                    dropped_ = 0;
                }
            }
            for (std::size_t i = 0; i < count; ++i) {
                const auto& row = batch[i];
                file_.append(row.sequence, row.timestamp, row.thread, row.event,
                             row.component.c_str(), row.phase.c_str());
            }
            if (count != 0) continue;
            file_.flush();
            {
                std::lock_guard lock(mutex_);
                if (!rows_.empty() || dropped_) continue;
                scheduled_ = false;
                idle_.notify_all();
                return;
            }
        }
    }

    void stopOnFailure() {
        std::lock_guard lock(mutex_);
        accepting_.store(false, std::memory_order_relaxed);
        rows_.clear();
        scheduled_ = false;
        idle_.notify_all();
    }

    static constexpr std::size_t maxPending_ = 4096;
    const std::filesystem::path path_;
    TraceFile file_;
    std::mutex mutex_;
    std::condition_variable idle_;
    std::deque<TraceRow> rows_;
    std::uint64_t nextSequence_ = 1;
    std::uint64_t dropped_ = 0;
    bool scheduled_ = false;
    bool opened_ = false;
    std::atomic<bool> accepting_{true};
};
#else
class ScrollTraceWriter final {
public:
    explicit ScrollTraceWriter(const std::filesystem::path& path) : enabled_(file_.open(path)) {}
    bool enabled() const { return enabled_; }
    void write(const ScrollTraceEvent& event) {
        if (!enabled_) return;
        std::lock_guard lock(mutex_);
        file_.append(nextSequence_++, scrollTraceNowMs(), currentTraceThread(),
                     event, event.component, event.phase);
    }
    void flush() {
        std::lock_guard lock(mutex_);
        file_.flush();
    }
private:
    std::mutex mutex_;
    TraceFile file_;
    std::uint64_t nextSequence_ = 1;
    const bool enabled_;
};
#endif

const std::shared_ptr<ScrollTraceWriter>& writer() {
    static const auto instance = []() -> std::shared_ptr<ScrollTraceWriter> {
        traceOrigin();
        const char* path = std::getenv("ONEUI_SCROLL_TRACE");
        if (!path || !*path) return {};
        return std::make_shared<ScrollTraceWriter>(std::filesystem::u8path(path));
    }();
    return instance;
}

} // namespace

bool scrollTraceEnabled() {
    const auto& trace = writer();
    return trace && trace->enabled();
}

double scrollTraceNowMs() {
    const auto elapsed = std::chrono::steady_clock::now() - traceOrigin();
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

void writeScrollTrace(const ScrollTraceEvent& event) {
    if (const auto& trace = writer()) trace->write(event);
}

void flushScrollTrace() {
    if (const auto& trace = writer()) trace->flush();
}

} // namespace oneui::internal
