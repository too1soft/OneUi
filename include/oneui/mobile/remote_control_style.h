#pragma once

#include "oneui/mobile/presentation.h"

namespace oneui::mobile {
enum class RemoteControlStyle { Segment, Key, Modifier };

// Explicit states keep session controls independent of form-button defaults.
inline std::shared_ptr<Button> remoteControlButton(
    std::wstring title, std::function<void()> action, const ControlPalette &palette,
    RemoteControlStyle kind, bool selected = false, float scale = 1) {
    auto button = std::make_shared<Button>(std::move(title));
    const bool segment = kind == RemoteControlStyle::Segment;
    ButtonStateStyleOverride normal;
    normal.background = segment ? (selected ? palette.pressed : Color{0, 0, 0, 0}) : palette.surface;
    normal.foreground = segment && !selected ? palette.secondary : palette.ink;
    normal.fontSize = (segment ? 13.f : 14.f) * scale;
    normal.fontWeight = selected ? 600 : 450;
    normal.padding = Insets{0, segment ? 2.f : 6.f};
    normal.radius = 8;
    normal.borderWidth = segment ? 0.f : .5f;
    normal.border = palette.separator;
    if (kind == RemoteControlStyle::Modifier && selected) {
        normal.background = palette.tint;
        normal.foreground = palette.ink.r > 200 ? Color{20, 35, 65} : Color{255, 255, 255};
        normal.borderWidth = 0;
    }
    ButtonStyleOverride style;
    style.normal = normal;
    auto hovered = normal;
    if(kind != RemoteControlStyle::Modifier || !selected)hovered.background = palette.pressed;
    style.hovered = hovered;
    auto pressed = hovered;
    if(kind != RemoteControlStyle::Modifier || !selected)pressed.background = palette.separator;
    style.pressed = pressed;
    auto disabled = normal;disabled.foreground = palette.secondary;style.disabled = disabled;
    auto focused = normal;focused.border = palette.tint;focused.borderWidth = 1.5f;style.focusVisible = focused;
    button->setStyleOverride(style);
    button->setPreferredSize({0, std::max(48.f, 48 * scale)});
    button->setOnClick(std::move(action));
    button->setFocusOnPointer(false);
    return button;
}
} // namespace oneui::mobile
