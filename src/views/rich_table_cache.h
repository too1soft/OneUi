#pragma once
#include "runtime.h"
namespace oneui::views {
struct Instance::RichTableCache {
    struct Row {
        std::shared_ptr<Scope> scope;
        std::vector<std::shared_ptr<Scope>> cells;
        std::vector<TableCell> value;
        bool selected=false;
    };
    const ListState* source=nullptr;
    uint64_t serial=0;
    std::vector<std::string> keys;
    std::map<std::string,Row> rows;
    std::unordered_map<const Scope*,std::string> owners;
    std::vector<int> selected;
};
}
