#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <windows.h>
namespace oneui::win32 {
struct ThreadFrameSource {
    HANDLE timer;
    std::weak_ptr<std::atomic<bool>> alive;
    std::function<void()> tick;
};
void registerFrameSource(ThreadFrameSource source);
void registerNativeWindow();
void unregisterNativeWindow();
int runWindowLoop();
} // namespace oneui::win32
