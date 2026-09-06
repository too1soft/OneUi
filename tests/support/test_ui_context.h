#pragma once
#include "internal/ui_clock.h"
#include "oneui/clipboard.h"
#include "oneui/view.h"
#include "recording_canvas.h"
#include <deque>
#include <stdexcept>

namespace oneui::test_support {
// Deterministic UI-thread fixture. No sleeping and no global OS clipboard.
class TestUiContext {
public:
    explicit TestUiContext(std::shared_ptr<Widget> widget) : root(std::move(widget)), clock_([this] { return now; }) {}
    ~TestUiContext() { close(); }
    double now = 1000;
    std::shared_ptr<Widget> root;
    CommandScope windowCommands;
    std::shared_ptr<MemoryClipboard> clipboard = std::make_shared<MemoryClipboard>();
    std::function<bool(const KeyEvent&)> rawKey;
    bool key(KeyEvent event) {
        if (closed_) return false;
        auto content = root;
        if (rawKey && rawKey(event)) return true;
        if (closed_ || !content) return true;
        if (dispatchCommandKey(content, &windowCommands, event)) return true;
        return event.pressed ? content->onKeyDown(event) : content->onKeyUp(event);
    }
    void replay(const std::vector<KeyEvent>& events) { for (auto event : events) key(event); }
    void advance(double milliseconds) {
        if (milliseconds < 0) throw std::invalid_argument("Cannot rewind a UI clock");
        now += milliseconds;
        if (!closed_ && root) root->tickAnimations(now);
        drain();
    }
    bool post(std::function<void()> task) {
        if (closed_) return false;
        tasks_.push_back(std::move(task)); return true;
    }
    void drain() {
        // Nested posts belong to the next turn, avoiding a self-posting busy loop.
        auto tasks = std::move(tasks_); tasks_.clear();
        for (auto& task : tasks) { if (closed_) break; task(); }
    }
    void close() { closed_ = true; tasks_.clear(); if (root) root->onFocusChanged(false); root.reset(); }
    RecordingCanvas paint() { RecordingCanvas canvas; if (root && !closed_) root->paint(canvas); return canvas; }
private:
    bool closed_ = false;
    std::deque<std::function<void()>> tasks_;
    internal::ScopedUiClock clock_;
};
}
