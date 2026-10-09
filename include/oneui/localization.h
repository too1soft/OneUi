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
};

// Application overrides are copied and synchronized. nullopt restores the
// platform default. Configure before opening dialogs or querying accessibility.
ONEUI_API bool setBuiltinText(BuiltinText key, std::optional<std::wstring> value);
ONEUI_API std::wstring builtinText(BuiltinText key);

} // namespace oneui
