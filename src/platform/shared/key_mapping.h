#pragma once
#include "oneui/widget.h"
namespace oneui::platform {
inline unsigned int asciiVirtualKey(unsigned int code) {
    if (code >= 'a' && code <= 'z')
        return code - 'a' + 'A';
    switch (code) {
    case ';':
        return 186;
    case '=':
        return 187;
    case ',':
        return 188;
    case '-':
        return 189;
    case '.':
        return 190;
    case '/':
        return 191;
    case '`':
        return 192;
    case '[':
        return 219;
    case '\\':
        return 220;
    case ']':
        return 221;
    case '\'':
        return 222;
    default:
        return code;
    }
}
inline bool printableVirtualKey(unsigned int code) {
    return code == 0 || code == 32 || (code >= 48 && code <= 90) || (code >= 186 && code <= 222);
}
// virtualKey uses the existing Win32-compatible namespace on every backend;
// scanCode remains native. This keeps terminal/raw-key consumers compatible.
inline Key logicalKey(unsigned int code) {
    switch (code) {
    case 9:
        return Key::Tab;
    case 13:
        return Key::Enter;
    case 32:
        return Key::Space;
    case 8:
        return Key::Backspace;
    case 27:
        return Key::Escape;
    case 37:
        return Key::Left;
    case 38:
        return Key::Up;
    case 39:
        return Key::Right;
    case 40:
        return Key::Down;
    case 36:
        return Key::Home;
    case 35:
        return Key::End;
    case 33:
        return Key::PageUp;
    case 34:
        return Key::PageDown;
    case 46:
        return Key::Delete;
    case 113:
        return Key::F2;
    case 'A':
        return Key::A;
    case 'C':
        return Key::C;
    case 'V':
        return Key::V;
    case 'X':
        return Key::X;
    default:
        return Key::Other;
    }
}
} // namespace oneui::platform
