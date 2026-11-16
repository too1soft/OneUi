// Opt-in, foreground-only fixture. It injects F24 only into its own window.
// Compile separately against the selected production DLL; never run in CTest.
#include "oneui/platform/window.h"
#include "oneui/controls/label.h"
#include "oneui/view.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
constexpr unsigned taskCount = 6000;
constexpr double budgetMs = 50.0;

long long qpc() {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

struct Observation {
    long long before = 0, after = 0, received = 0;
    unsigned completedAtReceipt = 0;
    bool foregroundAtReceipt = false;
};

struct State {
    HWND window = nullptr;
    DWORD pid = GetCurrentProcessId();
    long long frequency = 0;
    std::mutex mutex;
    std::array<Observation, 2> hardware;
    Observation posted;
    std::atomic<int> stage{0}, exitCode{2};
    std::atomic<unsigned> completed{0}, received{0}, painted{0};
    std::atomic<bool> aborted{false}, started{false}, ready{false};
    std::string error;

    double milliseconds(long long begin, long long end) const {
        return (end - begin) * 1000.0 / static_cast<double>(frequency);
    }
    bool foreground() {
        DWORD actual = 0;
        GetWindowThreadProcessId(window, &actual);
        if (IsWindow(window) && actual == pid && IsWindowVisible(window) &&
            !IsIconic(window) && GetForegroundWindow() == window) return true;
        aborted.store(true);
        return false;
    }
};

class CounterView final : public oneui::View {
public:
    CounterView(State& state, oneui::Window& window) : state_(state), window_(window) {
        label_ = std::make_shared<oneui::Label>(L"Waiting for frame warmup");
        label_->setFontSize(18);
        add(label_);
    }
    void text(std::wstring value) { label_->setText(std::move(value)); }
    void paint(oneui::Canvas& canvas) override {
        View::paint(canvas);
        state_.painted.store(state_.received.load());
    }
    bool onKeyDown(const oneui::KeyEvent& event) override {
        if (event.virtualKey == VK_ESCAPE) {
            state_.aborted.store(true);
            window_.close();
            return true;
        }
        return View::onKeyDown(event);
    }
protected:
    void layoutChildren() override {
        const auto bounds = frame();
        label_->setFrame({24, 32, bounds.width - 48, bounds.height - 64});
    }
private:
    State& state_;
    oneui::Window& window_;
    std::shared_ptr<oneui::Label> label_;
};

bool waitFor(State& state, const std::function<bool()>& condition, double seconds) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(static_cast<int>(seconds * 1000));
    while (!condition()) {
        if (state.aborted.load() || !state.foreground() || Clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return state.foreground();
}

bool injectF24(State& state, int stage) {
    if (!state.foreground()) return false;
    for (int key : {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN})
        if (GetAsyncKeyState(key) & 0x8000) return false;
    state.stage.store(stage);
    INPUT inputs[2]{};
    for (auto& input : inputs) {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_F24;
        input.ki.dwExtraInfo = 0x4f4e4555;
    }
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.hardware[stage].before = qpc();
    }
    // Only this atomic two-event call injects hardware input. The foreground
    // check is immediately adjacent; later observations also reject focus loss.
    if (!state.foreground()) return false;
    const auto sent = SendInput(2, inputs, sizeof(INPUT));
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.hardware[stage].after = qpc();
    }
    return sent == 2 && state.foreground();
}

void recordObservation(std::ostream& out, const Observation& value, const State& state) {
    out << "{\"injectionBeginQpc\":" << value.before
        << ",\"injectionReturnQpc\":" << value.after
        << ",\"receivedQpc\":" << value.received
        << ",\"receiptLatencyMs\":" << state.milliseconds(value.before, value.received)
        << ",\"completedAtReceipt\":" << value.completedAtReceipt
        << ",\"foregroundAtReceipt\":" << (value.foregroundAtReceipt ? "true" : "false") << '}';
}

void writeResult(State& state, const std::filesystem::path& output, bool complete) {
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto baseline = state.milliseconds(state.hardware[0].before, state.hardware[0].received);
    const auto flood = state.milliseconds(state.hardware[1].before, state.hardware[1].received);
    const bool observed = complete && !state.aborted.load() && baseline <= budgetMs;
    const bool fair = observed && flood <= budgetMs;
    state.exitCode.store(observed ? (fair ? 0 : 1) : 2);
    std::ofstream out(output / "result.json");
    out << "{\"status\":\"" << (observed ? "OBSERVED" : "INCOMPLETE")
        << "\",\"hardwareInputFair\":" << (fair ? "true" : "false")
        << ",\"budgetMs\":" << budgetMs << ",\"pid\":" << state.pid
        << ",\"hwnd\":" << reinterpret_cast<std::uintptr_t>(state.window)
        << ",\"qpcFrequency\":" << state.frequency
        << ",\"taskCount\":" << taskCount << ",\"callbackBusyUs\":250"
        << ",\"completed\":" << state.completed.load()
        << ",\"feedbackPaintedCounter\":" << state.painted.load()
        << ",\"baseline\":";
    recordObservation(out, state.hardware[0], state);
    out << ",\"flood\":";
    recordObservation(out, state.hardware[1], state);
    out << ",\"postedControl\":";
    recordObservation(out, state.posted, state);
    out << ",\"limitations\":[\"F24 receipt and a widget paint callback are measured, not monitor presentation.\","
           "\"Foreground checks do not lock the desktop against concurrent external changes.\","
           "\"A single controlled callback-flood sample is supplementary to daily-use navigation.\"]}\n";
}

