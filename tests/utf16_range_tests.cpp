#include "platform/shared/text_input.h"
#include <iostream>
#include <limits>

int main() {
    int failures = 0;
    const auto expect = [&](bool value, const char* name) {
        if (!value) { std::cerr << name << '\n'; ++failures; }
    };
    const auto emoji = oneui::unicode::fromUtf8(u8"\U0001f600");
    const auto document = L"A" + emoji + L"xy" + emoji + L"B";
    const auto maxOffset = std::numeric_limits<std::size_t>::max();
    expect(oneui::platform::wideToUtf16Offset(document, document.size()) == 8,
           "Cocoa range lengths count supplementary scalars as two UTF-16 units");
    expect(oneui::platform::utf16ToWideOffset(document, 2) == 1 &&
           oneui::platform::utf16ToWideOffset(document, 2, true) == 1 + emoji.size(),
           "UTF-16 offsets inside emoji round to the requested scalar edge");
    expect(oneui::platform::utf16ReplacementRange(document, 2, 1) ==
               std::make_pair(std::size_t{1}, 1 + emoji.size()),
           "Cocoa replacement cannot split an emoji or discard its prefix");
    expect(oneui::platform::utf16ReplacementRange(document, 2, 0) ==
               std::make_pair(std::size_t{1}, std::size_t{1}),
           "Zero-length Cocoa insertion never deletes an emoji");
    expect(oneui::platform::utf16ReplacementRange(document, 3, maxOffset) ==
               std::make_pair(1 + emoji.size(), document.size()),
           "Oversized Cocoa replacement length cannot overflow");
    expect(oneui::platform::utf16ReplacementRange(document, maxOffset, maxOffset) ==
               std::make_pair(document.size(), document.size()),
           "Out-of-document Cocoa offsets clamp to the end");
    for (std::size_t at = 0; at <= document.size(); at = oneui::unicode::next(document, at)) {
        expect(oneui::platform::utf16ToWideOffset(document, oneui::platform::wideToUtf16Offset(document, at)) == at,
               "Native/UTF-16 scalar offsets round trip");
        if (at == document.size()) break;
    }
    expect(oneui::platform::utf16ReplacementRange({}, maxOffset, maxOffset) ==
               std::make_pair(std::size_t{0}, std::size_t{0}), "Empty documents accept clamped ranges");
    std::cout << "UTF-16 ranges checked with " << sizeof(wchar_t) * 8 << "-bit wchar_t\n";
    return failures ? 1 : 0;
}
