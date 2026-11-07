#pragma once
#include "runtime.h"
#include <unordered_set>
namespace oneui::views {
struct Instance::RichTableCache {
    struct Row {
        std::shared_ptr<Scope> scope;
        std::shared_ptr<const std::vector<std::shared_ptr<Scope>>> cells;
        std::shared_ptr<const std::vector<TableCell>> value;
        bool selected=false;
        bool visible=true;
        uint64_t cellsVersion=0;
        uint64_t selectedVersion=0;
        int nextIndex=-1;
    };
    const ListState* source=nullptr;
    uint64_t serial=0;
    std::vector<std::string> keys;
    std::unordered_map<std::string,Row> rows;
    struct Owner { std::string key; bool cell=false; };
    std::unordered_map<const Scope*,Owner> owners;
    std::vector<int> selected;
    bool filterActive=false;
    std::unordered_set<std::string> filterKeys;
};
}
