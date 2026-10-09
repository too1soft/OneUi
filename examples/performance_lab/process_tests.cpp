#include "internal/unicode.h"
#include "lab_process.hpp"
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#endif

namespace {
using namespace std::chrono_literals;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string hex(const std::wstring &value) {
    const char *digits = "0123456789abcdef";
    std::string result;
    for (unsigned char ch : oneui::unicode::toUtf8(value)) {
        result += digits[ch >> 4];
        result += digits[ch & 15];
    }
    return result;
}
// Bound every test subprocess, including failures before a report is written.
struct Deadline {
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    std::thread thread;
    explicit Deadline(std::chrono::milliseconds delay)
        : thread([this, delay] {
              std::unique_lock<std::mutex> lock(mutex);
              if (!cv.wait_for(lock, delay, [this] { return done; }))
                  cancelled = true;
          }) {}
    ~Deadline() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            done = true;
        }
        cv.notify_one();
        thread.join();
    }
};
std::wstring pathArgument(const std::filesystem::path &value) {
#ifdef _WIN32
    return value.wstring();
#else
    return oneui::unicode::fromUtf8(value.u8string());
#endif
}
void processTests(const std::filesystem::path &output) {
    const std::vector<std::wstring> args{L"--process-fixture",      L"",
                                         L"中文路径 / with spaces", L"quoted \"value\"",
                                         L"C:\\path with space\\",  L"$() ` ; &"};
    Deadline deadline(5s);
    auto log = output / "arguments.log";
    auto result = lab::runLabProcess(args, log, deadline.cancelled);
    require(result.started && !result.cancelled && result.exitCode == 7,
            "Child exit code was not preserved");
    std::ifstream file(log);
    for (const auto &argument : args) {
        std::string line;
        require(bool(std::getline(file, line)) && line == hex(argument),
                "Direct launch changed a Unicode, empty, quoted or shell-like argument");
    }
    Deadline cancel(200ms);
    auto start = std::chrono::steady_clock::now();
    result = lab::runLabProcess({L"--process-fixture", L"--wait"}, output / "cancel.log",
                                cancel.cancelled);
    require(result.started && result.cancelled && result.exitCode != 0,
            "Cancelled child was not terminated and reaped");
    require(std::chrono::steady_clock::now() - start < 2s, "Cancellation stalled the UI thread");
    std::atomic<bool> alreadyCancelled{true};
    result = lab::runLabProcess({}, output / "not-started.log", alreadyCancelled);
    require(!result.started && result.cancelled, "Cancelled job still started a child");
    result = lab::runLabProcess({}, output / "missing.log", deadline.cancelled,
                                output / "missing-executable");
    require(!result.started && !result.error.empty(), "Spawn failure was reported as success");
    std::cout << "Unicode/quoting, child exit, cancellation/reaping and spawn failure passed.\n";
}
void benchmarkTests(const std::filesystem::path &demo, const std::filesystem::path &output) {
    for (const auto &mode : {L"cpu", L"gpu"}) {
        auto directory = output / (mode == std::wstring(L"cpu") ? "cpu" : "gpu");
        std::filesystem::create_directories(directory);
        auto report = directory / "renderer-performance.txt";
        std::filesystem::remove(report);
        Deadline deadline(30s);
        auto result = lab::runLabProcess(
            {L"--renderer", mode, L"--renderer-benchmark", L"chart", L"--load", L"medium",
             L"--particle-mode", L"combined", L"--width", L"1320", L"--height", L"900",
             L"--sample-seconds", L"1", L"--output", pathArgument(directory)},
            directory / "runtime.log", deadline.cancelled, demo);
        require(result.started && !result.cancelled && result.exitCode == 0 &&
                    std::filesystem::exists(report),
                "Independent renderer benchmark failed");
        std::ifstream file(report);
        std::string contents((std::istreambuf_iterator<char>(file)), {});
        require(contents.find("frame_callback_hz=") != std::string::npos &&
                    contents.find("physical_width=") != std::string::npos,
                "Benchmark lacks timing/geometry evidence");
    }
    std::cout << "Actual CPU/GPU benchmark children completed; reports record actual backend.\n";
}
int run(const std::vector<std::wstring> &arguments) {
    if (arguments.size() > 1 && arguments[1] == L"--process-fixture") {
        for (std::size_t i = 1; i < arguments.size(); ++i)
            std::cout << hex(arguments[i]) << '\n';
        std::cout.flush();
        if (arguments.size() > 2 && arguments[2] == L"--wait")
            std::this_thread::sleep_for(5s);
        return 7;
    }
    require(arguments.size() >= 2, "Usage: process-tests OUTPUT [DEMO_EXECUTABLE]");
    auto output = std::filesystem::u8path(oneui::unicode::toUtf8(arguments[1]));
    processTests(output);
    if (arguments.size() == 3)
        benchmarkTests(std::filesystem::u8path(oneui::unicode::toUtf8(arguments[2])), output);
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::vector<std::wstring> arguments;
#ifdef _WIN32
        auto wide = CommandLineToArgvW(GetCommandLineW(), &argc);
        require(wide != nullptr, "Command line unavailable");
        arguments.assign(wide, wide + argc);
        LocalFree(wide);
#else
        for (int i = 0; i < argc; ++i)
            arguments.push_back(oneui::unicode::fromUtf8(argv[i]));
#endif
        return run(arguments);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
