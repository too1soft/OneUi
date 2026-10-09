#pragma once
#include "oneui/widget.h"

namespace oneui::test {
inline KeyEvent editKey(Key key) {
    KeyEvent event;
    event.key = key;
#ifdef __APPLE__
    event.win = true; // Physical Command, independent of raw Control.
#else
    event.control = true;
#endif
    return event;
}

inline KeyEvent terminalClipboardKey(Key key) {
    auto event = editKey(key);
#ifndef __APPLE__
    event.shift = true; // Keep Ctrl+C available for terminal interrupts.
#endif
    return event;
}
} // namespace oneui::test
