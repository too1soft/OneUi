#include "text/text_layout.h"
#include "internal/unicode.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "unicode/ubidi.h"
#include "unicode/uchar.h"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
int failures = 0;
int tailoredWords = 0;
std::vector<std::string> tokens(const std::string& text) {
    std::istringstream input(text); std::vector<std::string> result; std::string token;
    while (input >> token) result.push_back(token);
    return result;
}
std::vector<int> numbers(const std::string& text) {
    std::vector<int> result;
    for (const auto& token : tokens(text)) result.push_back(token == "x" ? -1 : std::stoi(token));
    return result;
}
void check(bool good, const char* file, int line) {
    if (good) return;
    if (++failures <= 50) std::cerr << file << ':' << line << " conformance mismatch\n";
}
std::ifstream input(const char* name) {
    std::ifstream stream(std::string(ONEUI_TEXT_ASSETS) + '/' + name);
    if (!stream) throw std::runtime_error(std::string("Missing required test data: ") + name);
    return stream;
}
std::size_t breaks(SkUnicode& engine, const char* name, SkUnicode::BreakType type) {
    auto file = input(name); std::string line; int lineNumber = 0; std::size_t cases = 0;
    while (std::getline(file, line)) {
        ++lineNumber; line = line.substr(0, line.find('#'));
        if (tokens(line).empty()) continue;
        std::wstring wide; std::string utf8; std::vector<int> expected;
        for (const auto& token : tokens(line)) {
            if (token == u8"÷") expected.push_back(static_cast<int>(utf8.size()));
            else if (token != u8"×") {
                const auto cp = static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
                oneui::unicode::appendUtf8CodePoint(utf8, cp); oneui::unicode::appendWideCodePoint(wide, cp);
            }
        }
        auto iterator = engine.makeBreakIterator("und", type);
        if (!iterator || !iterator->setText(utf8.data(), static_cast<int>(utf8.size()))) throw std::runtime_error("Break iterator initialization failed");
        std::vector<int> actual;
        for (auto p = iterator->first(); !iterator->isDone(); p = iterator->next()) actual.push_back(p);
        if (type == SkUnicode::BreakType::kWords && utf8.find(':') != std::string::npos) {
            // ICU 74's intentional root-locale colon tailoring (ICU-22112/22127).
            // Assert its explicit profile instead of skipping upstream cases.
            const auto original = expected;
            for (std::size_t p = 0; p < utf8.size(); ++p) if (utf8[p] == ':') {
                expected.push_back(static_cast<int>(p));
                // WB4 ignores both Extend and Format (not only grapheme extenders).
                const auto suffix = oneui::unicode::fromUtf8(std::string_view(utf8).substr(p + 1));
                std::size_t end = 0;
                while (end < suffix.size()) {
                    const auto next = oneui::unicode::next(suffix, end);
                    // Decode the scalar via the portable UTF-16 conversion above.
                    std::uint32_t cp = static_cast<std::uint32_t>(suffix[end]);
                    if (sizeof(wchar_t) == 2 && next - end == 2) cp = 0x10000 + ((cp - 0xd800) << 10) + (suffix[end + 1] - 0xdc00);
                    const auto prop = u_getIntPropertyValue(cp, UCHAR_WORD_BREAK);
                    if (prop != U_WB_EXTEND && prop != U_WB_FORMAT && prop != U_WB_ZWJ) break;
                    end = next;
                }
                expected.push_back(static_cast<int>(p + 1 + oneui::unicode::toUtf8(std::wstring_view(suffix).substr(0, end)).size()));
            }
            std::sort(expected.begin(), expected.end());
            expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
            if (original != expected) ++tailoredWords;
        }
        check(actual == expected, name, lineNumber);
        if (type == SkUnicode::BreakType::kGraphemes) {
            oneui::text::LayoutOptions options; options.sensitive = true;
            const auto layout = oneui::text::Layout::make(wide, options);
            actual.clear();
            for (auto p : layout->graphemes()) actual.push_back(static_cast<int>(layout->utf8Offset(p)));
            check(actual == expected, "Layout grapheme/offset adapter", lineNumber);
        }
        ++cases;
    }
    return cases;
}
bool bidi(SkUnicode& engine, const std::vector<std::uint32_t>& scalars, int direction, int expectedBase,
          const std::vector<int>& expectedLevels, const std::vector<int>& expectedOrder) {
    std::u16string utf16; std::vector<int> positions;
    for (auto cp : scalars) {
        positions.push_back(static_cast<int>(utf16.size()));
        if (cp > 0xffff) { utf16.push_back(static_cast<char16_t>(0xd800 + ((cp - 0x10000) >> 10))); utf16.push_back(static_cast<char16_t>(0xdc00 + ((cp - 0x10000) & 1023))); }
        else utf16.push_back(static_cast<char16_t>(cp));
    }
    UErrorCode error = U_ZERO_ERROR;
    auto* resolved = ubidi_open();
    ubidi_setPara(resolved, reinterpret_cast<const UChar*>(utf16.data()), static_cast<int>(utf16.size()),
                  direction == 2 ? UBIDI_DEFAULT_LTR : static_cast<UBiDiLevel>(direction), nullptr, &error);
    if (U_FAILURE(error)) { ubidi_close(resolved); throw std::runtime_error("Bidi initialization failed"); }
    const auto base = ubidi_getParaLevel(resolved);
    std::vector<SkUnicode::BidiLevel> directLevels;
    for (int p : positions) directLevels.push_back(ubidi_getLevelAt(resolved, p));
    ubidi_close(resolved);
    if (expectedBase >= 0 && base != expectedBase) return false;
    auto iterator = engine.makeBidiIterator(reinterpret_cast<const uint16_t*>(utf16.data()), static_cast<int>(utf16.size()), base & 1 ? SkBidiIterator::kRTL : SkBidiIterator::kLTR);
    if (!iterator) throw std::runtime_error("SkUnicode Bidi iterator initialization failed");
    std::vector<SkUnicode::BidiLevel> levels;
    for (int p : positions) levels.push_back(iterator->getLevelAt(p));
    if (levels != directLevels) throw std::runtime_error("SkUnicode and direct ICU Bidi levels disagree");
    if (expectedLevels.size() != levels.size()) throw std::runtime_error("Invalid Bidi fixture");
    unsigned expectedDirection = 0, actualDirection = 0;
    bool sameLevels = true;
    for (std::size_t i = 0; i < levels.size(); ++i) if (expectedLevels[i] >= 0) {
        expectedDirection |= 1u << (expectedLevels[i] & 1);
        actualDirection |= 1u << (levels[i] & 1);
        sameLevels &= expectedLevels[i] == levels[i];
    }
    // ICU's documented unidirectional optimization returns paragraph levels.
    // Permit only equal parity in wholly unidirectional text; order is still exact.
    if (!sameLevels && (expectedDirection == 3 || expectedDirection != actualDirection)) {
        static int debug = 0;
        if (++debug <= 3) { std::cerr << "levels actual:"; for (auto l : levels) std::cerr << ' ' << int(l); std::cerr << " expected:"; for (auto l : expectedLevels) std::cerr << ' ' << l; std::cerr << '\n'; }
        return false;
    }
    std::vector<int32_t> visual(levels.size());
    engine.reorderVisual(levels.data(), static_cast<int>(levels.size()), visual.data());
    std::vector<int> order;
    for (auto index : visual) if (expectedLevels.at(index) >= 0) order.push_back(index);
    if (order != expectedOrder) {
        static int debug = 0;
        if (++debug <= 3) { std::cerr << "order actual:"; for (auto l : order) std::cerr << ' ' << l; std::cerr << " expected:"; for (auto l : expectedOrder) std::cerr << ' ' << l; std::cerr << '\n'; }
    }
    return order == expectedOrder;
}
std::size_t bidiCharacters(SkUnicode& engine) {
    auto file = input("BidiCharacterTest.txt"); std::string line; int lineNumber = 0; std::size_t cases = 0;
    while (std::getline(file, line)) {
        ++lineNumber; line = line.substr(0, line.find('#'));
        if (tokens(line).empty()) continue;
        std::vector<std::string> parts; std::istringstream stream(line); std::string part;
        while (std::getline(stream, part, ';')) parts.push_back(part);
        if (parts.size() != 5) throw std::runtime_error("Invalid BidiCharacterTest record");
        std::vector<std::uint32_t> scalars;
        for (const auto& token : tokens(parts[0])) scalars.push_back(static_cast<std::uint32_t>(std::stoul(token, nullptr, 16)));
        check(bidi(engine, scalars, std::stoi(parts[1]), std::stoi(parts[2]), numbers(parts[3]), numbers(parts[4])), "BidiCharacterTest", lineNumber);
        ++cases;
    }
    return cases;
}
std::size_t bidiClasses(SkUnicode& engine) {
    const std::map<std::string, std::uint32_t> representative{{"L",0x61},{"R",0x5d0},{"AL",0x627},{"EN",0x30},{"AN",0x660},
        {"ES",0x2b},{"ET",0x24},{"CS",0x2c},{"NSM",0x300},{"BN",0xad},{"B",0x2029},{"S",9},{"WS",0x20},{"ON",0x21},
        {"LRE",0x202a},{"RLE",0x202b},{"PDF",0x202c},{"LRO",0x202d},{"RLO",0x202e},{"LRI",0x2066},{"RLI",0x2067},{"FSI",0x2068},{"PDI",0x2069}};
    auto file = input("BidiTest.txt"); std::string line; int lineNumber = 0; std::size_t cases = 0;
    std::vector<int> levels, order;
    while (std::getline(file, line)) {
        ++lineNumber; line = line.substr(0, line.find('#'));
        if (tokens(line).empty()) continue;
        if (line.rfind("@Levels:", 0) == 0) { levels = numbers(line.substr(8)); continue; }
        if (line.rfind("@Reorder:", 0) == 0) { order = numbers(line.substr(9)); continue; }
        if (line.front() == '@') continue;
        const auto separator = line.find(';');
        if (separator == std::string::npos) throw std::runtime_error("Invalid BidiTest record");
        std::vector<std::uint32_t> scalars;
        for (const auto& token : tokens(line.substr(0, separator))) scalars.push_back(representative.at(token));
        const int mask = std::stoi(line.substr(separator + 1), nullptr, 16);
        for (int direction = 0; direction < 3; ++direction) {
            const int bit = direction == 2 ? 1 : direction == 0 ? 2 : 4;
            if (mask & bit) { check(bidi(engine, scalars, direction, -1, levels, order), "BidiTest", lineNumber); ++cases; }
        }
    }
    return cases;
}
std::size_t bidiBracketRegressions(SkUnicode& engine) {
    const auto levels = [&engine](const std::u16string& text) {
        auto iterator = engine.makeBidiIterator(reinterpret_cast<const uint16_t*>(text.data()),
                                               static_cast<int>(text.size()), SkBidiIterator::kRTL);
        if (!iterator) throw std::runtime_error("Bidi regression initialization failed");
        std::vector<int> result;
        for (int i = 0; i < static_cast<int>(text.size()); ++i) result.push_back(iterator->getLevelAt(i));
        return result;
    };
    const auto nest = [](int count, char16_t open = u'(', char16_t close = u')') {
        return u"a" + std::u16string(count, open) + u"b" + std::u16string(count, close);
    };
    // Removing only the bracket-pair property preserves the ON bidi class.
    // BD16 overflow returns an empty list for the entire affected sequence,
    // so this is an independent oracle, not a second copy of the stack algorithm.
    const auto unpaired = [](std::u16string text) {
        for (auto& c : text) if (u_getIntPropertyValue(c, UCHAR_BIDI_PAIRED_BRACKET_TYPE) != U_BPT_NONE) c = u'!';
        return text;
    };
    std::size_t cases = 0;
    const auto equivalent = [&](const std::u16string& actual, const std::u16string& expected) {
        check(levels(actual) == levels(expected), "BD16 sequence regression", static_cast<int>(++cases));
    };
    for (int depth : {62, 63, 64}) {
        for (bool canonical : {false, true}) {
            const auto text = nest(depth, canonical ? u'\u3008' : u'(', canonical ? u'\u232a' : u')');
            std::vector<int> expected(text.size(), 2);
            if (depth == 64) std::fill(expected.begin() + depth + 2, expected.end(), 1);
            check(levels(text) == expected, "BD16 exact stack capacity", static_cast<int>(++cases));
        }
    }
    const auto overflow = nest(64);
    const auto earlierPairs = u"a(b)\u0300\u05d0" + overflow + u"a(b)\u05d0";
    equivalent(earlierPairs, unpaired(earlierPairs));
    // An isolate has its own stack. Parent overflow resumes after PDI but must
    // neither suppress nor roll back the child's valid N0 pairs.
    const std::u16string isolate = u"\u2067a(b)\u05d0\u2069";
    equivalent(overflow + isolate + u"a(b)\u05d0", unpaired(overflow) + isolate + u"a!b!\u05d0");
    equivalent(u"a(b)\u05d0" + isolate + overflow, u"a!b!\u05d0" + isolate + unpaired(overflow));
    equivalent(u"a(\u2067" + overflow + u"\u2069b)\u05d0", u"a(\u2067" + unpaired(overflow) + u"\u2069b)\u05d0");
    // Explicit level-run boundaries and paragraph boundaries reset overflow.
    const std::u16string embedded = u"\u202ba(b)\u05d0\u202ca(b)\u05d0";
    equivalent(overflow + embedded, unpaired(overflow) + embedded);
    equivalent(overflow + u"\u2029a(b)\u05d0", unpaired(overflow) + u"\u2029a(b)\u05d0");
    equivalent(u"a(b)\u05d0\u2029" + overflow, u"a(b)\u05d0\u2029" + unpaired(overflow));
    // ICU retains resolved N0c entries for possible context correction. Those
    // entries are not active opening brackets and must not consume capacity.
    std::u16string pending = u"a[";
    for (int i = 0; i < 128; ++i) pending += u"(b)";
    pending += u"]";
    check(levels(pending) == std::vector<int>(pending.size(), 2), "BD16 pending N0c capacity", static_cast<int>(++cases));
    // Overridden brackets are strong characters, not BD16 candidates.
    equivalent(u"\u202d" + overflow + u"\u202c", u"\u202d" + unpaired(overflow) + u"\u202c");
    return cases;
}
}
int main() {
    try {
        // Initialize the same embedded ICU instance used by production layouts.
        oneui::text::Layout::make(L"initialize");
        UVersionInfo version; u_getUnicodeVersion(version);
        if (version[0] != 15 || version[1] != 1) throw std::runtime_error("Acceptance requires Unicode 15.1 data");
        auto engine = SkUnicodes::ICU::Make();
        if (!engine) throw std::runtime_error("Required ICU text engine unavailable");
        const auto graphemes = breaks(*engine, "GraphemeBreakTest.txt", SkUnicode::BreakType::kGraphemes);
        const auto words = breaks(*engine, "WordBreakTest.txt", SkUnicode::BreakType::kWords);
        const auto characters = bidiCharacters(*engine);
        const auto classes = bidiClasses(*engine);
        const auto regressions = bidiBracketRegressions(*engine);
        if (graphemes < 1000 || words < 1000 || characters < 90000 || classes < 700000) throw std::runtime_error("Conformance fixture unexpectedly incomplete");
        std::cout << "Unicode 15.1: " << graphemes << " grapheme, " << words << " word (" << tailoredWords << " explicit ICU colon profiles), " << characters << " Bidi character, " << classes << " Bidi class, " << regressions << " bracket regression cases; failures=" << failures << '\n';
        return failures ? 1 : 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
