#include "unicode/ubrk.h"
#include "unicode/uclean.h"
#include "unicode/udata.h"
#include "unicode/uloc.h"
#include "unicode/unorm2.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
uint64_t digest = 14695981039346656037ull;
void add(uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        digest ^= (value >> shift) & 255;
        digest *= 1099511628211ull;
    }
}
void check(UErrorCode error) {
    if (U_FAILURE(error)) throw std::runtime_error(u_errorName(error));
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("ICU package path required");
        std::ifstream input(argv[1], std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(input), {}};
        if (bytes.empty()) throw std::runtime_error("ICU package unavailable");
        UErrorCode error = U_ZERO_ERROR;
        udata_setCommonData(bytes.data(), &error);
        check(error);
        u_init(&error);
        check(error);
        const std::u16string text = u"中文换行日本語한국어 ไทยภาษาไทย ພາສາລາວ ភាសាខ្មែរ မြန်မာ "
            u"हिन्दी العربية עברית 👨‍👩‍👧‍👦 🇨🇳 á a.b 1,234.5 (test)\r\nSecond sentence!";
        int cases = 0;
        for (int locale = 0; locale < uloc_countAvailable(); ++locale) {
            const char* name = uloc_getAvailable(locale);
            for (const auto type : {UBRK_CHARACTER, UBRK_WORD, UBRK_LINE, UBRK_SENTENCE}) {
                error = U_ZERO_ERROR;
                auto* iterator = ubrk_open(type, name, reinterpret_cast<const UChar*>(text.data()),
                                           static_cast<int32_t>(text.size()), &error);
                check(error);
                if (!iterator) throw std::runtime_error("Missing break iterator");
                for (auto position = ubrk_first(iterator); position != UBRK_DONE; position = ubrk_next(iterator)) {
                    add(position);
                    add(ubrk_getRuleStatus(iterator));
                }
                ubrk_close(iterator);
                ++cases;
            }
        }
        for (const auto* normalizer : {unorm2_getNFCInstance(&error), unorm2_getNFKCInstance(&error),
                                       unorm2_getNFKCCasefoldInstance(&error)}) {
            check(error);
            UChar normalized[1024];
            const auto length = unorm2_normalize(normalizer, reinterpret_cast<const UChar*>(text.data()),
                static_cast<int32_t>(text.size()), normalized, 1024, &error);
            check(error);
            for (int i = 0; i < length; ++i) add(normalized[i]);
        }
        std::cout << cases << " locale/break combinations; digest=" << std::hex << digest << '\n';
        u_cleanup();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
