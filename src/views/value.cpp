#include "runtime.h"
#include "internal/unicode.h"
#include "internal/view_profile.h"
#include <cmath>
#include <limits>
namespace oneui::views {
Value Value::copy(const OneUiValue &v) {
    check(v.reserved == 0 && v.tag >= 1 && v.tag <= 4, ONEUI_VIEW_TYPE,
          "Invalid scalar tag/reserved");
    Value result;
    result.tag = v.tag;
    if (v.tag != 4) {
        const auto *bytes = reinterpret_cast<const uint8_t *>(&v.as);
        const size_t used = v.tag == 1 ? sizeof(uint32_t) : sizeof(uint64_t);
        check(std::all_of(bytes + used, bytes + sizeof(v.as), [](uint8_t b) { return b == 0; }),
              ONEUI_VIEW_TYPE, "Nonzero unused value bytes");
    }
    if (v.tag == 1) {
        check(v.as.boolean <= 1, ONEUI_VIEW_TYPE, "Invalid boolean");
        result.integer = v.as.boolean;
    } else if (v.tag == 2)
        result.integer = v.as.integer;
    else if (v.tag == 3) {
        check(std::isfinite(v.as.number), ONEUI_VIEW_TYPE, "Non-finite scalar");
        result.number = v.as.number;
    } else {
        check(v.as.utf8.length <= 1024 * 1024 && (v.as.utf8.data || !v.as.utf8.length),
              ONEUI_VIEW_TYPE, "Invalid UTF-8 buffer");
        if (v.as.utf8.length)
            result.text.assign(v.as.utf8.data, v.as.utf8.length);
        check(oneview::utf8(result.text), ONEUI_VIEW_TYPE, "Invalid UTF-8 value");
    }
    return result;
}
OneUiValue Value::borrow() const {
    OneUiValue v{};
    v.tag = tag;
    if (tag == 1)
        v.as.boolean = uint32_t(integer);
    else if (tag == 2)
        v.as.integer = integer;
    else if (tag == 3)
        v.as.number = number;
    else
        v.as.utf8 = {text.data(), text.size()};
    return v;
}
bool Value::operator==(const Value &r) const {
    return tag == r.tag && integer == r.integer && number == r.number && text == r.text;
}
Value Runtime::constant(uint32_t index) const {
    const auto &r = image.records[oneview::VALU].at(index);
    Value value;
    value.tag = r[0];
    uint64_t bits = uint64_t(r[2]) | uint64_t(r[3]) << 32;
    if (value.tag == 4)
        value.text = image.strings.at(r[2]);
    else if (value.tag == 3)
        std::memcpy(&value.number, &bits, 8);
    else
        std::memcpy(&value.integer, &bits, 8);
    return value;
}
std::string narrow(const std::wstring &text) {
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(text);
}
void setScalar(ui::Mount &factory, ui::Element &e, const std::string &name, const Value &v) {
    if (name == "host")
        return; // Identity belongs to the HOST table, never a widget setter.
    if (name == "item-key") {
        check(v.tag==4 && v.text=="id",ONEUI_VIEW_TYPE,"Builtin records use id keys");
        return; // Validated schema identity, not a Table display property.
    }
    if (v.tag == 1)
        factory.set(e, name, bool(v.integer));
    else if (v.tag == 2) {
        check(v.integer >= -1 && v.integer <= INT_MAX, ONEUI_VIEW_TYPE, "Component integer range");
        factory.set(e, name, int(v.integer));
    } else if (v.tag == 3) {
        check(std::abs(v.number) <= std::numeric_limits<float>::max(), ONEUI_VIEW_TYPE,
              "Component float range");
        factory.set(e, name, float(v.number));
    } else if (name == "class")
        factory.set(e, name, v.text); // Class tokens stay UTF-8 in StyleRegistry.
    else {
        // IR strings and foreign values have already passed strict UTF-8
        // validation. Reuse the native C ABI converter without a locale facet.
        std::wstring text;
        { ONEUI_VIEW_TIME(Conversion); text = unicode::fromUtf8(v.text); }
        factory.set(e, name, text);
    }
}
} // namespace oneui::views