void coordinate(State& state, oneui::Window& window, const std::filesystem::path& output,
                const std::filesystem::path& arm, const std::filesystem::path& finish) {
    while (!state.aborted.load() && !std::filesystem::exists(arm))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    bool complete = !state.aborted.load() && state.ready.load() && injectF24(state, 0);
    complete = complete && waitFor(state, [&] { return state.received.load() >= 1; }, 2);
    if (complete) {
        for (unsigned index = 0; index < taskCount; ++index) {
            if (!window.post([&] {
                    state.started.store(true);
                    const auto until = qpc() + state.frequency / 4000; // 250 us CPU work; no OS timer rounding.
                    while (qpc() < until) {}
                    state.completed.fetch_add(1);
                })) { complete = false; break; }
        }
        complete = complete && waitFor(state, [&] { return state.started.load(); }, 2);
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        complete = complete && state.completed.load() < taskCount && injectF24(state, 1);
        if (complete && state.foreground()) {
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                state.posted.before = qpc();
            }
            complete = PostMessageW(state.window, WM_KEYDOWN, VK_F23, 0) &&
                PostMessageW(state.window, WM_KEYUP, VK_F23, 0);
            std::lock_guard<std::mutex> lock(state.mutex);
            state.posted.after = qpc();
        }
        complete = complete && waitFor(state, [&] {
            std::lock_guard<std::mutex> lock(state.mutex);
            return state.received.load() >= 2 && state.posted.received && state.completed.load() == taskCount;
        }, 10);
        complete = complete && waitFor(state, [&] { return state.painted.load() >= 2; }, 2);
    }
    writeResult(state, output, complete);
    // Leave feedback visible until the root explicitly finishes this fixture.
    while (!state.aborted.load() && !std::filesystem::exists(finish))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    window.post([&window] { window.close(); });
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 4) { std::cerr << "Usage: fixture OUTPUT ARM_FILE FINISH_FILE\n"; return 2; }
    const std::filesystem::path output(argv[1]), arm(argv[2]), finish(argv[3]);
    if ((std::filesystem::exists(output) && !std::filesystem::is_empty(output)) ||
        std::filesystem::exists(arm) || std::filesystem::exists(finish)) return 2;
    std::filesystem::create_directories(output);
    State state;
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    state.frequency = frequency.QuadPart;
    oneui::WindowOptions options;
    options.title = L"OneUI hardware input fairness fixture";
    options.width = 760; options.height = 360;
    auto window = oneui::Window::create(options);
    window->initialize();
    state.window = static_cast<HWND>(window->nativeHandle());
    auto root = std::make_shared<CounterView>(state, *window);
    window->setContent(root);
    const auto closed = window->observeClosed([&] { state.aborted.store(true); });
    window->setRawKeyHandler([&](const oneui::KeyEvent& event) {
        if (!event.pressed || (event.virtualKey != VK_F24 && event.virtualKey != VK_F23)) return false;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            auto& value = event.virtualKey == VK_F23 ? state.posted : state.hardware[state.stage.load()];
            if (!value.received) {
                value.received = qpc(); value.completedAtReceipt = state.completed.load();
                value.foregroundAtReceipt = GetForegroundWindow() == state.window;
                if (!value.foregroundAtReceipt) state.aborted.store(true);
            }
        }
        if (event.virtualKey == VK_F24) {
            const auto count = state.received.fetch_add(1) + 1;
            root->text(L"Hardware F24 received: " + std::to_wstring(count));
        }
        return true;
    });
    unsigned warmFrames = 0;
    std::function<void(double)> warm;
    warm = [&](double) {
        root->text(L"Frame warmup " + std::to_wstring(++warmFrames) + L" / 20");
        if (warmFrames < 20) window->requestAnimationFrame(warm);
        else {
            state.ready.store(true);
            std::ofstream out(output / "ready.json");
            out << "{\"pid\":" << state.pid << ",\"hwnd\":"
                << reinterpret_cast<std::uintptr_t>(state.window) << ",\"warmFrames\":20}\n";
            root->text(L"Ready. Foreground this fixture and create the arm marker.");
        }
    };
    window->show();
    window->requestAnimationFrame(warm);
    std::thread worker(coordinate, std::ref(state), std::ref(*window), output, arm, finish);
    window->run();
    state.aborted.store(true);
    worker.join();
    return state.exitCode.load();
}
