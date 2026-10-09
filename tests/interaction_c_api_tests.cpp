#include "oneui/oneui_c_api.h"
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

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

static void throwRuntime(void* message) { throw std::runtime_error(*static_cast<std::string*>(message)); }
static void throwAllocation(void*) { throw std::bad_alloc(); }
static void throwUnknown(void*) { throw 7; }

static void errorDiagnostics(OneUiWidget* field) {
    oneui_clear_error();
    require(oneui_last_error_code() == OneUiErrorNone && !*oneui_last_error(), "explicit clear resets code and message");
    std::string message = u8"native failure: 连接失败";
    auto* token = oneui_widget_register_command_utf8(field, str("throw"), nullptr, throwRuntime, nullptr, &message, nullptr);
    require(token != nullptr, "register throwing C++ callback");
    require(oneui_widget_execute_command_utf8(field, str("throw")) == OneUiCommandDisabled, "exception keeps existing failure return");
    require(oneui_last_error_code() == OneUiErrorOperationFailed, "std::exception classification");
    require(std::string(oneui_last_error()) == "oneui_widget_execute_command_utf8: " + message, "UTF-8 exception reason and API context retained");
    const std::string mainError = oneui_last_error();
    require(oneui_widget_query_command_utf8(field, str("throw")) == OneUiCommandEnabled, "subsequent operation succeeds");
    require(oneui_last_error() == mainError, "success does not discard error");
    bool isolated = false;
    std::thread other([&] {
        const bool initiallyEmpty = oneui_last_error_code() == OneUiErrorNone && !*oneui_last_error();
        oneui_set_builtin_text_utf8(-1, "irrelevant");
        isolated = initiallyEmpty && oneui_last_error_code() == OneUiErrorInvalidArgument;
        oneui_clear_error();
    });
    other.join();
    require(isolated && oneui_last_error() == mainError, "errors and clear are isolated across threads");

    message.clear();
    for (int i = 0; i < 600; ++i) message += u8"😀";
    oneui_widget_execute_command_utf8(field, str("throw"));
    const std::string truncated = oneui_last_error();
    const std::string prefix = "oneui_widget_execute_command_utf8: ";
    require(truncated.size() <= 1023 && truncated.size() > 1000 && truncated.find(prefix) == 0, "error message is bounded");
    require((truncated.size() - prefix.size()) % 4 == 0, "truncation preserves scalar boundaries");
    for (size_t i = prefix.size(); i < truncated.size(); i += 4)
        require(truncated.substr(i, 4) == u8"😀", "truncated UTF-8 retains complete scalars");
    message = "\xffX\xc0\xafY\xf4\x90\x80\x80Z\xe2";
    oneui_widget_execute_command_utf8(field, str("throw"));
    require(oneui_last_error() == prefix + "?X??Y????Z?", "malformed and incomplete UTF-8 is sanitized");
    oneui_command_registration_destroy(token);

    token = oneui_widget_register_command_utf8(field, str("oom"), nullptr, throwAllocation, nullptr, nullptr, nullptr);
    require(token != nullptr, "register allocation failure");
    oneui_widget_execute_command_utf8(field, str("oom"));
    require(oneui_last_error_code() == OneUiErrorOutOfMemory && std::strstr(oneui_last_error(), "Memory allocation failed"), "allocation failure has allocation-free diagnostic");
    oneui_command_registration_destroy(token);
    token = oneui_widget_register_command_utf8(field, str("unknown"), nullptr, throwUnknown, nullptr, nullptr, nullptr);
    require(token != nullptr, "register unknown exception");
    oneui_widget_execute_command_utf8(field, str("unknown"));
    require(oneui_last_error_code() == OneUiErrorUnknownException, "non-standard exception classification");
    oneui_command_registration_destroy(token);
    oneui_clear_error();
}
int main() {
    try {
        require(oneui_utf8_abi_version() == ONEUI_UTF8_ABI_VERSION, "Runtime ABI matches the current header");
        auto* field = oneui_text_field_create_utf8(str("placeholder"));
        require(field != nullptr, "create editor");
        errorDiagnostics(field);
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
        require(oneui_last_error_code() == OneUiErrorInvalidArgument && std::strstr(oneui_last_error(), "registration"), "duplicate command retains invalid-argument reason");
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
