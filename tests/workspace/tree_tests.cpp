#include "internal/split_geometry.h"
#include "oneui/layout/workspace_tree.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace oneui;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < .001f; }
void simple() {
    DockTreeIndex tree;
    std::vector<WorkspaceNode> nodes{
        {"a", "A"}, {"b", "B"}, {"s", "", "a", "b", DockAxis::Horizontal, .7f}};
    const std::vector<WorkspacePane> panes{{"A", {220, 130}}, {"B", {220, 130}}};
    check(tree.reset(nodes, {{"g", "s"}}, panes) == DockError::None, "valid tree");
    WorkspaceGeometry layout;
    check(tree.layout("g", {0, 0, 300, 100}, layout) == DockError::None, "layout");
    check(near(layout.content.width, 445) && near(layout.content.height, 130),
          "scrollable minimum");
    check(layout.panes.size() == 2 && layout.dividers.size() == 1, "flat geometry");
    check(near(layout.panes[0].bounds.width, 220), "minimum clamp");
    DockGeometry legacy;
    DockNode old(DockAxis::Horizontal, .7f, DockNode(1), DockNode(2));
    check(dockGeometry(
              old, {0, 0, 1000, 600}, 5, [](auto) { return Size{220, 130}; }, legacy) ==
              DockError::None,
          "legacy");
    tree.layout("g", {0, 0, 1000, 600}, layout);
    check(near(layout.panes[0].bounds.width, legacy.panes.at(1).width), "shared legacy geometry");
    tree.layout("g", {0, 0, 300, 100}, layout, "B");
    check(layout.panes.size() == 1 && layout.dividers.empty() &&
              near(layout.panes[0].bounds.width, 300),
          "maximize");
    check(near(tree.nodes()[2].ratio, .7f), "maximize preserves ratio");
    for (float ratio : {0.f, 1.f, std::numeric_limits<float>::quiet_NaN()}) {
        nodes[2].ratio = ratio;
        check(tree.reset(nodes, {{"g", "s"}}, panes) != DockError::None, "bad ratio rejected");
        check(near(tree.nodes()[2].ratio, .7f), "failed reset preserves index");
    }
    nodes[2].ratio = .5f;
    nodes[2].first = "s";
    check(tree.reset(nodes, {{"g", "s"}}, panes) != DockError::None, "cycle rejected");
    nodes[2].first = "missing";
    check(tree.reset(nodes, {{"g", "s"}}, panes) != DockError::None, "dangling rejected");
    nodes[2].first = "a";
    nodes[1].pane = "A";
    check(tree.reset(nodes, {{"g", "s"}}, panes) == DockError::DuplicateId, "duplicate pane");
    nodes[1].pane = "B";
    check(tree.reset(nodes, {{"g", "s"}, {"other", "a"}}, panes) != DockError::None,
          "multi parent across groups");
    check(tree.reset(nodes, {{"g", "a"}}, panes) != DockError::None, "unreachable nodes");
    check(tree.layout("g", {0, 0, 2, 2}, layout, "missing") != DockError::None, "invalid maximize");
    check(layout.panes.size() == 1 && layout.panes[0].key == "B", "failed output unchanged");
    check(near(detail::splitAmount(10, .8f, 10, 30), 8), "dock shortage ratio");
    check(near(detail::splitAmount(10, .8f, 10, 30, detail::SplitShortage::MinimumShare), 2.5f),
          "split shortage minimum");
    check(detail::nearestNormalizedEdge(.5f, .5f) == 0, "nearest tie L");
    auto hit = dockHitTest(9, {50, 50}, {{1, {0, 0, 100, 100}, false}}, DockHitMode::PaneSwap);
    check(hit && hit->placement == DockHitPlacement::Swap, "pane center swap");
    hit = dockHitTest(9, {28, 50}, {{1, {0, 0, 100, 100}, false}}, DockHitMode::PaneSwap);
    check(hit && hit->placement == DockHitPlacement::Left, "strict center edge");
}
void deep(size_t depth) {
    std::vector<WorkspaceNode> nodes{{"leaf0", "p0"}};
    std::vector<WorkspacePane> panes{{"p0", {1, 1}}};
    std::string root = "leaf0";
    for (size_t i = 1; i <= depth; ++i) {
        auto id = std::to_string(i), leaf = "leaf" + id, pane = "p" + id, split = "s" + id;
        nodes.push_back({leaf, pane});
        panes.push_back({pane, {1, 1}});
        nodes.push_back({split, "", root, leaf, DockAxis::Horizontal, .5f});
        root = split;
    }
    DockTreeIndex tree;
    check(tree.reset(nodes, {{"g", root}}, panes) == DockError::None, "deep validation");
    WorkspaceGeometry g;
    check(tree.layout("g", {0, 0, 800, 600}, g) == DockError::None, "iterative deep layout");
    check(g.panes.size() == depth + 1 && g.dividers.size() == depth, "deep geometry count");
    check(g.panes.front().key == "p0", "DFS order");
    // Moving list storage never changes logical identity/geometry.
    std::reverse(nodes.begin(), nodes.end());
    check(tree.reset(nodes, {{"g", root}}, panes) == DockError::None, "arbitrary node order");
    WorkspaceGeometry moved;
    tree.layout("g", {0, 0, 800, 600}, moved);
    check(near(g.panes.front().bounds.width, moved.panes.front().bounds.width),
          "storage order not topology");
}
} // namespace
int main() {
    try {
        simple();
        for (auto d : {1, 64, 65, 1000})
            deep(d);
        std::cout << "workspace tree: PASS\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
