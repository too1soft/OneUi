// Button `shortcut`: a key chord that clicks the button while focus is anywhere
// in the view. Registered on the view root's command scope, so focused
// controls that claim the chord themselves (an editor's Ctrl+F) keep it.
#include "runtime.h"
#include "oneui/command.h"
#include "oneui/controls/button.h"
#include <cctype>
#include <sstream>

namespace oneui::views {
namespace {

std::string lower(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// "Ctrl+Shift+Tab", "Primary+S", "F5". Modifiers before the key, one key.
KeyChord parseChord(const std::string& text) {
    KeyChord chord;
    std::stringstream parts(text);
    std::string part;
    std::vector<std::string> tokens;
    while (std::getline(parts, part, '+')) {
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.front()))) part.erase(part.begin());
        while (!part.empty() && std::isspace(static_cast<unsigned char>(part.back()))) part.pop_back();
        check(!part.empty(), ONEUI_VIEW_TYPE, "Empty shortcut part");
        tokens.push_back(lower(part));
    }
    check(!tokens.empty(), ONEUI_VIEW_TYPE, "Empty shortcut");
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        const auto& m = tokens[i];
        unsigned flag = 0;
        if (m == "ctrl" || m == "control") flag = KeyModifierControl;
        else if (m == "shift") flag = KeyModifierShift;
        else if (m == "alt") flag = KeyModifierAlt;
        else if (m == "meta" || m == "win" || m == "cmd") flag = KeyModifierMeta;
        else if (m == "primary" || m == "mod") flag = KeyModifierPrimary;
        check(flag != 0 && !(chord.modifiers & flag), ONEUI_VIEW_TYPE, "Unknown or repeated shortcut modifier");
        chord.modifiers |= flag;
    }
    chord.key = tokens.back();
    return chord;
}

} // namespace

bool Runtime::shortcutScalar(Instance& instance, const std::string& name, const Value& value) {
    if (name != "shortcut" || instance.element.component != "Button") return false;
    instance.shortcut.reset();
    instance.shortcutChord.reset();
    if (!value.text.empty()) {
        try {
            instance.shortcutChord = parseChord(value.text);
        } catch (const Failure&) {
            throw std::invalid_argument("Invalid Button shortcut");
        }
        if (root) registerShortcut(instance);
        else pendingShortcuts.push_back(&instance);
    }
    return true;
}

void Runtime::registerShortcut(Instance& instance) {
    if (!instance.shortcutChord || !root || !root->element.widget) return;
    auto* target = &instance;
    const auto enabled = [target] {
        for (auto* at = target; at; ) {
            const auto& widget = at->element.widget;
            if (!widget || !widget->visible() || widget->disabled()) return false;
            const auto parent = at->parent.lock();
            at = parent.get();
        }
        return true;
    };
    const auto execute = [target] {
        if (auto button = target->element.as<Button>()) button->activate();
    };
    // The instance owns the subscription; it unregisters before the instance dies.
    instance.shortcut = std::make_shared<Subscription>(root->element.widget->commands().registerCommand(
        "oneui.shortcut." + std::to_string(reinterpret_cast<std::uintptr_t>(target)), execute, enabled,
        *instance.shortcutChord));
}

void Runtime::registerPendingShortcuts() {
    auto pending = std::move(pendingShortcuts);
    pendingShortcuts.clear();
    for (auto* instance : pending) registerShortcut(*instance);
}

} // namespace oneui::views
