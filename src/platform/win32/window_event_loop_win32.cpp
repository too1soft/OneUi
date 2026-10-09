#include "window_event_loop_win32.h"
#include <algorithm>
#include <vector>
namespace oneui::win32 {
namespace {
// HWND ownership and WM_QUIT are thread-local. A window on another UI thread
// must not keep this thread's loop alive, and destroying a layout-only window
// outside run() must not leave an exit message for a future window.
thread_local int g_liveWindowCount = 0;
thread_local unsigned int g_runLoopDepth = 0;

thread_local std::vector<ThreadFrameSource> g_frameSources;

struct RunLoopScope {
    RunLoopScope() { ++g_runLoopDepth; }
    ~RunLoopScope() { --g_runLoopDepth; }
    RunLoopScope(const RunLoopScope &) = delete;
    RunLoopScope &operator=(const RunLoopScope &) = delete;
};

} // namespace
void registerFrameSource(ThreadFrameSource source) { g_frameSources.push_back(std::move(source)); }
void registerNativeWindow() { ++g_liveWindowCount; }
void unregisterNativeWindow() {
    if (--g_liveWindowCount == 0 && g_runLoopDepth > 0)
        PostQuitMessage(0);
}
int runWindowLoop() {
    const RunLoopScope loopScope;
    MSG message{};
    while (true) {
        // Win7's non-composited paint path can continuously enqueue paint
        // messages. Draining until empty starves the waitable frame timer,
        // freezing animations despite an otherwise responsive message loop.
        // Bound each batch so input and animation handles both get service.
        const ULONGLONG batchStarted = GetTickCount64();
        unsigned int dispatched = 0;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                // A nested loop must pass the thread's exit request back
                // to its caller instead of consuming it on its behalf.
                if (g_runLoopDepth > 1)
                    PostQuitMessage(static_cast<int>(message.wParam));
                return static_cast<int>(message.wParam);
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (++dispatched >= 64 || GetTickCount64() - batchStarted >= 8)
                break;
        }

        // All windows on this UI thread share the message loop. Waiting
        // only on `this` starves animation/layout in secondary windows.
        g_frameSources.erase(std::remove_if(g_frameSources.begin(), g_frameSources.end(),
                                            [](const auto &source) {
                                                auto alive = source.alive.lock();
                                                return !alive || !alive->load();
                                            }),
                             g_frameSources.end());
        const auto sources = g_frameSources;
        std::vector<HANDLE> handles;
        std::vector<ThreadFrameSource> waiting;
        bool ticked = false;
        for (const auto &source : sources) {
            auto alive = source.alive.lock();
            if (!alive || !alive->load())
                continue;
            if (WaitForSingleObject(source.timer, 0) == WAIT_OBJECT_0) {
                source.tick();
                ticked = true;
            } else if (handles.size() < MAXIMUM_WAIT_OBJECTS - 1) {
                handles.push_back(source.timer);
                waiting.push_back(source);
            }
        }
        if (ticked)
            continue; // callbacks can create or destroy windows
        const DWORD handleCount = static_cast<DWORD>(handles.size());
        const DWORD waitResult = MsgWaitForMultipleObjectsEx(
            handleCount, handles.empty() ? nullptr : handles.data(),
            sources.size() > handles.size() ? 8 : INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + handleCount) {
            const auto &source = waiting[waitResult - WAIT_OBJECT_0];
            auto alive = source.alive.lock();
            if (alive && alive->load())
                source.tick();
            continue;
        }
        if (waitResult == WAIT_TIMEOUT)
            continue;
        if (waitResult == WAIT_OBJECT_0 + handleCount) {
            continue;
        }
        return -1;
    }
}
} // namespace oneui::win32
