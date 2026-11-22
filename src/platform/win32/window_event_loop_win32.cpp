#include "window_event_loop_win32.h"
#include "window_dispatch_trace_win32.h"
#include "internal/interaction_trace.h"
#include <algorithm>
#include <utility>
#include <vector>
namespace oneui::win32 {
namespace {
// HWND ownership and WM_QUIT are thread-local. A window on another UI thread
// must not keep this thread's loop alive, and destroying a layout-only window
// outside run() must not leave an exit message for a future window.
thread_local int g_liveWindowCount = 0;
thread_local unsigned int g_runLoopDepth = 0;
thread_local bool g_inputTurnRequested = false;

thread_local std::vector<ThreadFrameSource> g_frameSources;

struct RunLoopScope {
    RunLoopScope() { ++g_runLoopDepth; }
    ~RunLoopScope() {
        if (--g_runLoopDepth == 0)
            g_inputTurnRequested = false;
    }
    RunLoopScope(const RunLoopScope &) = delete;
    RunLoopScope &operator=(const RunLoopScope &) = delete;
};

struct QueuedMouseRelease {
    HWND window = nullptr;
    UINT message = WM_NULL;
};

UINT matchingClientRelease(UINT press) {
    switch (press) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        return WM_LBUTTONUP;
    case WM_RBUTTONDOWN:
        return WM_RBUTTONUP;
    case WM_MBUTTONDOWN:
        return WM_MBUTTONUP;
    default:
        return WM_NULL;
    }
}

bool peekMatchingMouseRelease(const QueuedMouseRelease& release) {
    MSG next{};
    constexpr auto inputProbe = PM_NOREMOVE | PM_QS_INPUT;
    if (!PeekMessageW(&next, nullptr, 0, 0, inputProbe)) {
        // The input-category probe can miss an already observed button-up.
        // A non-removing range probe can expose it again. Recheck the full
        // hardware queue so an intervening message keeps its place.
        if (!PeekMessageW(&next, nullptr, WM_NCMOUSEMOVE, WM_MOUSELAST, PM_NOREMOVE) ||
            !PeekMessageW(&next, nullptr, 0, 0, inputProbe))
            return false;
    }
    return next.hwnd == release.window && next.message == release.message;
}

bool peekMessage(MSG& message, bool& priorityInput, QueuedMouseRelease& pendingRelease) {
    const DispatchTraceScope trace("peek_message");
    const auto release = std::exchange(pendingRelease, {});
    if (release.window && peekMatchingMouseRelease(release) &&
        PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE | PM_QS_INPUT)) {
        // Dequeue at the head even if a reentrant sent message changed it.
        // This continuation is single-use and cannot arm another release.
        priorityInput = true;
        return true;
    }

    // Win32 normally retrieves posted work before hardware input. A yielded
    // batch gives pending input one turn. Finish a client button press if
    // its matching release is already next in the input queue.
    priorityInput = std::exchange(g_inputTurnRequested, false) &&
        PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE | PM_QS_INPUT) != FALSE;
    if (!priorityInput)
        return PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE;
    if (const auto matching = matchingClientRelease(message.message); matching != WM_NULL)
        pendingRelease = {message.hwnd, matching};
    return true;
}

void tickFrameSource(const ThreadFrameSource& source) {
    const DispatchTraceScope trace("frame_source_tick");
    source.tick();
}

} // namespace
void registerFrameSource(ThreadFrameSource source) { g_frameSources.push_back(std::move(source)); }
void registerNativeWindow() { ++g_liveWindowCount; }
void unregisterNativeWindow() {
    if (--g_liveWindowCount == 0 && g_runLoopDepth > 0)
        PostQuitMessage(0);
}
void requestInputTurnAfterPostedWork() {
    if (g_runLoopDepth > 0)
        g_inputTurnRequested = true;
}
int runWindowLoop() {
    const RunLoopScope loopScope;
    const internal::InteractionInputScope loopInteraction(0, WM_NULL, 0);
    MSG message{};
    while (true) {
        // Win7's non-composited paint path can continuously enqueue paint
        // messages. Draining until empty starves the waitable frame timer,
        // freezing animations despite an otherwise responsive message loop.
        // Bound each batch so input and animation handles both get service.
        {
            DispatchTraceScope trace("message_batch");
            const ULONGLONG batchStarted = GetTickCount64();
            unsigned int dispatched = 0;
            bool priorityInput = false;
            QueuedMouseRelease pendingRelease;
            while (peekMessage(message, priorityInput, pendingRelease)) {
                if (message.message == WM_QUIT) {
                    // A nested loop must pass the thread's exit request back
                    // to its caller instead of consuming it on its behalf.
                    if (g_runLoopDepth > 1)
                        PostQuitMessage(static_cast<int>(message.wParam));
                    return static_cast<int>(message.wParam);
                }
                const DispatchTraceScope dispatch(
                    "translate_dispatch", reinterpret_cast<std::uintptr_t>(message.hwnd), message.message);
                TranslateMessage(&message);
                const internal::InteractionInputScope interaction(
                    reinterpret_cast<std::uintptr_t>(message.hwnd), message.message, message.time);
                DispatchMessageW(&message);
                if (++dispatched >= 64 || GetTickCount64() - batchStarted >= 8) {
                    // The thread also dispatches messages for native helpers,
                    // including COM's private window. A slow helper can consume
                    // this entire batch without entering OneUI's callback path.
                    // A prioritized input message cannot claim another turn.
                    if (!priorityInput) g_inputTurnRequested = true;
                    break;
                }
            }
            trace.setDispatched(dispatched);
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
                tickFrameSource(source);
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
                tickFrameSource(source);
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
