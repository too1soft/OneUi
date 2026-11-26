#include "internal/scroll_trace.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <windows.h>

int main() {
    using namespace oneui::internal;
    const auto path = std::filesystem::current_path() /
        ("scroll-trace-" + std::to_string(GetCurrentProcessId()) + ".csv");
    struct RemoveTrace {
        std::filesystem::path path;
        ~RemoveTrace() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{path};
    const auto utf8 = path.u8string();
    const std::string setting(utf8.begin(), utf8.end());
    if (_putenv_s("ONEUI_SCROLL_TRACE", setting.c_str()) != 0 || !scrollTraceEnabled()) return 1;

    constexpr int rowCount = 32;
    std::array<std::thread, 4> producers;
    for (std::size_t producer = 0; producer < producers.size(); ++producer) {
        producers[producer] = std::thread([producer, rowCount] {
            for (int row = 0; row < rowCount; ++row) {
                auto label = "temporary producer label " + std::to_string(producer);
                auto phase = "row " + std::to_string(row);
                writeScrollTrace({label.c_str(), phase.c_str()});
                label.assign(label.size(), 'x');
                phase.assign(phase.size(), 'x');
            }
        });
    }
    for (auto& producer : producers) producer.join();
    flushScrollTrace();
    // Finishing a checkpoint does not turn tracing off for a later operation.
    writeScrollTrace({"test", "after checkpoint"});
    flushScrollTrace();

    std::ifstream input(path);
    std::string line;
    if (!std::getline(input, line) || line.find("sequence,time_ms,thread,component,phase,") != 0) return 2;
    std::array<int, 4> next{};
    int sequence = 1;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::array<std::string, 5> prefix;
        for (auto& value : prefix) {
            if (!std::getline(fields, value, ',')) return 3;
        }
        if (prefix[0] != std::to_string(sequence++)) return 4;
        if (prefix[3] == "test") {
            return prefix[4] == "after checkpoint" &&
                next == std::array<int, 4>{rowCount, rowCount, rowCount, rowCount} &&
                !std::getline(input, line) ? 0 : 5;
        }
        bool matched = false;
        for (std::size_t i = 0; i < next.size(); ++i) {
            if (prefix[3] != "temporary producer label " + std::to_string(i)) continue;
            if (prefix[4] != "row " + std::to_string(next[i]++)) return 6;
            matched = true;
            break;
        }
        if (!matched) return 7;
    }
    return 8;
}
