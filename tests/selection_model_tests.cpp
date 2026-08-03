#include "oneui/selection_model.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expectEqual(const std::string& name, int actual, int expected) {
    if (actual == expected) {
        return;
    }
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    ++failures;
}

void expectIndices(
    const std::string& name,
    const oneui::SelectionModel& model,
    std::vector<int> expected) {
    if (model.selectedIndices() == expected) {
        return;
    }
    std::cerr << name << ": selected indices differ\n";
    ++failures;
}

void testSingleSelectionAndBounds() {
    oneui::SelectionModel model;
    model.setItemCount(5);
    model.selectOnly(3);
    expectIndices("single selection", model, {3});
    expectEqual("single active", model.activeIndex(), 3);
    expectEqual("single anchor", model.anchorIndex(), 3);

    model.setItemCount(3);
    expectIndices("shrink removes invalid selection", model, {});
    expectEqual("shrink clears invalid active", model.activeIndex(), -1);
    expectEqual("shrink clears invalid anchor", model.anchorIndex(), -1);
}

void testPointerModifierSemantics() {
    oneui::SelectionModel model;
    model.setItemCount(10);
    model.setMode(oneui::SelectionMode::Multiple);

    model.applyPointerSelection(2, false, false);
    model.applyPointerSelection(5, true, false);
    expectIndices("ctrl adds a row", model, {2, 5});
    model.applyPointerSelection(2, true, false);
    expectIndices("ctrl removes a row", model, {5});

    model.applyPointerSelection(7, false, true);
    expectIndices("shift replaces with anchor range", model, {2, 3, 4, 5, 6, 7});
    expectEqual("shift keeps range anchor", model.anchorIndex(), 2);
    expectEqual("shift updates active row", model.activeIndex(), 7);

    model.applyPointerSelection(9, true, true);
    expectIndices("ctrl shift appends a range", model, {2, 3, 4, 5, 6, 7, 8, 9});
}

void testKeyboardAndSelectAllSemantics() {
    oneui::SelectionModel model;
    model.setItemCount(6);
    model.setMode(oneui::SelectionMode::Multiple);
    model.selectOnly(1);

    model.applyKeyboardSelection(3, true, false);
    expectIndices("ctrl arrow preserves selection", model, {1});
    expectEqual("ctrl arrow moves active row", model.activeIndex(), 3);

    model.applyKeyboardSelection(4, false, true);
    expectIndices("shift arrow extends from anchor", model, {1, 2, 3, 4});
    model.selectAll();
    expectIndices("ctrl a selects all", model, {0, 1, 2, 3, 4, 5});

    model.setMode(oneui::SelectionMode::Single);
    expectIndices("single mode retains active row", model, {4});
}

void testProgrammaticSelectionNormalizesInput() {
    oneui::SelectionModel model;
    model.setItemCount(5);
    model.setMode(oneui::SelectionMode::Multiple);
    model.setSelectedIndices({4, 2, 2, -1, 8, 1});
    expectIndices("programmatic selection is sorted unique and bounded", model, {1, 2, 4});
    expectEqual("programmatic selection activates last row", model.activeIndex(), 4);
    model.clear();
    expectIndices("clear removes selection", model, {});
}

} // namespace

int main() {
    testSingleSelectionAndBounds();
    testPointerModifierSemantics();
    testKeyboardAndSelectAllSemantics();
    testProgrammaticSelectionNormalizesInput();

    if (failures != 0) {
        std::cerr << failures << " selection model test(s) failed.\n";
        return 1;
    }
    std::cout << "Selection model tests passed.\n";
    return 0;
}
