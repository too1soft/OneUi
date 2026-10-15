#include "oneui/controls/tree_view.h"

#include <chrono>
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

std::vector<oneui::TreeItem> largeTree(int groups, int leaves) {
    std::vector<oneui::TreeItem> items;
    for (int g = 0; g < groups; ++g) {
        const auto group = L"g" + std::to_wstring(g);
        items.push_back({group, L"", L"Group " + std::to_wstring(g), L"", true});
        for (int l = 0; l < leaves; ++l)
            items.push_back({group + L"." + std::to_wstring(l), group, L"Leaf " + std::to_wstring(l), L"", false});
    }
    return items;
}

void testInternalScrollingPaintsOnlyVisibleRows() {
    oneui::TreeView tree;
    tree.setInternalScrolling(true);
    tree.setRowHeight(24.0f);
    tree.setFrame(oneui::Rect{0.0f, 0.0f, 300.0f, 480.0f});
    tree.setItems(largeTree(500, 99));
    expectEqual("fifty thousand visible rows", tree.visibleItemCount(), 50000);
    expectTrue("preferred height does not grow with content", tree.preferredSize().height < 1000.0f);
    expectTrue("scroll range covers the content", tree.maxScrollOffset() == 24.0f * 50000 - 480.0f);
    RecordingCanvas canvas;
    tree.paint(canvas);
    expectTrue("only visible rows are painted", canvas.texts.size() <= 30);
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 200; ++frame) {
        oneui::MouseWheelEvent wheel;
        wheel.position = {100.0f, 100.0f};
        wheel.deltaY = -1.0f;
        tree.onMouseWheel(wheel);
        RecordingCanvas frameCanvas;
        tree.paint(frameCanvas);
    }
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 200.0;
    std::cout << "tree view frame (50k visible nodes, recording canvas): " << ms << " ms\n";
    expectTrue("frames stay cheap", ms < 4.0);
    expectTrue("wheel scrolled", tree.scrollOffset() == 200.0f * 72.0f);
    tree.setSelectedId(L"g400.5");
    expectTrue("selection scrolls into view",
               tree.scrollOffset() <= 24.0f * (400 * 100 + 6) && tree.scrollOffset() + 480.0f >= 24.0f * (400 * 100 + 6));
    oneui::KeyEvent end;
    end.key = oneui::Key::End;
    tree.onKeyDown(end);
    expectTrue("End selects the last row", tree.selectedId() == L"g499.98");
    expectTrue("End scrolls to the bottom", tree.scrollOffset() == tree.maxScrollOffset());
}

void testLazyBranchesShowLoadingAndKeepState() {
    oneui::TreeView tree;
    tree.setInternalScrolling(true);
    tree.setRetainExpansion(true);
    tree.setFrame(oneui::Rect{0.0f, 0.0f, 300.0f, 200.0f});
    std::vector<std::pair<std::wstring, bool>> expansions;
    tree.setOnExpansionChanged([&](const std::wstring& id, bool expanded) { expansions.push_back({id, expanded}); });
    oneui::TreeItem db{L"db", L"", L"shop", L"", false};
    db.hasChildren = true;
    oneui::TreeItem other{L"other", L"", L"other", L"", false};
    tree.setItems({db, other});
    tree.setSelectedId(L"db");
    expectEqual("lazy branch starts collapsed", tree.visibleItemCount(), 2);
    oneui::KeyEvent right;
    right.key = oneui::Key::Right;
    tree.onKeyDown(right);
    expectTrue("expanding a lazy branch reports it", expansions.size() == 1 && expansions[0].first == L"db" && expansions[0].second);
    expectEqual("no placeholder until loading", tree.visibleItemCount(), 2);
    db.loading = true;
    tree.setItems({db, other});
    expectEqual("loading shows a placeholder row", tree.visibleItemCount(), 3);
    RecordingCanvas canvas;
    tree.paint(canvas);
    bool placeholder = false;
    for (const auto& text : canvas.texts) placeholder = placeholder || text == L"Loading...";
    expectTrue("placeholder text is painted", placeholder);
    oneui::KeyEvent down;
    down.key = oneui::Key::Down;
    tree.onKeyDown(down);
    expectTrue("keyboard skips the placeholder", tree.selectedId() == L"other");
    tree.setSelectedId(L"db");
    // Children arrive while the supplied expanded flag is still false: the
    // user's expansion and selection survive the refresh.
    db.loading = false;
    tree.setItems({db, {L"db.users", L"db", L"users", L"", false}, {L"db.orders", L"db", L"orders", L"", false}, other});
    expectEqual("children replace the placeholder", tree.visibleItemCount(), 4);
    expectTrue("selection survives the refresh", tree.selectedId() == L"db");
    // A changed supplied value still wins.
    db.expanded = true;
    tree.setItems({db, {L"db.users", L"db", L"users", L"", false}, other});
    db.expanded = false;
    tree.setItems({db, {L"db.users", L"db", L"users", L"", false}, other});
    expectEqual("collapsing from data wins", tree.visibleItemCount(), 2);
}

