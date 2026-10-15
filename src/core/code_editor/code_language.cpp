#include "oneui/controls/code_editor.h"

#include "internal/unicode.h"

#include <cctype>
#include <stdexcept>

namespace oneui {
namespace {

// A deliberately small JSON reader: objects, arrays, strings, booleans,
// null and numbers (numbers are skipped). Unknown keys are ignored so newer
// descriptions still load.
class Reader {
public:
    explicit Reader(std::string_view text) : text_(text) {}

    void fail(const char* what) const {
        throw std::invalid_argument(std::string("CodeEditor language: ") + what + " at byte " + std::to_string(at_));
    }
    void space() {
        while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\n' || text_[at_] == '\r')) ++at_;
    }
    bool peek(char c) {
        space();
        return at_ < text_.size() && text_[at_] == c;
    }
    void expect(char c) {
        if (!peek(c)) fail("unexpected character");
        ++at_;
    }
    bool done() {
        space();
        return at_ >= text_.size();
    }
    std::wstring string() {
        expect('"');
        std::string out;
        while (true) {
            if (at_ >= text_.size()) fail("unterminated string");
            const char c = text_[at_++];
            if (c == '"') break;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (at_ >= text_.size()) fail("bad escape");
            const char e = text_[at_++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                auto unit = hex4();
                if (unit >= 0xd800 && unit <= 0xdbff && at_ + 1 < text_.size() && text_[at_] == '\\' && text_[at_ + 1] == 'u') {
                    at_ += 2;
                    const auto low = hex4();
                    if (low < 0xdc00 || low > 0xdfff) fail("bad surrogate");
                    unit = 0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00);
                }
                unicode::appendUtf8CodePoint(out, unit);
                break;
            }
            default: fail("bad escape");
            }
        }
        return unicode::fromUtf8(out);
    }
    bool boolean() {
        space();
        if (text_.compare(at_, 4, "true") == 0) { at_ += 4; return true; }
        if (text_.compare(at_, 5, "false") == 0) { at_ += 5; return false; }
        fail("expected a boolean");
        return false;
    }
    std::vector<std::wstring> strings() {
        std::vector<std::wstring> result;
        expect('[');
        if (peek(']')) { ++at_; return result; }
        while (true) {
            result.push_back(string());
            if (peek(',')) { ++at_; continue; }
            expect(']');
            return result;
        }
    }
    void skip() {
        space();
        if (at_ >= text_.size()) fail("unexpected end");
        const char c = text_[at_];
        if (c == '"') { string(); return; }
        if (c == '{' || c == '[') {
            const char close = c == '{' ? '}' : ']';
            ++at_;
            if (peek(close)) { ++at_; return; }
            while (true) {
                if (c == '{') { string(); expect(':'); }
                skip();
                if (peek(',')) { ++at_; continue; }
                expect(close);
                return;
            }
        }
        if (text_.compare(at_, 4, "null") == 0) { at_ += 4; return; }
        if (c == 't' || c == 'f') { boolean(); return; }
        const auto start = at_;
        while (at_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[at_])) || text_[at_] == '-' ||
                                      text_[at_] == '+' || text_[at_] == '.' || text_[at_] == 'e' || text_[at_] == 'E')) ++at_;
        if (start == at_) fail("unexpected value");
    }

private:
    std::uint32_t hex4() {
        if (at_ + 4 > text_.size()) fail("bad \\u escape");
        std::uint32_t value = 0;
        for (int k = 0; k < 4; ++k) {
            const char h = text_[at_++];
            value <<= 4;
            if (h >= '0' && h <= '9') value |= static_cast<std::uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') value |= static_cast<std::uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') value |= static_cast<std::uint32_t>(h - 'A' + 10);
            else fail("bad \\u escape");
        }
        return value;
    }

    std::string_view text_;
    std::size_t at_ = 0;
};

} // namespace

CodeLanguage parseCodeLanguage(std::string_view json) {
    CodeLanguage language;
    language.stringQuotes.clear();
    Reader reader(json);
    if (reader.done()) return CodeLanguage{};
    reader.expect('{');
    if (reader.peek('}')) {
        reader.expect('}');
        return CodeLanguage{};
    }
    while (true) {
        const auto key = reader.string();
        reader.expect(':');
        if (key == L"case_sensitive") language.caseSensitive = reader.boolean();
        else if (key == L"keywords") language.keywords = reader.strings();
        else if (key == L"types") language.types = reader.strings();
        else if (key == L"functions") language.functions = reader.strings();
        else if (key == L"constants") language.constants = reader.strings();
        else if (key == L"line_comments") language.lineComments = reader.strings();
        else if (key == L"nested_block_comments") language.nestedBlockComments = reader.boolean();
        else if (key == L"backslash_escapes") language.backslashEscapes = reader.boolean();
        else if (key == L"dollar_quotes") language.dollarQuotes = reader.boolean();
        else if (key == L"string_quotes") language.stringQuotes = reader.string();
        else if (key == L"parameter_prefixes") language.parameterPrefixes = reader.string();
        else if (key == L"block_comments") {
            reader.expect('[');
            if (!reader.peek(']')) {
                while (true) {
                    const auto pair = reader.strings();
                    if (pair.size() != 2 || pair[0].empty() || pair[1].empty()) reader.fail("block_comments needs [open, close] pairs");
                    language.blockComments.emplace_back(pair[0], pair[1]);
                    if (reader.peek(',')) { reader.expect(','); continue; }
                    break;
                }
            }
            reader.expect(']');
        } else if (key == L"identifier_quotes") {
            for (const auto& pair : reader.strings()) {
                if (pair.size() != 2) reader.fail("identifier_quotes entries are two characters: open and close");
                language.identifierQuotes.emplace_back(pair[0], pair[1]);
            }
        } else {
            reader.skip();
        }
        if (reader.peek(',')) {
            reader.expect(',');
            continue;
        }
        reader.expect('}');
        break;
    }
    if (!reader.done()) reader.fail("trailing characters");
    return language;
}

} // namespace oneui
