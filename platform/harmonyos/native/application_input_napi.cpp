#include "application_napi.h"

namespace oneui::harmony::application {
namespace {
bool active(Host &host) {
    OneUiMobileStateV1 state{}; host.surface.core.runtime().state(state);
    return state.lifecycle == ONEUI_MOBILE_ACTIVE && state.surface_available;
}
std::u16string utf16(napi_env env, napi_value value) {
    size_t length; check(napi_get_value_string_utf16(env, value, nullptr, 0, &length));
    if (length > 1024 * 1024) throw std::runtime_error("Editor text bound");
    std::u16string text(length + 1, u'\0'); check(napi_get_value_string_utf16(env, value, text.data(), text.size(), &length)); text.resize(length); return text;
}
void wipe(std::u16string &text) { volatile char16_t *p = text.empty() ? nullptr : &text[0]; for (size_t i = 0; i < text.size(); ++i) p[i] = 0; }
napi_value editing(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = host(env, a[0]);
        if (!active(*value)) return undefined(env);
        auto state = value->surface.core.input().snapshot();
        if (!state.session) return undefined(env);
        napi_value result, text; check(napi_create_object(env, &result));
        check(napi_create_string_utf16(env, state.text.data(), state.text.size(), &text));
        property(env, result, "session", string(env, std::to_string(state.session))); property(env, result, "text", text);
        property(env, result, "anchor", number(env, state.anchor)); property(env, result, "caret", number(env, state.caret));
        property(env, result, "composingStart", number(env, state.composingStart)); property(env, result, "composingEnd", number(env, state.composingEnd));
        property(env, result, "multiline", boolean(env, state.multiline)); property(env, result, "sensitive", boolean(env, state.sensitive)); return result;
    });
}
napi_value edit(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 7); auto value = host(env, a[0]);
        if (!active(*value)) return boolean(env, false);
        mobile::EditingState state; state.session = handle(env, a[1]); state.text = utf16(env, a[2]);
        state.anchor = integer(env, a[3]); state.caret = integer(env, a[4]); state.composingStart = integer(env, a[5]); state.composingEnd = integer(env, a[6]);
        const bool accepted = value->surface.core.input().apply(state); wipe(state.text);
        if (accepted) value->surface.core.runtime().invalidate();
        return boolean(env, accepted);
    });
}
napi_value sensitive(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 6); auto value = host(env, a[0]); const int command = integer(env, a[2]);
        if (!active(*value)) return boolean(env, false);
        if (command < 0 || command > 7) throw std::runtime_error("Sensitive edit operation");
        auto text = utf16(env, a[3]); const bool accepted = value->surface.core.input().editSensitive(handle(env, a[1]),
            static_cast<mobile::SensitiveEdit>(command), text, integer(env, a[4]), integer(env, a[5])); wipe(text);
        if (accepted) value->surface.core.runtime().invalidate();
        return boolean(env, accepted);
    });
}
napi_value command(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 6); auto value = host(env, a[0]); const int operation = integer(env, a[2]);
        if (!active(*value)) return boolean(env, false);
        if (operation < 0 || operation > 7) throw std::runtime_error("Editor operation");
        const auto session = handle(env, a[1]); const int first = integer(env, a[4]), second = integer(env, a[5]);
        auto text = utf16(env, a[3]);
        const bool accepted = value->surface.core.input().editCommand(session,
            static_cast<mobile::SensitiveEdit>(operation), text, first, second); wipe(text);
        if (accepted) value->surface.core.runtime().invalidate();
        return boolean(env, accepted);
    });
}
napi_value geometry(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = host(env, a[0]);
        if (!active(*value)) return undefined(env);
        const auto geometry = value->surface.core.input().selectionGeometry();
        if (!geometry.session) return undefined(env);
        napi_value result, rectangle; check(napi_create_object(env, &result)); check(napi_create_object(env, &rectangle));
        property(env, result, "session", string(env, std::to_string(geometry.session)));
        napi_value coordinate;
        auto set = [&](const char *name, float value) { check(napi_create_double(env, value, &coordinate)); property(env, rectangle, name, coordinate); };
        set("left", geometry.caretRect.x); set("top", geometry.caretRect.y); set("width", geometry.caretRect.width); set("height", geometry.caretRect.height);
        property(env, result, "caret", rectangle); return result;
    });
}
}
void registerInput(napi_env env, napi_value exports) {
    const napi_property_descriptor methods[]{
        {"editing", nullptr, editing, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"edit", nullptr, edit, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sensitiveEdit", nullptr, sensitive, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"commandEdit", nullptr, command, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"geometry", nullptr, geometry, nullptr, nullptr, nullptr, napi_default, nullptr}};
    check(napi_define_properties(env, exports, std::size(methods), methods));
}
}
