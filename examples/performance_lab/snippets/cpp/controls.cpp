#include <memory>
#include <oneui/controls/button.h>

std::shared_ptr<oneui::Button> example_button(std::function<void()> onClick) {
    auto button = std::make_shared<oneui::Button>(L"执行");
    button->setOnClick(std::move(onClick));
    // Captured objects must outlive the callback, or use weak_ptr.
    button->setTooltip(L"也可以用键盘激活");
    return button;
}
