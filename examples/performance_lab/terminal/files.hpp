#pragma once
#include "models.hpp"
#include <map>

namespace lab::terminal {
class Files {
    struct Entry {
        std::wstring name;
        bool directory;
        std::wstring size;
    };
    FilesModel &model_;
    std::shared_ptr<oneui::Table> table_ = std::make_shared<oneui::Table>();
    std::map<std::wstring, std::vector<Entry>> folders_;
    std::vector<std::wstring> rows_, selected_;
    std::vector<oneui::Subscription> subscriptions_;
    bool updating_ = false;
    int serial_ = 0;
    std::vector<Entry> &folder();
    void filter();
    void open(int row);

  public:
    explicit Files(FilesModel &, oneui::ui::Mount &);
    ~Files();
    const auto &table() const { return table_; }
};
} // namespace lab::terminal
