#pragma once

#include "oneui/mobile/navigation.h"
#include "oneui/mobile/remote_control_style.h"
#include "oneui/mobile/toolbar_button.h"
#include "oneui/mobile/action_icons.h"

namespace oneui::mobile {
// A docked keyboard header. The owner reserves its height below the video;
// it never anchors itself over content or controls the operating-system IME.
inline std::shared_ptr<Stack> remoteKeyboardBar(
    int section, ControlPalette palette, std::function<void(int)> select,
    std::function<void()> backspace, std::function<void()> close, float scale = 1,
    std::shared_ptr<Widget> capture = {}) {
    const float touch = std::max(48.f, 48 * scale);
    auto bar = std::make_shared<Stack>(StackDirection::Row);
    bar->setGap(4);
    bar->setPreferredSize({0, touch});
    StackFlex fill;
    fill.grow = 1;
    fill.basis = 0;
    auto tabs = std::make_shared<Stack>(StackDirection::Row);
    const std::wstring titles[]{L"手机键盘", L"电脑键盘", L"快捷键"};
    for (int i = 0; i < 3; ++i) {
        if (i == 0 && section == 0 && capture) {
            capture->setPreferredSize({0, touch});
            tabs->add(capture);tabs->setFlex(capture, fill);
            continue;
        }
        auto item = remoteControlButton(titles[i], [select, i] { select(i); }, palette,
                                         RemoteControlStyle::Segment, i == section, scale);
        item->setAccessibleRole(AccessibilityRole::CheckBox);
        auto state = item->accessibilityState();state.checked = i == section;item->setAccessibilityState(state);
        tabs->add(item);tabs->setFlex(item, fill);
    }
    bar->add(tabs);bar->setFlex(tabs, fill);
    for (int i = 0; i < 2; ++i) {
        auto action = std::make_shared<ToolbarButton>(L"",IconSymbol::ChevronDown,false,
                                                      palette,i?close:backspace,scale);
        action->setIconOnly(true);
        if(!i)action->setGlyphPainter(paintBackspaceIcon);
        action->setAccessibleName(i ? L"关闭键盘" : L"退格");
        action->setPreferredSize({touch, touch});
        action->setFocusOnPointer(false);
        bar->add(action);
    }
    return bar;
}
} // namespace oneui::mobile
