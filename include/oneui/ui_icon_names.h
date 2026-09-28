#pragma once
#include "oneui/icon.h"
#include <optional>
#include <string_view>
namespace oneui::ui {
inline std::optional<IconSymbol> namedIcon(std::string_view name) {
    struct Entry { std::string_view name; IconSymbol symbol; };
    static constexpr Entry entries[] = {
        {"down",IconSymbol::OutlineWorkbenchDown},{"left",IconSymbol::OutlineWorkbenchLeft},{"right",IconSymbol::OutlineWorkbenchRight},
        {"upload",IconSymbol::OutlineWorkbenchUpload},{"more",IconSymbol::OutlineWorkbenchMore},{"maximize",IconSymbol::OutlineWorkbenchMaximize},
        {"code",IconSymbol::OutlineWorkbenchCode},{"database",IconSymbol::OutlineWorkbenchDatabase},{"grip",IconSymbol::OutlineWorkbenchGrip},
        {"terminal",IconSymbol::OutlineWorkbenchTerminal},{"server",IconSymbol::OutlineWorkbenchServer},
        {"folder",IconSymbol::OutlineWorkbenchFolder},{"file",IconSymbol::OutlineWorkbenchFile},
        {"search",IconSymbol::OutlineWorkbenchSearch},{"plus",IconSymbol::OutlineWorkbenchPlus},
        {"close",IconSymbol::OutlineWorkbenchClose},{"split",IconSymbol::OutlineWorkbenchSplit},
        {"layout",IconSymbol::OutlineWorkbenchLayout},{"cpu",IconSymbol::OutlineWorkbenchCpu},
        {"refresh",IconSymbol::OutlineRefresh},{"sun",IconSymbol::OutlineSun},
        {"copy",IconSymbol::OutlineCopy},{"pause",IconSymbol::Pause},
        {"up",IconSymbol::OutlineWorkbenchUp},{"link",IconSymbol::OutlineWorkbenchLink},
        {"download",IconSymbol::OutlineWorkbenchDownload},{"settings",IconSymbol::OutlineWorkbenchSettings}
    };
    for(const auto& entry:entries)if(entry.name==name)return entry.symbol;
    return {};
}
}
