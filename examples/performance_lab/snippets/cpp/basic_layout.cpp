#include <oneui/oneui.h>

// Return the root to Window::setContent; the widget tree retains its children.
std::shared_ptr<oneui::Stack> example_layout() {
    auto row = std::make_shared<oneui::Stack>(oneui::StackDirection::Row);
    row->setEngine(oneui::StackEngine::Yoga);
    row->setGap(12);
    row->setPadding(oneui::Insets{16});
    auto first = std::make_shared<oneui::Label>(L"固定宽度");
    auto second = std::make_shared<oneui::Label>(L"弹性宽度");
    row->add(first);
    row->add(second);
    row->setFlex(first, {0, 0, 120, 80});
    row->setFlex(second, {1, 1, 0, 80});
    return row;
}
