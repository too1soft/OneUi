#include "requirements.h"
#include <iomanip>
#include <set>
#include <sstream>

namespace oneview {
namespace {
const std::map<std::string, std::vector<std::string>> implicitComponents{
    {"Workspace", {"Column", "Row"}},
    {"DockPanel", {"Column"}},
    {"ListPage", {"Column", "Text"}},
    {"DetailPage", {"Column", "Text", "Content", "Scroll"}},
    {"SettingsPage", {"Column", "Text", "Content", "Scroll"}},
    {"Content", {"Row", "Column"}},
    {"FormRow", {"Column", "Text", "ValidationMessage"}},
    {"FormGrid", {"FormRow"}},
    {"Page", {"Text"}}, {"Header", {"Text"}}, {"Section", {"Text"}},
    {"EmptyState", {"Text"}}, {"LoadingState", {"Text"}},
    {"FontPicker", {"HeaderSlot"}},
    {"MediaPicker", {"HeaderSlot"}}, {"MediaPreview", {"HeaderSlot"}}
};
void strings(std::ostream& out, const std::set<std::string>& values) {
    bool first = true;
    out << '[';
    for (const auto& value : values) {
        if (!first) out << ',';
        first = false;
        out << std::quoted(value);
    }
    out << ']';
}
}
std::string emitRequirements(const Image& image) {
    std::set<std::string> roots, closure, available;
    for (const auto& [name, contract] : registry()) available.insert(name);
    bool external = false;
    for (const auto& component : image.components) {
        roots.insert(component.name);
        external |= !available.count(component.name);
    }
    // Mount always creates its initial stack, including for leaf components.
    closure = roots;
    closure.insert("Column");
    bool changed = true;
    while (changed) {
        changed = false;
        auto previous = closure;
        for (const auto& name : previous) {
            const auto found = implicitComponents.find(name);
            if (found != implicitComponents.end())
                for (const auto& dependency : found->second)
                    changed |= closure.insert(dependency).second;
        }
    }
    std::ostringstream out;
    out << "{\"format_version\":1,\"full_runtime\":" << (external ? "true" : "false")
        << ",\"roots\":";
    strings(out, roots);
    out << ",\"components\":";
    strings(out, closure);
    out << ",\"available_components\":";
    strings(out, available);
    out << "}\n";
    return out.str();
}
}
