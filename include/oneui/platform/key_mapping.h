#pragma once

namespace oneui::platform {

// One source of platform shortcut semantics. The legacy raw `win` field carries
// Meta/Command on every backend; keep its ABI while exposing a neutral accessor.
#ifdef __APPLE__
inline constexpr bool primaryUsesMeta = true;
#else
inline constexpr bool primaryUsesMeta = false;
#endif

inline constexpr bool primaryModifier(bool control, bool meta) {
    return primaryUsesMeta ? meta : control;
}

template <typename Event>
bool terminalCopyShortcut(const Event& event) {
    using Key = decltype(event.key);
    if constexpr (primaryUsesMeta) return event.meta() && event.key == Key::C;
    return (event.control && event.shift && event.key == Key::C) ||
           (event.control && !event.shift && event.virtualKey == 0x2D);
}

template <typename Event>
bool terminalPasteShortcut(const Event& event) {
    using Key = decltype(event.key);
    if constexpr (primaryUsesMeta) return event.meta() && event.key == Key::V;
    return (event.control && event.shift && event.key == Key::V) ||
           (!event.control && event.shift && event.virtualKey == 0x2D);
}

} // namespace oneui::platform
