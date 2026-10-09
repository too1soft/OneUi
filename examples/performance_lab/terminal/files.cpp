#include "files.hpp"
#include <cwctype>

namespace lab::terminal {
namespace {
std::wstring lower(std::wstring text) {
    for (auto &c : text)
        c = std::towlower(c);
    return text;
}
} // namespace
std::vector<Files::Entry> &Files::folder() {
    auto [it, inserted] = folders_.try_emplace(model_.path.get());
    if (inserted) {
        it->second = {{L"config", true, L"—"},
                      {L"logs", true, L"—"},
                      {L"services", true, L"—"},
                      {L"scripts", true, L"—"},
                      {L"README.md", false, L"3.2 KB"},
                      {L"compose.yaml", false, L"1.8 KB"},
                      {L"gateway.toml", false, L"856 B"},
                      {L"deploy.sh", false, L"2.4 KB"}};
        for (int i = 1; i <= 10; ++i)
            it->second.push_back(
                {L"access-sample-" + std::to_wstring(i) + L".log", false, L"128 KB"});
    }
    return it->second;
}
void Files::filter() {
    std::vector<std::vector<oneui::TableCell>> data;
    rows_.clear();
    const auto query = lower(model_.fileQuery.get());
    for (const auto &entry : folder()) {
        if (!query.empty() && lower(entry.name).find(query) == std::wstring::npos)
            continue;
        oneui::TableCell name;
        name.text = entry.name;
        name.leadingIcon = entry.directory ? oneui::IconSymbol::OutlineWorkbenchFolder
                                           : oneui::IconSymbol::OutlineWorkbenchFile;
        name.iconSize = 16;
        oneui::TableCell size;
        size.text = entry.size;
        size.alignment = oneui::TextAlign::Right;
        oneui::TableCell modified;
        modified.text = L"示例时间";
        oneui::TableCell mode;
        mode.text = entry.directory ? L"drwxr-xr-x" : L"-rw-r--r--";
        data.push_back({name, size, modified, mode});
        rows_.push_back(entry.name);
    }
    const auto offset = table_->scrollOffset();
    updating_ = true;
    table_->setRichRows(std::move(data));
    std::vector<int> selection;
    std::vector<std::wstring> retained;
    for (int i = 0; i < int(rows_.size()); ++i)
        if (std::find(selected_.begin(), selected_.end(), rows_[i]) != selected_.end()) {
            selection.push_back(i);
            retained.push_back(rows_[i]);
        }
    selected_ = std::move(retained);
    table_->setSelectedIndices(std::move(selection));
    table_->setScrollOffset(offset);
    updating_ = false;
    model_.removeFile.canExecute.set(!selected_.empty());
    model_.download.canExecute.set(!selected_.empty());
    model_.fileSummary.set(std::to_wstring(rows_.size()) + L" / " +
                           std::to_wstring(folder().size()) + L" 个项目 · 仅内存模拟");
}
void Files::open(int row) {
    if (row < 0 || row >= int(rows_.size()))
        return;
    const auto name = rows_[row];
    const auto &entries = folder();
    auto entry =
        std::find_if(entries.begin(), entries.end(), [&](const auto &f) { return f.name == name; });
    if (entry == entries.end())
        return;
    if (entry->directory) {
        model_.path.set(model_.path.get() + L"/" + name);
        selected_.clear();
        model_.fileQuery.set(L"");
        filter();
    } else
        model_.transferLabel.set(L"预览 " + name + L" · 无磁盘读取");
}
Files::Files(FilesModel &model, oneui::ui::Mount &mount) : model_(model) {
    table_->setColumns(
        {{L"名称", 0}, {L"大小", 86, oneui::TextAlign::Right}, {L"修改时间", 100}, {L"权限", 104}});
    table_->setSelectionMode(oneui::SelectionMode::Multiple);
    table_->setSelectionColumnVisible(true);
    table_->setSelectionColumnWidth(30);
    table_->setRowHeight(31);
    table_->setHeaderHeight(30);
    table_->setColumnDividersVisible(false);
    table_->setAccessibleName(L"模拟文件列表");
    table_->setOnSelectionChanged([this](const auto &indices) {
        if (updating_)
            return;
        selected_.clear();
        for (int i : indices)
            if (i >= 0 && i < int(rows_.size()))
                selected_.push_back(rows_[i]);
        model_.removeFile.canExecute.set(!selected_.empty());
        model_.download.canExecute.set(!selected_.empty());
    });
    table_->setOnActivated([this](int row) { open(row); });
    mount.registerNative("files", table_);
    model_.up.setAction([this] {
        auto path = model_.path.get();
        if (path == L"/srv/workspace")
            return;
        path.resize(path.find_last_of(L'/'));
        model_.path.set(path);
        selected_.clear();
        model_.fileQuery.set(L"");
        filter();
    });
    model_.newFolder.setAction([this] {
        auto name = L"new-folder-" + std::to_wstring(++serial_);
        folder().insert(folder().begin(), {name, true, L"—"});
        selected_ = {name};
        model_.fileQuery.set(L"");
        filter();
        model_.transferLabel.set(L"已创建 " + name + L" · 未修改磁盘");
    });
    model_.removeFile.setAction([this] {
        auto &data = folder();
        data.erase(std::remove_if(data.begin(), data.end(),
                                  [this](const auto &entry) {
                                      return std::find(selected_.begin(), selected_.end(),
                                                       entry.name) != selected_.end();
                                  }),
                   data.end());
        selected_.clear();
        filter();
        model_.transferLabel.set(L"已删除选中的模拟文件 · 未修改磁盘");
    });
    model_.upload.setAction([this] {
        auto name = L"upload-sample-" + std::to_wstring(++serial_) + L".txt";
        folder().insert(folder().begin(), {name, false, L"2.0 KB"});
        selected_ = {name};
        model_.fileQuery.set(L"");
        filter();
        model_.transferLabel.set(L"模拟上传完成 · " + name);
    });
    model_.download.setAction([this] {
        if (selected_.empty())
            return;
        model_.transferValue.set(0);
        model_.transferLabel.set(L"模拟下载 " + std::to_wstring(selected_.size()) + L" 个项目");
        model_.transferring.set(true);
    });
    subscriptions_.push_back(model_.fileQuery.subscribeScoped([this](const auto &) { filter(); }));
    filter();
}
Files::~Files() {
    table_->setOnSelectionChanged({});
    table_->setOnActivated({});
}
} // namespace lab::terminal
