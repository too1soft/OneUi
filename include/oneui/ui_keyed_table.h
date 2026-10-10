#pragma once
#include "oneui/controls/table.h"
#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
namespace oneui::ui {
struct TableRow {
    std::wstring id;
    std::vector<std::wstring> cells;
    bool operator==(const TableRow& r) const { return id == r.id && cells == r.cells; }
};

class KeyedTable {
    std::shared_ptr<Table> table_;
    std::vector<TableRow> rows_;
    bool updating_ = false;
public:
    std::function<void(std::wstring)> selected;
    std::function<std::wstring()> selectionValue;
    explicit KeyedTable(std::shared_ptr<Table> table) : table_(std::move(table)) {
        table_->setOnChanged([this](int i) { if (!updating_ && selected) selected(i >= 0 && i < int(rows_.size()) ? rows_[i].id : L""); });
    }
    ~KeyedTable() { table_->setOnChanged({}); }
    void select(const std::wstring& id) {
        const auto found = std::find_if(rows_.begin(), rows_.end(), [&](auto& row) { return row.id == id; });
        updating_ = true; table_->setSelectedIndex(found == rows_.end() ? -1 : int(found - rows_.begin())); updating_ = false;
    }
    void updateRow(size_t index, const TableRow& row) {
        if (index >= rows_.size() || rows_[index].id != row.id) throw std::invalid_argument("Row identity");
        if (rows_[index].cells == row.cells) return;
        table_->updateRow(index,row.cells); rows_[index]=row;
    }
    void insert(size_t index, const std::vector<TableRow>& rows) {
        if (index > rows_.size()) throw std::invalid_argument("Insert index");
        std::vector<std::vector<std::wstring>> cells;
        for (auto& row : rows) cells.push_back(row.cells);
        updating_=true;
        try { table_->insertRows(index,std::move(cells)); rows_.insert(rows_.begin()+index,rows.begin(),rows.end()); }
        catch (...) { updating_=false; throw; }
        updating_=false;
    }
    void erase(size_t index, size_t count) {
        if (index>rows_.size() || count>rows_.size()-index) throw std::invalid_argument("Erase range");
        updating_=true;
        try { table_->eraseRows(index,count); rows_.erase(rows_.begin()+index,rows_.begin()+index+count); }
        catch (...) { updating_=false; throw; }
        updating_=false;
    }
    void move(size_t index, size_t count, size_t to) {
        if (index>rows_.size() || count>rows_.size()-index || to>rows_.size()-count) throw std::invalid_argument("Move range");
        updating_=true;
        try { table_->moveRows(index,count,to); } catch (...) { updating_=false; throw; }
        if (to < index) std::rotate(rows_.begin()+to,rows_.begin()+index,rows_.begin()+index+count);
        else std::rotate(rows_.begin()+index,rows_.begin()+index+count,rows_.begin()+to+count);
        updating_=false;
    }
    void update(const std::vector<TableRow>& rows) {
        std::set<std::wstring> keys;
        for (auto& r : rows) if (r.id.empty() || !keys.insert(r.id).second) throw std::invalid_argument("DataTable requires unique nonempty row IDs");
        if (rows == rows_) return;
        const auto offset = table_->scrollOffset();
        std::vector<std::wstring> selectedKeys;
        for (int i : table_->selectedIndices()) if (i >= 0 && i < int(rows_.size())) selectedKeys.push_back(rows_[i].id);
        bool sameKeys = rows.size() == rows_.size();
        if (sameKeys) for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i].id != rows_[i].id) { sameKeys = false; break; }
        updating_ = true;
        if (sameKeys) {
            for (std::size_t i = 0; i < rows.size(); ++i) if (rows[i].cells != rows_[i].cells) table_->updateRow(i, rows[i].cells);
        } else {
            std::vector<std::vector<std::wstring>> cells; cells.reserve(rows.size());
            for (auto& r : rows) cells.push_back(r.cells);
            table_->setRows(std::move(cells));
        }
        rows_ = rows;
        if (!sameKeys) {
            std::vector<int> indices;
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (std::find(selectedKeys.begin(), selectedKeys.end(), rows[i].id) != selectedKeys.end()) indices.push_back(int(i));
            table_->setSelectedIndices(std::move(indices)); table_->setScrollOffset(offset);
        }
        if (selectionValue) { select(selectionValue()); table_->setScrollOffset(offset); }
        updating_ = false;
        if (selected) { const int i = table_->selectedIndex(); selected(i >= 0 && i < int(rows.size()) ? rows[i].id : L""); }
    }
};

}
