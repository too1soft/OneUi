#pragma once

#include "oneui/export.h"
#include <optional>
#include <string>

namespace oneui {

enum class BuiltinText {
    MinimizeWindow,
    MaximizeWindow,
    RestoreWindow,
    CloseWindow,
    Accept,
    Cancel,
    ShowMainWindow,
    QuitApplication,
    Loading,
    // Code editor find bar. FindMatchCount uses {0} (current) and {1} (total).
    FindPlaceholder,
    ReplacePlaceholder,
    FindNoResults,
    FindInvalidPattern,
    FindMatchCount,
    ReplaceOne,
    ReplaceAll,
    FindMatchCase,
    FindRegex,
    FindWholeWord,
    FindPrevious,
    FindNext,
    FindClose,
};

// Application overrides are copied and synchronized. nullopt restores the
// platform default. Configure before opening dialogs or querying accessibility.
ONEUI_API bool setBuiltinText(BuiltinText key, std::optional<std::wstring> value);
ONEUI_API std::wstring builtinText(BuiltinText key);

} // namespace oneui
