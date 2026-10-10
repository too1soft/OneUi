#include "oneui/oneui_c_api.h"
#include "oneui_c_api_error.h"
#include "oneui_c_api_widget_internal.h"
#include "oneui_c_api_window_internal.h"
#include "oneui/controls/label.h"
#include "oneui/controls/text_field.h"
#include "../internal/unicode.h"
#include <stdexcept>
#include <cmath>

struct OneUiCommandRegistration { oneui::Subscription subscription; };
namespace {
std::string string(OneUiUtf8String value) {
    if (!value.data && value.length) throw std::invalid_argument("Null string buffer");
    std::string result(value.data ? value.data : "", value.length);
    if (result.find('\0') != std::string::npos || oneui::unicode::toUtf8(oneui::unicode::fromUtf8(result)) != result)
        throw std::invalid_argument("Invalid UTF-8 identifier");
    return result;
}
struct Callback {
    OneUiVoidCallback execute;
    OneUiCommandEnabledCallback enabled;
    void* data;
    OneUiDestroyCallback destroy;
    ~Callback() { if (destroy) destroy(data); }
};
OneUiCommandRegistration* registration(oneui::CommandScope* scope, OneUiUtf8String id,
    const OneUiKeyChordUtf8* shortcut, OneUiVoidCallback execute, OneUiCommandEnabledCallback enabled,
    void* data, OneUiDestroyCallback destroy) noexcept {
    // Establish ownership before validating any argument, including a null scope.
    std::unique_ptr<Callback> pending;
    try { pending = std::make_unique<Callback>(); }
    catch (...) { oneui::capi::recordCurrentException(__func__); if (destroy) destroy(data); return nullptr; }
    pending->execute = execute; pending->enabled = enabled; pending->data = data; pending->destroy = destroy;
    try {
        std::shared_ptr<Callback> callback(std::move(pending));
        if (!scope || !execute) throw std::invalid_argument("Command scope and execute callback are required");
        std::optional<oneui::KeyChord> key;
        if (shortcut) key = oneui::KeyChord{string(shortcut->key), shortcut->modifiers};
        auto result = std::make_unique<OneUiCommandRegistration>();
        result->subscription = scope->registerCommand(string(id), [callback] { callback->execute(callback->data); },
            [callback] { return !callback->enabled || callback->enabled(callback->data) != 0; }, std::move(key));
        return result.release();
    } catch (...) { oneui::capi::recordCurrentException(__func__); return nullptr; }
}
}
extern "C" {
OneUiCommandRegistration* oneui_widget_register_command_utf8(OneUiWidget* widget, OneUiUtf8String id,
    const OneUiKeyChordUtf8* shortcut, OneUiVoidCallback execute, OneUiCommandEnabledCallback enabled,
    void* data, OneUiDestroyCallback destroy) {
    return registration(widget && widget->widget ? &widget->widget->commands() : nullptr, id, shortcut, execute, enabled, data, destroy);
}
OneUiCommandRegistration* oneui_window_register_command_utf8(OneUiWindow* window, OneUiUtf8String id,
    const OneUiKeyChordUtf8* shortcut, OneUiVoidCallback execute, OneUiCommandEnabledCallback enabled,
    void* data, OneUiDestroyCallback destroy) {
    return registration(window && window->window ? &window->window->commands() : nullptr, id, shortcut, execute, enabled, data, destroy);
}
void oneui_command_registration_destroy(OneUiCommandRegistration* value) { delete value; }
OneUiCommandResult oneui_widget_query_command_utf8(OneUiWidget* widget, OneUiUtf8String id) {
    try { return widget ? static_cast<OneUiCommandResult>(oneui::queryCommand(widget->widget, nullptr, string(id))) : OneUiCommandNotFound; }
    catch (...) { oneui::capi::recordCurrentException(__func__); return OneUiCommandDisabled; }
}
OneUiCommandResult oneui_widget_execute_command_utf8(OneUiWidget* widget, OneUiUtf8String id) {
    try { return widget ? static_cast<OneUiCommandResult>(oneui::executeCommand(widget->widget, nullptr, string(id))) : OneUiCommandNotFound; }
    catch (...) { oneui::capi::recordCurrentException(__func__); return OneUiCommandDisabled; }
}
OneUiCommandResult oneui_window_query_command_utf8(OneUiWindow* window, OneUiUtf8String id) {
    try { return window && window->window ? static_cast<OneUiCommandResult>(window->window->queryCommand(string(id))) : OneUiCommandNotFound; }
    catch (...) { oneui::capi::recordCurrentException(__func__); return OneUiCommandDisabled; }
}
OneUiCommandResult oneui_window_execute_command_utf8(OneUiWindow* window, OneUiUtf8String id) {
    try { return window && window->window ? static_cast<OneUiCommandResult>(window->window->executeCommand(string(id))) : OneUiCommandNotFound; }
    catch (...) { oneui::capi::recordCurrentException(__func__); return OneUiCommandDisabled; }
}
int oneui_widget_set_text_options_utf8(OneUiWidget* widget, const OneUiTextOptionsUtf8* options) {
    try {
        if (!widget || !widget->widget || !options || options->direction > 2 || options->wrap > 1)
            throw std::invalid_argument("Invalid widget, text direction, or wrap mode");
        oneui::TextOptions value{static_cast<oneui::TextDirection>(options->direction), static_cast<oneui::TextWrapMode>(options->wrap), string(options->locale)};
        if (auto field = std::dynamic_pointer_cast<oneui::TextField>(widget->widget)) field->setTextOptions(std::move(value));
        else if (auto label = std::dynamic_pointer_cast<oneui::Label>(widget->widget)) label->setTextOptions(std::move(value));
        else throw std::invalid_argument("Widget does not support text options");
        return 1;
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}
int oneui_text_field_get_position_utf8(OneUiWidget* widget, OneUiTextPositionUtf8* output) {
    try {
        if (!widget || !output) throw std::invalid_argument("Text field and output are required");
        auto field = std::dynamic_pointer_cast<oneui::TextField>(widget->widget);
        if (!field) throw std::invalid_argument("Widget is not a text field");
        const auto value = field->textPosition();
        *output = {value.utf8Offset, static_cast<unsigned>(value.affinity)};
        return 1;
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}
int oneui_text_field_set_position_utf8(OneUiWidget* widget, OneUiTextPositionUtf8 value) {
    try {
        if (!widget || value.affinity > 1) throw std::invalid_argument("Invalid text field or affinity");
        auto field = std::dynamic_pointer_cast<oneui::TextField>(widget->widget);
        if (!field || !field->setTextPosition({value.utf8_offset, static_cast<oneui::TextAffinity>(value.affinity)}))
            throw std::invalid_argument("Text position must be a grapheme boundary in a text field");
        return 1;
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}
}

#include "oneui_c_api_test_input.inc"
