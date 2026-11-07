#pragma once

#include "oneui/mobile/presentation.h"
#include <array>
#include <cstdint>
#include <functional>

namespace oneui::mobile {
// Shared state for on-screen modifier keys. The consumer maps HID usages to
// its protocol and releases the state on panel, focus and session changes.
class RemoteModifierKeys final {
  public:
    using Sink = std::function<void(uint32_t, bool)>;
    void setSink(Sink sink) {
        release();
        sink_ = std::move(sink);
    }
    bool pressed(int index) const {
        return index >= 0 && index < 4 && (held_ & (1u << index));
    }
    void toggle(int index) {
        if (index < 0 || index >= 4)
            return;
        held_ ^= 1u << index;
        if (sink_)
            sink_(usages_[index], pressed(index));
    }
    void release() {
        const auto held = held_;
        held_ = 0;
        for (int index = 3; index >= 0; --index)
            if ((held & (1u << index)) && sink_)
                sink_(usages_[index], false);
    }

  private:
    static constexpr std::array<uint32_t, 4> usages_{0x0700e0, 0x0700e2, 0x0700e1, 0x0700e3};
    Sink sink_;
    unsigned held_ = 0;
};

inline std::shared_ptr<Stack> remoteModifierRow(const RemoteModifierKeys &state, ControlPalette palette,
                                               std::function<void(int)> toggle, float scale = 1,
                                               bool disabled = false) {
    static const std::array<std::wstring, 4> labels{L"Ctrl", L"Alt", L"Shift", L"Win / ⌘"};
    auto row = std::make_shared<Stack>(StackDirection::Row);
    row->setGap(8);
    row->setPreferredSize({0, std::max(48.f, 48 * scale)});
    for (int index = 0; index < 4; ++index) {
        auto key = actionButton(labels[index], [toggle, index] { toggle(index); }, palette,
                                state.pressed(index) ? ActionKind::Primary : ActionKind::Secondary, scale);
        key->setAccessibleRole(AccessibilityRole::CheckBox);
        auto info = key->accessibilityState();
        info.checked = state.pressed(index);
        key->setAccessibilityState(info);
        key->setAccessibleDescription(L"点按保持按住，再次点按松开");
        key->setDisabled(disabled);
        row->add(key);
        StackFlex fill;
        fill.grow = 1;
        fill.basis = 0;
        row->setFlex(key, fill);
    }
    return row;
}
} // namespace oneui::mobile
