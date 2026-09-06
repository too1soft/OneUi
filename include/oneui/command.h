#pragma once
#include "oneui/export.h"
#include "oneui/subscription.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace oneui {
struct KeyEvent;
class Widget;
enum class CommandResult : unsigned int { NotFound = 0, Disabled = 1, Executed = 2, Enabled = 3 };
enum KeyModifier : unsigned int {
    KeyModifierShift = 1, KeyModifierControl = 2, KeyModifierAlt = 4,
    KeyModifierMeta = 8, KeyModifierPrimary = 16
};
struct KeyChord {
    std::string key;
    unsigned int modifiers = 0;
};

class ONEUI_API CommandScope {
public:
    CommandScope();
    ~CommandScope();
    CommandScope(const CommandScope&) = delete;
    CommandScope& operator=(const CommandScope&) = delete;
    // Invalid/duplicate registrations throw invalid_argument. An empty predicate
    // means enabled. Dropping the returned subscription unregisters immediately.
    Subscription registerCommand(std::string id, std::function<void()> execute,
                                 std::function<bool()> enabled = {}, std::optional<KeyChord> shortcut = {});
    CommandResult query(const std::string& id) const;
    CommandResult execute(const std::string& id) const;
    CommandResult dispatchKey(const KeyEvent& event, const std::string& logicalKey = {}) const;
private:
    struct Data;
    std::shared_ptr<Data> data_;
};

// Uses the active focus path. Modal boundaries prevent reaching background or
// window commands, including when no command exists in the modal subtree.
ONEUI_API CommandResult queryCommand(const std::shared_ptr<Widget>& root, CommandScope* window, const std::string& id);
ONEUI_API CommandResult executeCommand(const std::shared_ptr<Widget>& root, CommandScope* window, const std::string& id);
ONEUI_API bool dispatchCommandKey(const std::shared_ptr<Widget>& root, CommandScope* window,
                                 const KeyEvent& event, const std::string& logicalKey = {});
ONEUI_API std::string logicalKeyName(const KeyEvent& event);
} // namespace oneui
