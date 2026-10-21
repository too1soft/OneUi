#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oneui/view_cpp_adapter.h"
#include <cassert>
#include <iostream>

int main() {
    const std::string utf8 = u8"A中文🙂𠮷";
    const std::wstring native = L"A中文\U0001F642\U00020BB7";
    assert(oneui::ui::wide(utf8) == native);
    assert(oneui::ui::view::wide(utf8) == native);
    assert(oneui::ui::view::utf8(native) == utf8);
    assert(oneui::unicode::toUtf8(oneui::unicode::fromUtf8(utf8)) == utf8);
    assert(native.size() == (sizeof(wchar_t) == 2 ? 7 : 5));
    // A non-scalar input cannot put UTF-16 surrogate code points into UTF-32.
    const auto invalid = oneui::unicode::fromUtf8("\xED\xA0\x80");
    for (wchar_t code : invalid) assert(code == 0xFFFD);
    std::cout << "native-wchar declarative adapters passed (" << sizeof(wchar_t) * 8 << " bit)\n";
}
