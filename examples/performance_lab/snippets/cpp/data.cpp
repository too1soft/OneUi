#include <memory>
#include <oneui/controls/table.h>
#include <oneui/controls/tree_view.h>

std::shared_ptr<oneui::Table> example_table() {
    auto table = std::make_shared<oneui::Table>();
    table->setColumns({{L"名称", 0}, {L"状态", 100}});
    table->setRows({{L"节点 A", L"在线"}, {L"节点 B", L"离线"}});
    table->setOnActivated([](int index) { (void)index; });
    return table;
}
std::shared_ptr<oneui::TreeView> example_tree() {
    auto tree = std::make_shared<oneui::TreeView>();
    tree->setItems({{L"root", L"", L"资源", L"", true}, {L"child", L"root", L"节点", L"", true}});
    return tree;
}
