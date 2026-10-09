#include "oneui_c_api_dock_model_internal.h"

namespace oneui::capi {
namespace {
std::optional<DockNode> decodeNode(const OneUiDockSnapshot &source, int index, unsigned depth,
                                   size_t &count) {
    if (index == -1)
        return {};
    if (index < 0 || size_t(index) >= source.node_count || depth > 64 || ++count > 65536)
        throw std::invalid_argument("Invalid dock tree reference/depth");
    const auto &node = source.nodes[index];
    if (node.kind == 0 && node.pane)
        return DockNode(node.pane);
    if (node.kind != 1 || node.axis < 0 || node.axis > 1)
        throw std::invalid_argument("Invalid dock tree node");
    auto first = decodeNode(source, node.first, depth + 1, count),
         second = decodeNode(source, node.second, depth + 1, count);
    if (!first || !second)
        throw std::invalid_argument("Split requires two children");
    return DockNode(DockAxis(node.axis), node.ratio, std::move(*first), std::move(*second));
}
int encodeNode(const std::optional<DockNode> &root, std::vector<OneUiDockTreeNode> &nodes) {
    if (!root)
        return -1;
    const auto walk = [&](auto &&self, const DockNode &node) -> int {
        const auto index = int(nodes.size());
        nodes.push_back({node.pane ? 0 : 1, node.pane, int(node.axis), node.ratio, -1, -1});
        if (!node.pane) {
            auto first = self(self, *node.first), second = self(self, *node.second);
            nodes[index].first = first;
            nodes[index].second = second;
        }
        return index;
    };
    return walk(walk, *root);
}
DockFloating floating(const OneUiDockFloating &value) {
    return {dockRect(value.bounds), value.pinned != 0, value.order};
}
OneUiDockFloating floating(DockId id, const DockFloating &value) {
    return {id, dockRect(value.bounds), value.pinned, value.order};
}
} // namespace
DockSnapshot decodeDockSnapshot(const OneUiDockSnapshot &source) {
    if (source.node_count > 65536 || source.floating_count > 1024 || source.hidden_count > 1024 ||
        (source.node_count && !source.nodes) || (source.floating_count && !source.floating) ||
        (source.hidden_count && !source.hidden))
        throw std::invalid_argument("Invalid dock snapshot arrays");
    DockSnapshot result;
    size_t count = 0;
    result.root = decodeNode(source, source.root, 0, count);
    result.maximized = source.maximized;
    for (size_t i = 0; i < source.floating_count; ++i) {
        const auto &item = source.floating[i];
        if (!result.floating.emplace(item.pane, floating(item)).second)
            throw std::invalid_argument("Duplicate floating pane ID");
    }
    for (size_t i = 0; i < source.hidden_count; ++i) {
        const auto &item = source.hidden[i];
        if (item.edge < 0 || item.edge > 3)
            throw std::invalid_argument("Invalid dock edge");
        DockRestore old;
        old.before = decodeNode(source, item.before_root, 0, count);
        old.after = decodeNode(source, item.after_root, 0, count);
        old.target = item.target;
        old.edge = DockEdge(item.edge);
        old.ratio = item.ratio;
        if (item.has_floating)
            old.floating = floating(item.floating);
        if (!result.hidden.emplace(item.pane, std::move(old)).second)
            throw std::invalid_argument("Duplicate hidden pane ID");
    }
    return result;
}
void visitDockSnapshot(const DockSnapshot &source, OneUiDockSnapshotCallback callback, void *data) {
    if (!callback)
        throw std::invalid_argument("Snapshot callback is required");
    std::vector<OneUiDockTreeNode> nodes;
    std::vector<OneUiDockFloating> floats;
    std::vector<OneUiDockRestore> hidden;
    const int root = encodeNode(source.root, nodes);
    for (const auto &[id, value] : source.floating)
        floats.push_back(floating(id, value));
    for (const auto &[id, value] : source.hidden) {
        OneUiDockRestore old{};
        old.pane = id;
        old.before_root = encodeNode(value.before, nodes);
        old.after_root = encodeNode(value.after, nodes);
        old.target = value.target;
        old.edge = int(value.edge);
        old.ratio = value.ratio;
        old.has_floating = bool(value.floating);
        if (value.floating)
            old.floating = floating(id, *value.floating);
        hidden.push_back(old);
    }
    const OneUiDockSnapshot result{nodes.data(),  nodes.size(),  root,          floats.data(),
                                   floats.size(), hidden.data(), hidden.size(), source.maximized};
    callback(&result, data);
}
} // namespace oneui::capi
