#include <memory>
#include <oneui/controls/virtual_list.h>

std::shared_ptr<oneui::VirtualList> example_virtual_list() {
    auto list = std::make_shared<oneui::VirtualList>();
    std::vector<oneui::ListItem> items;
    std::vector<std::wstring> ids;
    for (int i = 0; i < 10000; ++i) {
        ids.push_back(std::to_wstring(i));
        items.push_back({L"节点 " + ids.back(), L"本地模拟数据"});
    }
    list->setItems(std::move(items));
    list->setItemDragIds(std::move(ids));
    list->setSelectionMode(oneui::SelectionMode::Multiple);
    list->setRowHeight(44);
    // Data rows, not ten thousand child widgets; paints only the viewport.
    return list;
}
