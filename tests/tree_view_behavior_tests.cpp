#include "oneui/controls/tree_view.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

class RecordingCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override { ++fills; }
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring& text, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {
        texts.push_back(text);
    }
    float measureTextWidth(const std::wstring&, float size, int = 400) const override { return size * 0.60f; }

    int fills = 0;
    std::vector<std::wstring> texts;
};

void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << " failed\n";
        ++failures;
    }
}

void expectEqual(const char* name, std::size_t actual, std::size_t expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void populateTree(oneui::TreeView& tree) {
    tree.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 180.0f});
    tree.setItems({
        {L"platform", L"", L"Platform", L"12", true},
        {L"production", L"platform", L"Production", L"8", true},
        {L"staging", L"platform", L"Staging", L"4", true},
        {L"finance", L"", L"Finance", L"3", false},
        {L"payments", L"finance", L"Payments", L"3", true},
    });
}

void testVisibleHierarchyAndKeyboardNavigation() {
    oneui::TreeView tree;
    populateTree(tree);
    expectEqual("initial visible tree rows", tree.visibleItemCount(), 4);
    expectTrue("initial tree content height", tree.contentHeight() == 128.0f);
    expectTrue("initial root selected", tree.selectedId() == L"platform");

    tree.onKeyDown(oneui::KeyEvent{oneui::Key::Right});
    expectTrue("right moves into expanded child", tree.selectedId() == L"production");
    tree.onKeyDown(oneui::KeyEvent{oneui::Key::Left});
    expectTrue("left moves to parent", tree.selectedId() == L"platform");
    tree.onKeyDown(oneui::KeyEvent{oneui::Key::Left});
    expectTrue("left collapses expanded parent", !tree.isExpanded(L"platform"));
    expectEqual("collapsed rows hidden", tree.visibleItemCount(), 2);
    tree.onKeyDown(oneui::KeyEvent{oneui::Key::Right});
    expectTrue("right expands collapsed parent", tree.isExpanded(L"platform"));
    expectEqual("expanded rows restored", tree.visibleItemCount(), 4);

    tree.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    expectTrue("down selects next visible item", tree.selectedId() == L"production");
}

void testPointerToggleAndSelectionCallbacks() {
    oneui::TreeView tree;
    populateTree(tree);
    std::wstring changed;
    int changeCount = 0;
    std::wstring expansionChanged;
    bool expansionState = true;
    int expansionChangeCount = 0;
    tree.setOnSelectionChanged([&](const std::wstring& id) {
        changed = id;
        ++changeCount;
    });
    tree.setOnExpansionChanged([&](const std::wstring& id, bool expanded) {
        expansionChanged = id;
        expansionState = expanded;
        ++expansionChangeCount;
    });

    tree.onMouseDown(oneui::MouseEvent{{10.0f, 16.0f}, oneui::MouseButton::Left});
    tree.onMouseUp(oneui::MouseEvent{{10.0f, 16.0f}, oneui::MouseButton::Left});
    expectTrue("pointer toggles root", !tree.isExpanded(L"platform"));
    expectEqual("pointer collapse hides child rows", tree.visibleItemCount(), 2);
    expectTrue("expansion callback reports ID", expansionChanged == L"platform");
    expectTrue("expansion callback reports collapsed state", !expansionState);
    expectEqual("expansion callback once", static_cast<std::size_t>(expansionChangeCount), 1);

    tree.onMouseDown(oneui::MouseEvent{{90.0f, 48.0f}, oneui::MouseButton::Left});
    tree.onMouseUp(oneui::MouseEvent{{90.0f, 48.0f}, oneui::MouseButton::Left});
    expectTrue("pointer selects second root", tree.selectedId() == L"finance");
    expectTrue("selection callback reports ID", changed == L"finance");
    expectEqual("selection callback once", static_cast<std::size_t>(changeCount), 1);
}

void testCollapsingTheSelectedChildReportsTheFallbackSelection() {
    oneui::TreeView tree;
    populateTree(tree);
    tree.setSelectedId(L"production");
    std::wstring changed;
    tree.setOnSelectionChanged([&](const std::wstring& id) { changed = id; });

    tree.setExpanded(L"platform", false);
    expectTrue("collapsed child selection falls back to parent", tree.selectedId() == L"platform");
    expectTrue("collapsed child fallback notifies caller", changed == L"platform");
}

void testPaintAndAccessibilityReflectTheSelectedNode() {
    oneui::TreeView tree;
    populateTree(tree);
    tree.setSelectedId(L"staging");
    RecordingCanvas canvas;
    tree.paint(canvas);

    bool sawTitle = false;
    bool sawDetail = false;
    for (const auto& text : canvas.texts) {
        sawTitle = sawTitle || text == L"Staging";
        sawDetail = sawDetail || text == L"4";
    }
    expectTrue("tree paints selected title", sawTitle);
    expectTrue("tree paints structured detail", sawDetail);
    const auto info = tree.accessibilityInfo();
    expectTrue("tree accessibility role", info.role == oneui::AccessibilityRole::List);
    expectTrue("tree accessibility value", info.value == L"Staging - 4");
    expectTrue("tree accessibility selected", info.state.selected);
}

void testInvalidParentsAndDuplicateIdsRemainSafe() {
    oneui::TreeView tree;
    tree.setItems({
        {L"root", L"missing", L"Root", L"", true},
        {L"root", L"", L"Duplicate", L"", true},
        {L"self", L"self", L"Self", L"", true},
    });
    expectEqual("invalid nodes become roots and duplicate is skipped", tree.visibleItemCount(), 2);
    expectTrue("first unique item remains selectable", tree.selectedId() == L"root");
}

} // namespace

int main() {
    testVisibleHierarchyAndKeyboardNavigation();
    testPointerToggleAndSelectionCallbacks();
    testCollapsingTheSelectedChildReportsTheFallbackSelection();
    testPaintAndAccessibilityReflectTheSelectedNode();
    testInvalidParentsAndDuplicateIdsRemainSafe();

    if (failures != 0) {
        std::cerr << failures << " tree view behavior test(s) failed.\n";
        return 1;
    }
    return 0;
}
