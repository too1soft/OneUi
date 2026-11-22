// Opt-in probe. It sends hardware events only to its own foreground window.
#include "oneui/platform/window.h"
#include "oneui/view.h"

#include <windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
constexpr UINT workMessage = WM_APP + 12;
constexpr int workCount = 5;
constexpr auto property = L"OneUI.ClickTurnFixture";

struct Receipt {
    UINT message;
    int completed;
    Clock::time_point time;
};
struct State {
    WNDPROC previous{};
    std::atomic_bool started{false};
    std::atomic_bool armed{false};
    std::atomic_int completed{0};
    bool ordered = true;
    bool inputSent = false;
    Clock::time_point inputStart{}, inputEnd{};
    std::vector<Receipt> receipts;
};

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    auto& state = *static_cast<State*>(GetPropW(hwnd, property));
    if (state.armed && (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK ||
                        message == WM_LBUTTONUP ||
                        ((message == WM_KEYDOWN || message == WM_KEYUP) && wp == VK_F24))) {
        state.receipts.push_back({message, state.completed.load(), Clock::now()});
    }
    return CallWindowProcW(state.previous, hwnd, message, wp, lp);
}

LRESULT CALLBACK workProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    if (message == workMessage) {
        auto& state = *reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        state.ordered &= static_cast<int>(wp) == state.completed.load();
        state.started = true;
        const auto deadline = Clock::now() + 30ms;
        while (Clock::now() < deadline) {}
        ++state.completed;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

bool ownsPoint(HWND hwnd, POINT point) {
    DWORD owner = 0;
    GetWindowThreadProcessId(hwnd, &owner);
    POINT cursor{};
    return owner == GetCurrentProcessId() && GetForegroundWindow() == hwnd &&
           GetAncestor(WindowFromPoint(point), GA_ROOT) == hwnd &&
           GetCursorPos(&cursor) && cursor.x == point.x && cursor.y == point.y;
}

INPUT mouse(DWORD flags) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    return input;
}

INPUT key(DWORD flags) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_F24;
    input.ki.dwFlags = flags;
    return input;
}

bool sendProbeInput(HWND hwnd, POINT point, std::wstring_view mode, State& state) {
    if (!ownsPoint(hwnd, point)) return false;
    for (int modifier : {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN, VK_LBUTTON})
        if (GetAsyncKeyState(modifier) & 0x8000) return false;

    auto down = mouse(MOUSEEVENTF_LEFTDOWN), up = mouse(MOUSEEVENTF_LEFTUP);
    std::array<INPUT, 4> sequence{down, up, down, up};
    UINT count = 2;
    if (mode == L"interleaved") {
        sequence = {down, key(0), key(KEYEVENTF_KEYUP), up};
        count = 4;
    } else if (mode == L"twice") {
        count = 4;
    } else if (mode == L"held") {
        count = 1;
    }
    state.inputStart = Clock::now();
    state.armed = true;
    const auto sent = SendInput(count, sequence.data(), sizeof(INPUT));
    state.inputEnd = Clock::now();
    if (mode == L"held" && sent == 1) {
        std::this_thread::sleep_for(78ms);
        const bool ownedBeforeRelease = ownsPoint(hwnd, point);
        const auto released = SendInput(1, &up, sizeof(INPUT));
        return ownedBeforeRelease && released == 1;
    }
    if (sent != count && sent != 0) {
        if (mode == L"interleaved" && sent == 2) {
            auto keyUp = key(KEYEVENTF_KEYUP);
            SendInput(1, &keyUp, sizeof(INPUT));
        }
        if (mode == L"interleaved" || sent % 2 != 0)
            SendInput(1, &up, sizeof(INPUT));
    }
    return sent == count;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    const auto mode = std::wstring_view(argv[1]);
    if (mode != L"click" && mode != L"twice" && mode != L"held" && mode != L"interleaved") return 2;
    wchar_t library[MAX_PATH]{};
    GetModuleFileNameW(GetModuleHandleW(L"oneui.dll"), library, MAX_PATH);
    std::wcout << L"Loaded SDK: " << library << std::endl;

    State state;
    oneui::WindowOptions options;
    options.title = L"OneUI queued click fixture";
    options.width = 480;
    options.height = 360;
    auto window = oneui::Window::create(options);
    window->setContent(std::make_shared<oneui::View>());
    window->initialize();
    window->show();
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    if (!SetWindowPos(hwnd, HWND_TOPMOST, 80, 70, 480, 360, SWP_SHOWWINDOW)) return 2;
    SetForegroundWindow(hwnd);
    POINT point{200, 160};
    if (!ClientToScreen(hwnd, &point) || GetForegroundWindow() != hwnd ||
        GetAncestor(WindowFromPoint(point), GA_ROOT) != hwnd || !SetCursorPos(point.x, point.y)) return 2;

    if (!SetPropW(hwnd, property, &state)) return 3;
    state.previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(windowProc)));
    if (!state.previous) return 3;
    WNDCLASSW cls{};
    cls.lpfnWndProc = workProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"OneUI.ClickTurnFixture.Work";
    if (!RegisterClassW(&cls)) return 3;
    const HWND work = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 0, 0,
                                     HWND_MESSAGE, nullptr, cls.hInstance, &state);
    if (!work) return 3;

    std::thread driver([&] {
        std::this_thread::sleep_for(600ms);
        bool posted = true;
        for (int i = 0; i < workCount; ++i) posted &= PostMessageW(work, workMessage, i, 0) != FALSE;
        const auto deadline = Clock::now() + 450ms;
        while (!state.started && Clock::now() < deadline) SwitchToThread();
        if (posted && state.started) state.inputSent = sendProbeInput(hwnd, point, mode, state);
        std::this_thread::sleep_for(250ms);
        DWORD owner = 0;
        GetWindowThreadProcessId(hwnd, &owner);
        if (owner == GetCurrentProcessId()) PostMessageW(hwnd, WM_CLOSE, 0, 0);
    });
    const int result = window->run();
    driver.join();
    DestroyWindow(work);
    const auto elapsed = [&](Clock::time_point end) {
        return std::chrono::duration<double, std::milli>(end - state.inputStart).count();
    };
    std::cout << "{\"inputSent\":" << state.inputSent << ",\"fifo\":" << state.ordered
              << ",\"hwnd\":" << reinterpret_cast<std::uintptr_t>(hwnd)
              << ",\"pid\":" << GetCurrentProcessId()
              << ",\"completed\":" << state.completed << ",\"sendMs\":" << elapsed(state.inputEnd)
              << ",\"inputBeginNs\":"
              << std::chrono::duration_cast<std::chrono::nanoseconds>(state.inputStart.time_since_epoch()).count()
              << ",\"received\":[";
    for (size_t i = 0; i < state.receipts.size(); ++i) {
        const auto& entry = state.receipts[i];
        std::cout << (i ? "," : "") << "{\"message\":" << entry.message << ",\"completed\":" << entry.completed
                  << ",\"ms\":" << elapsed(entry.time) << "}";
    }
    std::cout << "]}\n";
    const auto expected = mode == L"twice" || mode == L"interleaved" ? 4u : 2u;
    return result == 0 && state.inputSent && state.ordered && state.completed == workCount &&
           state.receipts.size() == expected ? 0 : 1;
}
