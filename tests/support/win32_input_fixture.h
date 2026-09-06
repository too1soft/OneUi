#pragma once
#ifdef _WIN32
#include <windows.h>
#include <stdexcept>

namespace oneui::test_support {
// SendMessage does not update GetKeyState. Isolate synthetic input from keys
// currently held by the user; SetKeyboardState only changes this test thread.
class ScopedKeyboardState {
public:
    ScopedKeyboardState() {
        if (!GetKeyboardState(saved_)) throw std::runtime_error("GetKeyboardState failed");
        BYTE clear[256]{};
        if (!SetKeyboardState(clear)) throw std::runtime_error("SetKeyboardState failed");
    }
    ~ScopedKeyboardState() { SetKeyboardState(saved_); }
    ScopedKeyboardState(const ScopedKeyboardState&) = delete;
    ScopedKeyboardState& operator=(const ScopedKeyboardState&) = delete;
private:
    BYTE saved_[256]{};
};
} // namespace oneui::test_support
#endif
