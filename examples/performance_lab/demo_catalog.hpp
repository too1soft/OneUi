#pragma once
#include <string>
#include <vector>

namespace lab {
enum class Category { Layout, Performance, Features };
enum class PageKind {
    Layout,
    Recipes,
    Workspace,
    Dock,
    Performance,
    Controls,
    Text,
    Data,
    Drawing,
    Notifications,
    Menus,
    Windows,
    MultiWindow,
    Files,
    Terminal,
    Remote,
    Reactive,
    Theme,
    Diagnostics
};
struct PageDescriptor {
    const char *id;
    Category category;
    const wchar_t *title;
    const wchar_t *description;
    PageKind kind;
    int variant;
    const char *capabilities;
};
const std::vector<PageDescriptor> &pageCatalog();
const PageDescriptor *findPage(const std::string &id);
const wchar_t *categoryName(Category category);
} // namespace lab
