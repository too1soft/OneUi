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
        {"copy",IconSymbol::OutlineCopy},{"copy-native",IconSymbol::Copy},{"pause",IconSymbol::Pause},
        {"up",IconSymbol::OutlineWorkbenchUp},{"link",IconSymbol::OutlineWorkbenchLink},{"broadcast",IconSymbol::OutlineWorkbenchBroadcast},
        {"download",IconSymbol::OutlineWorkbenchDownload},{"settings",IconSymbol::OutlineWorkbenchSettings},
        {"key-dots",IconSymbol::KeyDots},{"check-circle",IconSymbol::CheckCircle},
        {"notebook",IconSymbol::Notebook},{"trash",IconSymbol::Trash},
        {"user",IconSymbol::User},{"refresh-circle",IconSymbol::Refresh},{"edit",IconSymbol::Edit},
        {"terminal-native",IconSymbol::Terminal},{"desktop",IconSymbol::Desktop},{"sparkle",IconSymbol::Sparkle},
        {"check",IconSymbol::Check},{"upload-native",IconSymbol::Upload},
        {"server-native",IconSymbol::Server},{"server-outline",IconSymbol::OutlineServer},
        {"terminal-outline",IconSymbol::OutlineTerminal},{"close-outline",IconSymbol::OutlineClose},
        {"more-outline",IconSymbol::OutlineMore},{"grid",IconSymbol::LayoutGrid},
        {"monitor",IconSymbol::Monitor},{"folder-outline",IconSymbol::OutlineFolder},{"sliders",IconSymbol::Sliders},
        {"left-outline",IconSymbol::OutlineLeft},{"right-outline",IconSymbol::OutlineChevron},
        {"down-outline",IconSymbol::OutlineDown},{"key-outline",IconSymbol::OutlineKey},
        {"tag-outline",IconSymbol::OutlineTag},{"pin-outline",IconSymbol::OutlinePin},
        {"search-outline",IconSymbol::OutlineSearch},{"inspector-outline",IconSymbol::OutlineInspector},
        {"info-circle",IconSymbol::InfoCircle},{"warning-triangle",IconSymbol::WarningTriangle},
        {"close-native",IconSymbol::Close},
        {"minimize",IconSymbol::Minimize},{"maximize-native",IconSymbol::Maximize},
        {"restore-native",IconSymbol::Restore},
        {"note-outline",IconSymbol::OutlineWorkbenchNote},{"box-outline",IconSymbol::OutlineWorkbenchBox},
        {"sparkles-outline",IconSymbol::OutlineWorkbenchSparkles},{"toolbox",IconSymbol::Toolbox},
        {"shield-outline",IconSymbol::OutlineShield},{"globe",IconSymbol::Globe},
        {"import-outline",IconSymbol::OutlineImport},{"sidebar-outline",IconSymbol::OutlineSidebar},
        {"play",IconSymbol::Play},
        {"search-native",IconSymbol::Search},{"code-native",IconSymbol::Code},
        {"remote-assist",IconSymbol::RemoteAssist},{"radio-on",IconSymbol::RadioOn},
        {"folder-native",IconSymbol::Folder},{"file-native",IconSymbol::File},
        {"download-native",IconSymbol::Download},{"chevron-down",IconSymbol::ChevronDown},
        {"plus-native",IconSymbol::Plus},{"chevron-up",IconSymbol::ChevronUp}
    };
    for(const auto& entry:entries)if(entry.name==name)return entry.symbol;
    return {};
}
}
