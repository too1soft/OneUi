#pragma once
#include "internal/runtime_error.h"
#include "oneui/controls/terminal_view.h"

#include <algorithm>
#include "internal/ui_clock.h"
#include <cmath>
#include <cwctype>
#include <limits>
#include <utility>

namespace oneui::terminal_detail {
constexpr std::size_t kMaximumCells = 2'000'000;
constexpr unsigned int kVirtualKeyShift = 0x10;
constexpr unsigned int kVirtualKeyControl = 0x11;
constexpr unsigned int kVirtualKeyAlt = 0x12;
constexpr unsigned int kVirtualKeyLeftWin = 0x5B;
constexpr unsigned int kVirtualKeyRightWin = 0x5C;
constexpr double kCursorBlinkPeriodMs = 1060.0;
constexpr double kCursorBlinkOnMs = 530.0;
constexpr double kSlowTextBlinkPeriodMs = 1000.0;
constexpr double kSlowTextBlinkOnMs = 500.0;
constexpr double kRapidTextBlinkPeriodMs = 500.0;
constexpr double kRapidTextBlinkOnMs = 250.0;
constexpr double kMultiClickIntervalMs = 500.0;
constexpr std::size_t kMaximumSparseInvalidations = 32;

inline bool hasStyle(const TerminalCell& cell, TerminalCellStyle style) {
    return (cell.style & static_cast<std::uint32_t>(style)) != 0;
}

inline Color dim(Color color) {
    color.r = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.r) * 0.62f));
    color.g = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.g) * 0.62f));
    color.b = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.b) * 0.62f));
    return color;
}

inline bool sameColor(Color left, Color right) {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

inline bool sameTerminalCell(const TerminalCell& left, const TerminalCell& right) {
    return left.text == right.text && sameColor(left.foreground, right.foreground) &&
           sameColor(left.background, right.background) && left.style == right.style &&
           left.hyperlinkId == right.hyperlinkId &&
           left.underlineStyle == right.underlineStyle &&
           sameColor(left.underlineColor, right.underlineColor) &&
           left.underlineColorSet == right.underlineColorSet;
}

inline bool isCopyShortcut(const KeyEvent& event) {
    return platform::terminalCopyShortcut(event);
}

inline bool isPasteShortcut(const KeyEvent& event) {
    return platform::terminalPasteShortcut(event);
}

inline bool isModifierKey(const KeyEvent& event) {
    return event.virtualKey == kVirtualKeyShift || event.virtualKey == kVirtualKeyControl ||
           event.virtualKey == kVirtualKeyAlt || event.virtualKey == kVirtualKeyLeftWin ||
           event.virtualKey == kVirtualKeyRightWin;
}

inline double currentTimeMs() {
    return internal::uiTimeMs();
}

}
