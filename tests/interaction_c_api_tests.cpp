#include "oneui/oneui_c_api.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

static OneUiUtf8String str(const char* text) { return {text, std::strlen(text)}; }
static void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
struct Context { int* calls; int* drops; OneUiCommandRegistration** self = nullptr; };
static void invoke(void* data) {
    auto* context = static_cast<Context*>(data);
    ++*context->calls;
    if (context->self) { auto* value = *context->self; *context->self = nullptr; oneui_command_registration_destroy(value); }
    // Releasing the token inside the callback must not release this context yet.
    require(*context->drops == 0, "in-flight context destroyed prematurely");
}
static void release(void* data) { auto* c = static_cast<Context*>(data); ++*c->drops; delete c; }
static int disabled(void*) { return 0; }
int main() {
    try {
        require(ONEUI_UTF8_ABI_VERSION == 26, "ABI is 26");
        auto* field = oneui_text_field_create_utf8(str("placeholder"));
        require(field != nullptr, "create editor");
        int calls = 0, drops = 0;
        OneUiCommandRegistration* token = nullptr;
        OneUiKeyChordUtf8 chord{str("k"), OneUiKeyModifierPrimary};
        token = oneui_widget_register_command_utf8(field, str("test.self"), &chord, invoke, nullptr, new Context{&calls, &drops, &token}, release);
        require(token && oneui_widget_query_command_utf8(field, str("test.self")) == OneUiCommandEnabled, "query command");
        require(oneui_widget_execute_command_utf8(field, str("test.self")) == OneUiCommandExecuted && calls == 1 && drops == 1 && !token, "self-unregister releases exactly once after invocation");
        drops = 0;
        token = oneui_widget_register_command_utf8(field, str("disabled"), nullptr, invoke, disabled, new Context{&calls, &drops}, release);
        require(token && oneui_widget_execute_command_utf8(field, str("disabled")) == OneUiCommandDisabled && calls == 1, "disabled command never invokes");
        int failedDrops = 0;
        auto* duplicate = oneui_widget_register_command_utf8(field, str("disabled"), nullptr, invoke, nullptr, new Context{&calls, &failedDrops}, release);
        require(!duplicate && failedDrops == 1, "failure takes ownership and releases context");
        OneUiTextOptionsUtf8 options{0, 0, str("und")};
        require(oneui_widget_set_text_options_utf8(field, &options), "text options");
        options.direction = 99;
        require(!oneui_widget_set_text_options_utf8(field, &options), "invalid direction rejected");
        oneui_text_field_set_text_utf8(field, str(u8"中é👨‍👩‍👧‍👦"));
        OneUiTextPositionUtf8 position{};
        require(oneui_text_field_set_position_utf8(field, {3, 1}) && oneui_text_field_get_position_utf8(field, &position) && position.utf8_offset == 3, "portable UTF-8 position");
        require(!oneui_text_field_set_position_utf8(field, {1, 1}), "UTF-8 interior rejected");
        require(!oneui_text_field_set_position_utf8(field, {4, 1}), "combining interior rejected");
        require(!oneui_text_field_set_position_utf8(field, {10, 1}), "ZWJ interior rejected");
        oneui_widget_destroy(field);
        require(drops == 1, "scope destruction releases callback context even while token survives");
        oneui_command_registration_destroy(token);
        require(drops == 1, "expired token release is safe");
        std::cout << "Interaction C ABI tests passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
