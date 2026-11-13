#include "oneui/interaction_trace.h"
#include "internal/interaction_trace.h"
#ifdef _WIN32
#include <windows.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace oneui {
namespace {
struct Event {
    internal::InteractionSnapshot snapshot;
    std::uint64_t sequence = 0;
    long long qpc = 0;
    DWORD pid = 0, tid = 0;
    const char* phase = "unknown";
    const char* backend = "unknown";
    int success = -1;
    unsigned message = 0;
    double queueAge = -1;
    bool visible = false, foreground = false;
};

// Logging never opens/writes a file on the UI thread. Both pending memory and
// output are bounded; a final dropped count makes overflow/truncation explicit.
class TraceWriter {
public:
    TraceWriter() {
        const auto* path = std::getenv("ONEUI_INTERACTION_TRACE");
        if (!path || !*path) return;
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        frequency_ = frequency.QuadPart;
        const std::filesystem::path output(path);
        events_ = std::make_unique<Event[]>(capacity_);
        accepting_.store(true, std::memory_order_relaxed);
        worker_ = std::thread([this, output] { write(output); });
    }
    ~TraceWriter() { finish(); }
    void finish() {
        std::lock_guard<std::mutex> finishLock(finishMutex_);
        if (!worker_.joinable()) return;
        accepting_.store(false, std::memory_order_relaxed);
        { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; }
        ready_.notify_one();
        worker_.join();
    }
    bool enabled() const { return accepting_.load(std::memory_order_relaxed); }
    void push(Event event) {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        event.qpc = now.QuadPart;
        event.pid = GetCurrentProcessId();
        event.tid = GetCurrentThreadId();
        const auto hwnd = reinterpret_cast<HWND>(event.snapshot.window);
        event.visible = hwnd && IsWindowVisible(hwnd) && !IsIconic(hwnd);
        event.foreground = hwnd && GetForegroundWindow() == hwnd;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            if (size_ == capacity_ || accepted_ >= 250000) {
                ++dropped_;
                return;
            }
            event.sequence = ++accepted_;
            events_[(head_ + size_) % capacity_] = event;
            ++size_;
        }
        ready_.notify_one();
    }
private:
    void row(std::ofstream& out, const Event& e) {
        out << e.sequence << ',' << e.qpc << ',' << frequency_ << ',' << e.pid << ','
            << e.tid << ',' << e.snapshot.window << ',' << e.snapshot.operation << ','
            << e.snapshot.generation << ',' << e.phase << ',' << e.snapshot.paintSequence
            << ',' << e.snapshot.ready << ',' << e.visible << ',' << e.foreground << ','
            << e.backend << ',' << e.success << ',' << e.message << ',' << e.queueAge
            << ',' << dropped_.load(std::memory_order_relaxed) << '\n';
    }
    void write(const std::filesystem::path& output) {
        std::ofstream out(output, std::ios::out | std::ios::trunc);
        if (!out) return;
        out << "sequence,qpc_ticks,qpc_frequency,pid,tid,hwnd,operation_id,generation,phase,"
               "paint_sequence,ready,visible,foreground,backend,success,message,queue_age_ms,dropped\n";
        std::array<Event, 64> batch;
        for (;;) {
            std::size_t count = 0;
            bool finished = false;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] { return stopping_ || size_ > 0; });
                while (count < batch.size() && size_ > 0) {
                    batch[count++] = events_[head_];
                    head_ = (head_ + 1) % capacity_;
                    --size_;
                }
                finished = stopping_ && size_ == 0;
            }
            for (std::size_t i = 0; i < count; ++i) row(out, batch[i]);
            out.flush();
            if (finished) break;
        }
        Event end;
        end.phase = "trace_end";
        end.sequence = accepted_ + 1;
        LARGE_INTEGER now{}; QueryPerformanceCounter(&now); end.qpc = now.QuadPart;
        end.pid = GetCurrentProcessId(); end.tid = GetCurrentThreadId();
        row(out, end);
    }
    bool stopping_ = false;
    std::atomic<bool> accepting_{false};
    long long frequency_ = 0;
    std::mutex mutex_;
    std::mutex finishMutex_;
    std::condition_variable ready_;
    std::thread worker_;
    static constexpr std::size_t capacity_ = 4096;
    std::unique_ptr<Event[]> events_;
    std::size_t head_ = 0, size_ = 0;
    std::uint64_t accepted_ = 0;
    std::atomic<std::uint64_t> dropped_{0};
};
TraceWriter& writer() { static TraceWriter value; return value; }
struct WindowTrace {
    std::uint64_t lastInput = 0;
    internal::InteractionSnapshot committed;
};
struct ThreadTraceState {
    std::unordered_map<std::uintptr_t, WindowTrace> windows;
    std::array<std::pair<std::uint64_t, std::uintptr_t>, 4096> operations{};
    std::size_t operationSlot = 0;
    std::uint64_t activeOperation = 0;
};
ThreadTraceState& threadState() {
    // Inactive tracing reserves only one pointer per thread, not the token ring.
    thread_local std::unique_ptr<ThreadTraceState> state;
    if (!state) state = std::make_unique<ThreadTraceState>();
    return *state;
}
std::atomic<std::uint64_t> nextOperation{0};

