#include "code_lexer.h"

#include "code_document.h"

#include <algorithm>
#include <cwctype>

namespace oneui::code {
namespace {

constexpr std::uint32_t kindMask = 0xff000000u;
constexpr std::uint32_t blockState = 0x01000000u;
constexpr std::uint32_t stringState = 0x02000000u;
constexpr std::uint32_t identifierState = 0x03000000u;
constexpr std::uint32_t dollarState = 0x04000000u;

std::wstring upper(std::wstring text) {
    for (auto& c : text) c = static_cast<wchar_t>(std::towupper(c));
    return text;
}

bool identStart(wchar_t c) { return c == L'_' || std::iswalpha(c) || c >= 0x80; }
bool identPart(wchar_t c) { return identStart(c) || std::iswdigit(c); }

bool startsWith(const std::wstring& text, std::size_t at, const std::wstring& prefix) {
    return !prefix.empty() && text.compare(at, prefix.size(), prefix) == 0;
}

void push(std::vector<CodeToken>* out, std::size_t start, std::size_t end, CodeTokenKind kind) {
    if (out && end > start)
        out->push_back({static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(end - start), kind});
}

} // namespace

void Lexer::setLanguage(CodeLanguage language) {
    language_ = std::move(language);
    keywords_.clear();
    types_.clear();
    functions_.clear();
    constants_.clear();
    auto fill = [&](std::unordered_set<std::wstring>& set, const std::vector<std::wstring>& words) {
        for (const auto& word : words) set.insert(language_.caseSensitive ? word : upper(word));
    };
    fill(keywords_, language_.keywords);
    fill(types_, language_.types);
    fill(functions_, language_.functions);
    fill(constants_, language_.constants);
}

void Lexer::reset(const Document& document) {
    endStates_.assign(document.lineCount(), 0);
    std::uint32_t state = 0;
    for (std::size_t line = 0; line < document.lineCount(); ++line) {
        state = lex(document.line(line), state, nullptr);
        endStates_[line] = state;
    }
    relexed_ = document.lineCount();
}

void Lexer::apply(const Document& document, const LineChange& change) {
    const auto first = static_cast<std::ptrdiff_t>(change.first);
    endStates_.erase(endStates_.begin() + first, endStates_.begin() + first + static_cast<std::ptrdiff_t>(change.removed));
    endStates_.insert(endStates_.begin() + first, change.inserted, 0u);
    relexed_ = 0;
    std::uint32_t state = startState(change.first);
    for (auto line = change.first; line < document.lineCount(); ++line) {
        const auto next = lex(document.line(line), state, nullptr);
        ++relexed_;
        if (line >= change.first + change.inserted && endStates_[line] == next) break;
        endStates_[line] = next;
        state = next;
    }
}

std::vector<CodeToken> Lexer::tokens(const Document& document, std::size_t line) const {
    std::vector<CodeToken> result;
    if (line >= document.lineCount()) return result;
    lex(document.line(line), line <= endStates_.size() ? startState(line) : 0, &result);
    return result;
}

bool Lexer::inert(const Document& document, std::size_t line, std::size_t column) const {
    for (const auto& token : tokens(document, line)) {
        if (column >= token.start && column < token.start + token.length)
            return token.kind == CodeTokenKind::String || token.kind == CodeTokenKind::Comment ||
                   token.kind == CodeTokenKind::Identifier;
    }
    return false;
}

std::uint32_t Lexer::dollarTag(const std::wstring& tag) const {
    const auto found = std::find(dollarTags_.begin(), dollarTags_.end(), tag);
    if (found != dollarTags_.end()) return static_cast<std::uint32_t>(found - dollarTags_.begin());
    dollarTags_.push_back(tag);
    return static_cast<std::uint32_t>(dollarTags_.size() - 1);
}

CodeTokenKind Lexer::classify(const std::wstring& word, bool call) const {
    const auto key = language_.caseSensitive ? word : upper(word);
    if (call && functions_.count(key)) return CodeTokenKind::Function;
    if (keywords_.count(key)) return CodeTokenKind::Keyword;
    if (types_.count(key)) return CodeTokenKind::Type;
    if (constants_.count(key)) return CodeTokenKind::Constant;
    if (functions_.count(key) || call) return CodeTokenKind::Function;
    return CodeTokenKind::Plain;
}

std::uint32_t Lexer::lex(const std::wstring& text, std::uint32_t state, std::vector<CodeToken>* out) const {
    const auto& lang = language_;
    std::size_t i = 0;
    const std::size_t n = text.size();

    // Scans a quoted run whose opening quote is already consumed. Returns true
    // when the closing quote was found on this line.
    auto scanQuoted = [&](wchar_t close, bool backslash) {
        while (i < n) {
            if (backslash && text[i] == L'\\') {
                i += 2;
                continue;
            }
            if (text[i] == close) {
                if (i + 1 < n && text[i + 1] == close) {
                    i += 2;
                    continue;
                }
                ++i;
                return true;
            }
            ++i;
        }
        i = n;
        return false;
    };
    auto scanBlock = [&](std::size_t kind, std::uint32_t depth) -> std::uint32_t {
        const auto& pair = lang.blockComments[kind];
        while (i < n) {
            if (startsWith(text, i, pair.second)) {
                i += pair.second.size();
                if (--depth == 0) return 0;
                continue;
            }
            if (lang.nestedBlockComments && startsWith(text, i, pair.first)) {
                i += pair.first.size();
                ++depth;
                continue;
            }
            ++i;
        }
        return blockState | (static_cast<std::uint32_t>(kind) << 16) | std::min<std::uint32_t>(depth, 0xffff);
    };
    auto scanDollar = [&](const std::wstring& tag) {
        const std::wstring close = L"$" + tag + L"$";
        const auto found = text.find(close, i);
        if (found == std::wstring::npos) {
            i = n;
            return false;
        }
        i = found + close.size();
        return true;
    };

    // Resume a construct that spans lines.
    switch (state & kindMask) {
    case blockState: {
        const auto kind = (state >> 16) & 0xff;
        if (kind < lang.blockComments.size()) {
            const auto next = scanBlock(kind, state & 0xffff);
            push(out, 0, i, CodeTokenKind::Comment);
            if (next) return next;
        }
        break;
    }
    case stringState: {
        const auto quote = state & 0xffff;
        if (quote < lang.stringQuotes.size()) {
            const bool closed = scanQuoted(lang.stringQuotes[quote], lang.backslashEscapes);
            push(out, 0, i, CodeTokenKind::String);
            if (!closed) return state;
        }
        break;
    }
    case identifierState: {
        const auto quote = state & 0xffff;
        if (quote < lang.identifierQuotes.size()) {
            const bool closed = scanQuoted(lang.identifierQuotes[quote].second, false);
            push(out, 0, i, CodeTokenKind::Identifier);
            if (!closed) return state;
        }
        break;
    }
    case dollarState: {
        const auto tag = state & 0xffffff;
        if (tag < dollarTags_.size()) {
            const bool closed = scanDollar(dollarTags_[tag]);
            push(out, 0, i, CodeTokenKind::String);
            if (!closed) return state;
        }
        break;
    }
    default: break;
    }

    while (i < n) {
        const wchar_t c = text[i];
        if (std::iswspace(c)) {
            ++i;
            continue;
        }
        const auto start = i;
        bool matched = false;
        for (const auto& prefix : lang.lineComments) {
            if (startsWith(text, i, prefix)) {
                push(out, i, n, CodeTokenKind::Comment);
                return 0;
            }
        }
        for (std::size_t k = 0; k < lang.blockComments.size() && !matched; ++k) {
            if (startsWith(text, i, lang.blockComments[k].first)) {
                i += lang.blockComments[k].first.size();
                const auto next = scanBlock(k, 1);
                push(out, start, i, CodeTokenKind::Comment);
                if (next) return next;
                matched = true;
            }
        }
        if (matched) continue;
        if (const auto quote = lang.stringQuotes.find(c); quote != std::wstring::npos) {
            ++i;
            const bool closed = scanQuoted(c, lang.backslashEscapes);
            push(out, start, i, CodeTokenKind::String);
            if (!closed) return stringState | static_cast<std::uint32_t>(quote);
            continue;
        }
        for (std::size_t k = 0; k < lang.identifierQuotes.size() && !matched; ++k) {
            if (c == lang.identifierQuotes[k].first) {
                ++i;
                const bool closed = scanQuoted(lang.identifierQuotes[k].second, false);
                push(out, start, i, CodeTokenKind::Identifier);
                if (!closed) return identifierState | static_cast<std::uint32_t>(k);
                matched = true;
            }
        }
        if (matched) continue;
        if (c == L'$' && lang.dollarQuotes) {
            std::size_t j = i + 1;
            if (j < n && (text[j] == L'$' || (identStart(text[j]) && !std::iswdigit(text[j])))) {
                while (j < n && text[j] != L'$' && identPart(text[j])) ++j;
                if (j < n && text[j] == L'$') {
                    const std::wstring tag = text.substr(i + 1, j - i - 1);
                    i = j + 1;
                    const bool closed = scanDollar(tag);
                    push(out, start, i, CodeTokenKind::String);
                    if (!closed) return dollarState | dollarTag(tag);
                    continue;
                }
            }
        }
        if (lang.parameterPrefixes.find(c) != std::wstring::npos) {
            std::size_t j = i + 1;
            // :: is a cast, not a parameter.
            const bool cast = c == L':' && j < n && text[j] == L':';
            if (!cast && (c == L'?' || (j < n && identPart(text[j])))) {
                while (j < n && identPart(text[j])) ++j;
                push(out, start, j, CodeTokenKind::Parameter);
                i = j;
                continue;
            }
        }
        if (std::iswdigit(c) || (c == L'.' && i + 1 < n && std::iswdigit(text[i + 1]))) {
            if (c == L'0' && i + 1 < n && (text[i + 1] == L'x' || text[i + 1] == L'X')) {
                i += 2;
                while (i < n && std::iswxdigit(text[i])) ++i;
            } else {
                while (i < n && (std::iswdigit(text[i]) || text[i] == L'.')) ++i;
                if (i < n && (text[i] == L'e' || text[i] == L'E')) {
                    std::size_t j = i + 1;
                    if (j < n && (text[j] == L'+' || text[j] == L'-')) ++j;
                    if (j < n && std::iswdigit(text[j])) {
                        i = j;
                        while (i < n && std::iswdigit(text[i])) ++i;
                    }
                }
            }
            push(out, start, i, CodeTokenKind::Number);
            continue;
        }
        if (identStart(c)) {
            while (i < n && identPart(text[i])) ++i;
            std::size_t j = i;
            while (j < n && (text[j] == L' ' || text[j] == L'\t')) ++j;
            const bool call = j < n && text[j] == L'(';
            if (out) push(out, start, i, classify(text.substr(start, i - start), call));
            continue;
        }
        ++i;
        push(out, start, i, CodeTokenKind::Operator);
    }
    return 0;
}

} // namespace oneui::code
