#include "oneui/command.h"
#include "oneui/widget.h"
#include "internal/unicode.h"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>

namespace oneui {
namespace {
std::string canonical(std::string value) {
    for (auto& c : value) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    if (value == " ") value = "space";
    return value;
}
unsigned int modifiers(unsigned int flags) {
    if (flags & KeyModifierPrimary) {
        flags &= ~KeyModifierPrimary;
#ifdef __APPLE__
        flags |= KeyModifierMeta;
#else
        flags |= KeyModifierControl;
#endif
    }
    return flags;
}
struct FocusPath {
    std::vector<std::shared_ptr<Widget>> widgets;
    bool modal = false;
};
FocusPath focusPath(std::shared_ptr<Widget> node) {
    FocusPath path;
    std::vector<Widget*> seen;
    while (node && node->visible() && !node->disabled()) {
        if (std::find(seen.begin(), seen.end(), node.get()) != seen.end()) break;
        seen.push_back(node.get());
        if (node->isCommandBoundary()) { path.modal = true; path.widgets.clear(); }
        else path.widgets.push_back(node);
        node = node->activeFocusChild();
    }
    return path;
}
}

struct CommandScope::Data {
    struct Entry {
        std::function<void()> execute;
        std::function<bool()> enabled;
        std::optional<KeyChord> shortcut;
        bool active = true;
    };
    bool alive = true;
    std::map<std::string, std::shared_ptr<Entry>> entries;
};
CommandScope::CommandScope() : data_(std::make_shared<Data>()) {}
CommandScope::~CommandScope() {
    data_->alive = false;
    for (const auto& item : data_->entries) item.second->active = false;
}
Subscription CommandScope::registerCommand(std::string id, std::function<void()> execute,
                                          std::function<bool()> enabled, std::optional<KeyChord> shortcut) {
    if (id.empty() || id.find('\0') != std::string::npos || unicode::toUtf8(unicode::fromUtf8(id)) != id ||
        !execute || data_->entries.count(id))
        throw std::invalid_argument("Invalid or duplicate OneUI command registration");
    if (shortcut) {
        shortcut->key = canonical(shortcut->key);
        shortcut->modifiers = modifiers(shortcut->modifiers);
        if (shortcut->key.empty() || (shortcut->modifiers & ~15u)) throw std::invalid_argument("Invalid OneUI shortcut");
        for (const auto& item : data_->entries) {
            const auto& key = item.second->shortcut;
            if (key && key->key == shortcut->key && key->modifiers == shortcut->modifiers)
                throw std::invalid_argument("Duplicate OneUI shortcut in command scope");
        }
    }
    auto entry = std::make_shared<Data::Entry>(Data::Entry{std::move(execute), std::move(enabled), std::move(shortcut), true});
    data_->entries.emplace(id, entry);
    return Subscription([weak = std::weak_ptr<Data>(data_), id = std::move(id)] {
        if (auto data = weak.lock()) {
            const auto it = data->entries.find(id);
            if (it != data->entries.end()) { it->second->active = false; data->entries.erase(it); }
        }
    });
}
CommandResult CommandScope::query(const std::string& id) const {
    const auto data = data_;
    const auto found = data->entries.find(id);
    if (found == data->entries.end()) return CommandResult::NotFound;
    const auto entry = found->second;
    const bool enabled = !entry->enabled || entry->enabled();
    return data->alive && entry->active && enabled ? CommandResult::Enabled : CommandResult::Disabled;
}
CommandResult CommandScope::execute(const std::string& id) const {
    const auto data = data_;
    const auto found = data->entries.find(id);
    if (found == data->entries.end()) return CommandResult::NotFound;
    const auto entry = found->second;
    const bool enabled = !entry->enabled || entry->enabled();
    if (!data->alive || !entry->active || !enabled) return CommandResult::Disabled;
    entry->execute();
    return CommandResult::Executed;
}
std::string logicalKeyName(const KeyEvent& event) {
    switch (event.key) {
    case Key::A: return "a"; case Key::C: return "c"; case Key::V: return "v"; case Key::X: return "x";
    case Key::Tab: return "tab"; case Key::Enter: return "enter"; case Key::Escape: return "escape";
    case Key::Space: return "space"; case Key::Backspace: return "backspace"; case Key::Delete: return "delete";
    case Key::Left: return "left"; case Key::Right: return "right"; case Key::Up: return "up"; case Key::Down: return "down";
    case Key::Home: return "home"; case Key::End: return "end"; case Key::PageUp: return "pageup";
    case Key::PageDown: return "pagedown"; case Key::F2: return "f2"; default: break;
    }
    if (event.virtualKey >= 'A' && event.virtualKey <= 'Z') return std::string(1, static_cast<char>(event.virtualKey + 32));
    // Backends normalize virtual keys to Windows-compatible codes. 0x70 is
    // F1, not lowercase 'p'; physical/raw virtual keys remain unchanged.
    if (event.virtualKey >= 0x70 && event.virtualKey <= 0x87) return "f" + std::to_string(event.virtualKey - 0x6f);
    if (event.virtualKey >= 0x60 && event.virtualKey <= 0x69) return "num" + std::to_string(event.virtualKey - 0x60);
    if (event.virtualKey >= '0' && event.virtualKey <= '9')
        return std::string(1, static_cast<char>(event.virtualKey));
    return {};
}
CommandResult CommandScope::dispatchKey(const KeyEvent& event, const std::string& logicalKey) const {
    if (!event.pressed) return CommandResult::NotFound;
    const auto key = canonical(logicalKey.empty() ? logicalKeyName(event) : logicalKey);
    const unsigned flags = (event.shift ? KeyModifierShift : 0u) | (event.control ? KeyModifierControl : 0u) |
        (event.alt ? KeyModifierAlt : 0u) | (event.win ? KeyModifierMeta : 0u);
    // Retain the data across predicates that destroy their owning scope.
    const auto data = data_;
    for (const auto& item : data->entries) {
        const auto shortcut = item.second->shortcut;
        if (shortcut && shortcut->key == key && shortcut->modifiers == flags) {
            const auto id = item.first;
            return execute(id);
        }
    }
    return CommandResult::NotFound;
}
CommandResult queryCommand(const std::shared_ptr<Widget>& root, CommandScope* window, const std::string& id) {
    auto path = focusPath(root);
    for (auto it = path.widgets.rbegin(); it != path.widgets.rend(); ++it) {
        auto result = (*it)->commands().query(id);
        if (result == CommandResult::NotFound) result = (*it)->queryBuiltinCommand(id);
        if (result != CommandResult::NotFound) return result;
    }
    return !path.modal && window ? window->query(id) : CommandResult::NotFound;
}
CommandResult executeCommand(const std::shared_ptr<Widget>& root, CommandScope* window, const std::string& id) {
    auto path = focusPath(root);
    for (auto it = path.widgets.rbegin(); it != path.widgets.rend(); ++it) {
        auto result = (*it)->commands().execute(id);
        if (result == CommandResult::NotFound) result = (*it)->executeBuiltinCommand(id);
        if (result != CommandResult::NotFound) return result;
    }
    return !path.modal && window ? window->execute(id) : CommandResult::NotFound;
}
bool dispatchCommandKey(const std::shared_ptr<Widget>& root, CommandScope* window, const KeyEvent& event, const std::string& logicalKey) {
    if (!event.pressed) return false;
    auto path = focusPath(root);
    if (!path.widgets.empty() && path.widgets.back()->hasTextComposition()) return false;
    for (auto it = path.widgets.rbegin(); it != path.widgets.rend(); ++it) {
        auto result = (*it)->commands().dispatchKey(event, logicalKey);
        if (result == CommandResult::NotFound) result = (*it)->dispatchBuiltinCommandKey(event, logicalKey);
        if (result != CommandResult::NotFound) return true;
    }
    return !path.modal && window && window->dispatchKey(event, logicalKey) != CommandResult::NotFound;
}
} // namespace oneui