void testActivationContextAndDecorations() {
    oneui::TreeView tree;
    tree.setInternalScrolling(true);
    tree.setFrame(oneui::Rect{0.0f, 0.0f, 300.0f, 200.0f});
    oneui::TreeItem table{L"t", L"", L"customers", L"12 MB", false};
    table.hasChildren = true;
    table.icon = oneui::IconSymbol::Terminal;
    table.badge = L"view";
    table.classes = "muted strong";
    table.highlightStart = 2;
    table.highlightLength = 3;
    tree.setItems({table, {L"u", L"", L"users", L"", false}});
    std::vector<std::wstring> activated, menus;
    oneui::Point menuAt{};
    tree.setOnActivated([&](const std::wstring& id) { activated.push_back(id); });
    tree.setOnContextMenu([&](const std::wstring& id, oneui::Point at) { menus.push_back(id); menuAt = at; });
    oneui::MouseEvent twice;
    twice.position = {150.0f, 16.0f};
    twice.clickCount = 2;
    tree.onMouseDown(twice);
    tree.onMouseUp(twice);
    expectTrue("double-click activates instead of toggling", activated.size() == 1 && activated[0] == L"t" && !tree.isExpanded(L"t"));
    oneui::KeyEvent enter;
    enter.key = oneui::Key::Enter;
    tree.onKeyDown(enter);
    expectEqual("Enter activates", activated.size(), 2);
    oneui::KeyEvent space;
    space.key = oneui::Key::Space;
    tree.onKeyDown(space);
    expectTrue("Space still toggles", tree.isExpanded(L"t"));
    oneui::MouseEvent right;
    right.position = {150.0f, 48.0f};
    right.button = oneui::MouseButton::Right;
    tree.onMouseDown(right);
    expectTrue("right click selects and reports", menus.size() == 1 && menus[0] == L"u" && tree.selectedId() == L"u");
    expectTrue("right click reports the pointer", menuAt.x == 150.0f && menuAt.y == 48.0f);
    oneui::KeyEvent apps;
    apps.key = oneui::Key::Other;
    apps.virtualKey = 0x5d;
    tree.onKeyDown(apps);
    expectTrue("menu key reports below the row", menus.size() == 2 && menuAt.y == 64.0f);
    RecordingCanvas canvas;
    tree.paint(canvas);
    bool badge = false, detail = false;
    for (const auto& text : canvas.texts) {
        badge = badge || text == L"view";
        detail = detail || text == L"12 MB";
    }
    expectTrue("badge and detail are painted", badge && detail);
}

int main() {
    testInternalScrollingPaintsOnlyVisibleRows();
    testLazyBranchesShowLoadingAndKeepState();
    testActivationContextAndDecorations();
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
