#include "platform/shared/accessibility.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/progress_bar.h"
#include <cmath>
#include <unordered_set>

namespace oneui::platform {
std::vector<AccessibleNode> AccessibilityContext::snapshot() {
    std::vector<AccessibleNode> result;
    if (!valid() || !exposed()) return result;
    for (auto i = ids_.begin(); i != ids_.end();) {
        if (i->first.expired()) i = ids_.erase(i); else ++i;
    }
    const auto content = root();
    std::shared_ptr<Widget> focused;
    std::unordered_set<Widget*> focusPath;
    for (auto w = content; w && focusPath.insert(w.get()).second; w = w->activeFocusChild()) {
        if (!w->visible() || w->disabled()) break;
        if (w->focused()) focused = w;
    }
    std::unordered_set<Widget*> visited;
    std::function<void(const std::shared_ptr<Widget>&, std::uint64_t, bool, unsigned)> visit;
    visit = [&](const std::shared_ptr<Widget>& w, std::uint64_t parent, bool disabled, unsigned depth) {
        if (!w || !w->visible() || depth > 256 || !visited.insert(w.get()).second) return;
        if (auto popup = std::dynamic_pointer_cast<Popup>(w); popup && !popup->isOpen()) return;
        auto info = w->accessibilityInfo();
        disabled |= w->disabled() || info.state.disabled;
        info.state.disabled = disabled;
        info.state.focused = w == focused;
        // Routing containers forward their focused child's text-input state;
        // that does not turn a named parent group into a password control.
        const bool sensitive = (w->textInputState().sensitive && !w->activeFocusChild()) ||
            (std::dynamic_pointer_cast<TextField>(w) && std::static_pointer_cast<TextField>(w)->passwordMode());
        if (sensitive) info.value.clear();
        const auto frame = w->frame();
        if ((info.role != AccessibilityRole::None || !info.name.empty()) && frame.width > 0 && frame.height > 0) {
            auto [entry, inserted] = ids_.try_emplace(std::weak_ptr<Widget>(w), next_);
            if (inserted) ++next_;
            result.push_back({entry->second, parent, w, std::move(info), frame, w->isFocusable(), sensitive});
            auto& node = result.back();
            if (const auto field = std::dynamic_pointer_cast<TextField>(w); field && !sensitive) {
                node.text = AccessibleText{field->text(), field->caretIndex(), field->selectionStart(), field->selectionEnd(), field->multiline()};
            }
            if (const auto slider = std::dynamic_pointer_cast<Slider>(w)) {
                node.number = AccessibleNumber{std::isfinite(slider->value()) ? std::clamp(slider->value(), slider->minimum(), slider->maximum()) : slider->minimum(),
                    slider->minimum(), slider->maximum(), slider->step(), !disabled && !node.info.state.readOnly};
            } else if (const auto progress = std::dynamic_pointer_cast<ProgressBar>(w); progress && !progress->indeterminate()) {
                node.number = AccessibleNumber{std::isfinite(progress->value()) ? std::clamp(progress->value(), 0.0, 1.0) : 0.0, 0, 1, 0, false};
                node.info.state.readOnly = true;
            }
            parent = entry->second;
        }
        if (auto host = std::dynamic_pointer_cast<OverlayHost>(w)) {
            const OverlayEntry* modal = nullptr;
            for (const auto& entry : host->overlays())
                if (entry.child && entry.child->visible() && entry.trapsFocus && (!modal || entry.layer >= modal->layer)) modal = &entry;
            if (modal) { visit(modal->child, parent, disabled, depth + 1); return; }
            visit(host->content(), parent, disabled, depth + 1);
            for (const auto& entry : host->overlays()) visit(entry.child, parent, disabled, depth + 1);
            return;
        }
        if (auto view = std::dynamic_pointer_cast<View>(w))
            for (const auto& child : view->children()) visit(child, parent, disabled, depth + 1);
        if (auto popup = std::dynamic_pointer_cast<Popup>(w)) visit(popup->content(), parent, disabled, depth + 1);
    };
    visit(content, 0, false, 0);
    return valid() ? result : std::vector<AccessibleNode>{};
}
std::optional<AccessibleNode> AccessibilityContext::resolve(std::uint64_t id) {
    for (auto& node : snapshot()) if (node.id == id) return node;
    return std::nullopt;
}
bool AccessibilityContext::pressable(AccessibilityRole role) {
    return role == AccessibilityRole::Button || role == AccessibilityRole::CheckBox ||
        role == AccessibilityRole::RadioButton || role == AccessibilityRole::ComboBox;
}
bool AccessibilityContext::focusNode(std::uint64_t id) {
    auto node = resolve(id);
    if (!node || node->info.state.disabled || !node->focusable) return false;
    auto widget = node->widget.lock();
    return widget && focus(widget.get());
}
bool AccessibilityContext::press(std::uint64_t id) {
    auto node = resolve(id);
    if (!node || node->info.state.disabled || !pressable(node->info.role) || !focusNode(id)) return false;
    node = resolve(id); // Focus callbacks may remove/disable the target or close the window.
    if (!node || node->info.state.disabled || !pressable(node->info.role)) return false;
    auto widget = node->widget.lock();
    if (!widget) return false;
    KeyEvent event{node->info.role == AccessibilityRole::ComboBox ? Key::Enter : Key::Space};
    return widget->onKeyDown(event);
}
bool AccessibilityContext::setValue(std::uint64_t id, const std::wstring& value) {
    auto node = resolve(id);
    if (!node || node->info.role != AccessibilityRole::TextBox || node->info.state.disabled ||
        node->info.state.readOnly || value.size() > 1024 * 1024) return false;
    auto target = node->widget.lock();
    if (!target || target->hasTextComposition() || !focusNode(id)) return false;
    node = resolve(id);
    if (!node || node->info.role != AccessibilityRole::TextBox || node->info.state.disabled || node->info.state.readOnly) return false;
    auto widget = node->widget.lock();
    if (!widget || !widget->textInputState().editable || widget->hasTextComposition()) return false;
    const auto field = std::dynamic_pointer_cast<TextField>(widget);
    return widget->replaceTextRange(0, field ? field->text().size() : widget->textInputState().text.size(), value);
}
bool AccessibilityContext::selectText(std::uint64_t id, std::size_t start, std::size_t end) {
    auto node = resolve(id);
    if (!node || !node->text || node->info.state.disabled || start > node->text->value.size() ||
        end > node->text->value.size()) return false;
    const auto expected = node->text->value;
    auto target = node->widget.lock();
    if (!target || target->hasTextComposition() || !focusNode(id)) return false;
    node = resolve(id);
    if (!node || !node->text || node->info.state.disabled || node->text->value != expected) return false;
    auto field = std::dynamic_pointer_cast<TextField>(node->widget.lock());
    if (!field || field->hasTextComposition()) return false;
    field->setSelectionRange(start, end);
    return true;
}
bool AccessibilityContext::replaceText(std::uint64_t id, std::size_t start, std::size_t end, const std::wstring& value) {
    auto node = resolve(id);
    if (!node || !node->text || node->info.state.disabled || node->info.state.readOnly || start > end ||
        end > node->text->value.size() || value.size() > 1024 * 1024) return false;
    const auto expected = node->text->value;
    auto target = node->widget.lock();
    if (!target || target->hasTextComposition() || !focusNode(id)) return false;
    node = resolve(id);
    if (!node || !node->text || node->info.state.disabled || node->info.state.readOnly || node->text->value != expected) return false;
    auto field = std::dynamic_pointer_cast<TextField>(node->widget.lock());
    return field && !field->hasTextComposition() && field->replaceTextRange(start, end, value);
}
bool AccessibilityContext::setNumber(std::uint64_t id, double value) {
    auto node = resolve(id);
    if (!std::isfinite(value) || !node || !node->number || !node->number->writable ||
        value < node->number->minimum || value > node->number->maximum || !focusNode(id)) return false;
    node = resolve(id);
    if (!node || !node->number || !node->number->writable || value < node->number->minimum || value > node->number->maximum) return false;
    auto slider = std::dynamic_pointer_cast<Slider>(node->widget.lock());
    if (!slider || slider->dragging()) return false;
    return slider->setValueFromInput(value);
}
bool AccessibilityContext::stepNumber(std::uint64_t id, bool increase) {
    auto node = resolve(id);
    if (!node || !node->number || !node->number->writable) return false;
    const auto n = *node->number;
    return setNumber(id, std::clamp(n.value + (increase ? n.step : -n.step), n.minimum, n.maximum));
}
} // namespace oneui::platform
