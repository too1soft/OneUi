#pragma once
#include <X11/Xlib.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace oneui::linux_platform {
// A clipboard peer can disappear between SelectionRequest and our response.
// Trap only BadWindow for that exact peer; all unrelated errors retain the
// application's handler. Xlib's process-wide handler is restored before return.
class ClipboardPeerErrorScope {
public:
    ClipboardPeerErrorScope(Display* display, ::Window peer)
        : lock_(mutex_), display_(display) {
        XSync(display_, False);
        ready_ = false;
        failed_ = false;
        activeDisplay_ = display;
        activePeer_ = peer;
        previous_ = XSetErrorHandler(handle);
        forwardingHandler_ = previous_;
        ready_ = true;
    }
    ~ClipboardPeerErrorScope() {
        XSync(display_, False);
        XSetErrorHandler(previous_);
        activeDisplay_ = nullptr;
    }
    bool failed() {
        XSync(display_, False);
        return failed_;
    }
    ClipboardPeerErrorScope(const ClipboardPeerErrorScope&) = delete;
    ClipboardPeerErrorScope& operator=(const ClipboardPeerErrorScope&) = delete;
private:
    static int handle(Display* display, XErrorEvent* event) {
        // The handler is process-wide: callbacks on another application's Xlib
        // thread must still be forwarded and must not touch stack-owned state.
        while (!ready_) std::this_thread::yield();
        if (display == activeDisplay_ && event->error_code == BadWindow &&
            event->resourceid == activePeer_) {
            failed_ = true;
            return 0;
        }
        const auto previous = forwardingHandler_.load();
        return previous ? previous(display, event) : 0;
    }
    inline static std::mutex mutex_;
    inline static std::atomic<Display*> activeDisplay_{nullptr};
    inline static std::atomic<::Window> activePeer_{0};
    inline static std::atomic<XErrorHandler> forwardingHandler_{nullptr};
    inline static std::atomic<bool> ready_{true}, failed_{false};
    std::unique_lock<std::mutex> lock_;
    Display* display_;
    XErrorHandler previous_ = nullptr;
};
} // namespace oneui::linux_platform