std::uintptr_t findWindow(std::uint64_t operation) {
    if (!operation) return 0;
    for (const auto& entry : threadState().operations) if (entry.first == operation) return entry.second;
    return 0;
}
bool isInteractionMessage(unsigned message) {
    switch (message) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_MBUTTONDOWN: case WM_MBUTTONUP:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: return true;
    default: return false;
    }
}
}
void finishInteractionTrace() { if (writer().enabled()) writer().finish(); }
std::uint64_t currentInteractionOperation(void* nativeWindow) {
    if (!writer().enabled()) return 0;
    auto& state = threadState();
    if (!nativeWindow) return state.activeOperation;
    const auto found = state.windows.find(reinterpret_cast<std::uintptr_t>(nativeWindow));
    return found == state.windows.end() ? 0 : found->second.lastInput;
}
void markInteractionPhase(void* nativeWindow, std::uint64_t operation,
                          std::uint64_t generation, const char* phase) {
    if (!writer().enabled() || !operation || !phase) return;
    const auto window = nativeWindow ? reinterpret_cast<std::uintptr_t>(nativeWindow) : findWindow(operation);
    if (!window || findWindow(operation) != window || !IsWindow(reinterpret_cast<HWND>(window))) return;
    const char* stablePhase = nullptr;
    for (const char* candidate : {"action_start", "route_requested", "route_commit", "route_ready", "action_end", "measurement_end"})
        if (std::strcmp(candidate, phase) == 0) { stablePhase = candidate; break; }
    if (!stablePhase) return;
    auto& state = threadState().windows[window];
    internal::InteractionSnapshot snapshot{window, operation, generation, 0, false};
    if (std::strcmp(stablePhase, "route_commit") == 0) {
        snapshot.paintSequence = state.committed.paintSequence;
        state.committed = snapshot;
    } else if (std::strcmp(stablePhase, "route_ready") == 0) {
        if (state.committed.operation != operation || state.committed.generation != generation) return;
        state.committed.ready = true;
        snapshot.ready = true;
    }
    Event event; event.snapshot = snapshot; event.phase = stablePhase;
    writer().push(event);
}
namespace internal {
InteractionInputScope::InteractionInputScope(std::uintptr_t window, unsigned message, unsigned messageTime) {
    if (!writer().enabled()) return;
    auto& state = threadState();
    previous_ = state.activeOperation; active_ = true;
    // Each dispatch owns its scope, including paint/posted work reached by a
    // nested loop. Only a real input dispatch may expose a current operation.
    state.activeOperation = 0;
    if (!window || !isInteractionMessage(message)) return;
    state.activeOperation = ++nextOperation;
    state.operations[state.operationSlot++ % state.operations.size()] = {state.activeOperation, window};
    state.windows[window].lastInput = state.activeOperation;
    Event event; event.snapshot.window = window; event.snapshot.operation = state.activeOperation;
    event.phase = "input_received"; event.message = message;
    // MSG.time and GetTickCount share the wrapping 32-bit system clock.
    event.queueAge = static_cast<DWORD>(GetTickCount() - messageTime);
    writer().push(event);
}
InteractionInputScope::~InteractionInputScope() { if (active_) threadState().activeOperation = previous_; }
InteractionSnapshot beginInteractionPaint(std::uintptr_t window) {
    if (!writer().enabled()) return {};
    auto& state = threadState().windows[window].committed;
    state.window = window;
    ++state.paintSequence;
    traceInteractionPaint(state, "paint_start");
    return state;
}
void traceInteractionPaint(const InteractionSnapshot& snapshot, const char* phase, const char* backend, int success) {
    if (!snapshot.window || !writer().enabled()) return;
    Event event; event.snapshot = snapshot; event.phase = phase; event.backend = backend; event.success = success;
    writer().push(event);
}
void forgetInteractionWindow(std::uintptr_t window) {
    if (!writer().enabled()) return;
    auto& state = threadState();
    state.windows.erase(window);
    for (auto& entry : state.operations) if (entry.second == window) entry = {};
}
} // namespace internal
} // namespace oneui
#else
namespace oneui {
std::uint64_t currentInteractionOperation(void*) { return 0; }
void markInteractionPhase(void*, std::uint64_t, std::uint64_t, const char*) {}
void finishInteractionTrace() {}
namespace internal {
InteractionInputScope::InteractionInputScope(std::uintptr_t, unsigned, unsigned) {}
InteractionInputScope::~InteractionInputScope() = default;
InteractionSnapshot beginInteractionPaint(std::uintptr_t) { return {}; }
void traceInteractionPaint(const InteractionSnapshot&, const char*, const char*, int) {}
void forgetInteractionWindow(std::uintptr_t) {}
}
}
#endif
