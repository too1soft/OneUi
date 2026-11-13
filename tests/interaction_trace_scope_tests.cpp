#include "oneui/interaction_trace.h"
#include "internal/interaction_trace.h"
#include <windows.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void scopes(std::uintptr_t window) {
    using oneui::currentInteractionOperation;
    using oneui::internal::InteractionInputScope;
    check(currentInteractionOperation() == 0, "initial dispatch has no input");
    {
        InteractionInputScope outer(window, WM_LBUTTONUP, GetTickCount());
        const auto first = currentInteractionOperation();
        check(first != 0, "outer input exposes an operation");
        {
            InteractionInputScope posted(window, WM_APP + 1, GetTickCount());
            check(currentInteractionOperation() == 0, "nested posted work clears outer input");
            {
                InteractionInputScope input(window, WM_KEYDOWN, GetTickCount());
                check(currentInteractionOperation() != 0 && currentInteractionOperation() != first,
                      "nested input exposes its own operation");
            }
            check(currentInteractionOperation() == 0, "nested input restores posted scope");
        }
        check(currentInteractionOperation() == first, "posted scope restores outer input");
        {
            InteractionInputScope paint(window, WM_PAINT, GetTickCount());
            check(currentInteractionOperation() == 0, "nested paint clears outer input");
        }
        check(currentInteractionOperation() == first, "paint scope restores outer input");
        {
            InteractionInputScope threadMessage(0, WM_NULL, GetTickCount());
            check(currentInteractionOperation() == 0, "windowless dispatch clears outer input");
        }
        check(currentInteractionOperation() == first, "windowless scope restores outer input");
    }
    check(currentInteractionOperation() == 0, "outer input restores idle scope");
}
}

int main() {
    const auto output = std::filesystem::temp_directory_path() /
        ("oneui-interaction-scope-" + std::to_string(GetCurrentProcessId()) + ".csv");
    _putenv_s("ONEUI_INTERACTION_TRACE", output.string().c_str());
    HWND window = CreateWindowExW(0, L"STATIC", L"trace scope", WS_OVERLAPPED,
                                 0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    int result = 0;
    try {
        check(window != nullptr, "create diagnostic window");
        scopes(reinterpret_cast<std::uintptr_t>(window));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result = 1;
    }
    oneui::finishInteractionTrace();
    if (window) DestroyWindow(window);
    std::ifstream trace(output);
    std::string line; unsigned inputs = 0, endings = 0;
    while (std::getline(trace, line)) {
        if (line.find(",input_received,") != std::string::npos) ++inputs;
        if (line.find(",trace_end,") != std::string::npos) ++endings;
    }
    if (inputs != 2 || endings != 1) {
        std::cerr << "trace must contain two inputs and one complete ending\n"; result = 1;
    }
    trace.close();
    std::error_code ignored; std::filesystem::remove(output, ignored);
    return result;
}
