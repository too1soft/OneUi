#include "oneui/controls/select.h"

#include <iostream>

namespace {
bool keyboardChoiceSurvivesEquivalentItems() {
    oneui::State<int> selected(0);
    oneui::Select select;
    select.setFrame(oneui::Rect{0, 0, 120, 30});
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.bindSelectedIndex(selected);
    int changes = 0;
    select.setOnChanged([&](int) { ++changes; });

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    return selected.get() == 2 && changes == 1;
}

bool pressedChoiceSurvivesEquivalentItems() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0, 0, 120, 30});
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    int changes = 0;
    select.setOnChanged([&](int) { ++changes; });
    const oneui::MouseEvent field{oneui::Point{10, 10}};
    const oneui::MouseEvent option{oneui::Point{10, 79}};

    select.onMouseDown(field);
    select.onMouseUp(field);
    select.onMouseDown(option);
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.onMouseUp(option);
    return select.selectedIndex() == 1 && changes == 1;
}

bool equivalentItemsStillNormalizeInvalidBinding() {
    oneui::State<int> selected(0);
    oneui::Select select;
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.bindSelectedIndex(selected);
    selected.set(99);
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    return selected.get() == 2 && select.selectedIndex() == 2;
}

bool explicitSelectionChangeReplacesProvisionalChoice() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0, 0, 120, 30});
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    select.setSelectedIndex(2);
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    return select.selectedIndex() == 2;
}

bool boundSelectionChangeReplacesProvisionalChoice() {
    oneui::State<int> selected(0);
    oneui::Select select;
    select.setFrame(oneui::Rect{0, 0, 120, 30});
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.bindSelectedIndex(selected);
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    selected.set(2);
    select.setItems({L"Choose a session", L"Primary", L"Secondary"});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    return selected.get() == 2;
}
} // namespace

int main() {
    bool passed = true;
    if (!keyboardChoiceSurvivesEquivalentItems()) {
        std::cerr << "Equivalent item refresh lost the keyboard choice.\n";
        passed = false;
    }
    if (!pressedChoiceSurvivesEquivalentItems()) {
        std::cerr << "Equivalent item refresh lost the pressed choice.\n";
        passed = false;
    }
    if (!equivalentItemsStillNormalizeInvalidBinding()) {
        std::cerr << "Equivalent items did not normalize an invalid bound index.\n";
        passed = false;
    }
    if (!explicitSelectionChangeReplacesProvisionalChoice()) {
        std::cerr << "Explicit selection change was overwritten by a provisional choice.\n";
        passed = false;
    }
    if (!boundSelectionChangeReplacesProvisionalChoice()) {
        std::cerr << "Bound selection change was overwritten by a provisional choice.\n";
        passed = false;
    }
    if (passed) std::cout << "Select rebinding tests passed.\n";
    return passed ? 0 : 1;
}
