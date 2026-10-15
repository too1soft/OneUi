#include "code_editor_impl.h"

#include <algorithm>
#include <cwctype>
#include <regex>

namespace oneui {
namespace {

constexpr std::size_t matchLimit = 100000;

bool wordChar(wchar_t c) { return c == L'_' || std::iswalnum(c) || c >= 0x80; }

std::wstring lower(std::wstring text) {
    for (auto& c : text) c = static_cast<wchar_t>(std::towlower(c));
    return text;
}

bool wholeWordAt(const std::wstring& text, std::size_t start, std::size_t end) {
    return (start == 0 || !wordChar(text[start - 1])) && (end >= text.size() || !wordChar(text[end]));
}

std::wregex compile(const std::wstring& query, const CodeSearchOptions& options) {
    auto flags = std::regex_constants::ECMAScript;
    if (!options.matchCase) flags |= std::regex_constants::icase;
    return std::wregex(query, flags);
}

} // namespace

void CodeEditorImpl::refreshMatches() {
    if (!matchesDirty) return;
    matchesDirty = false;
    matches.clear();
    searchError.clear();
    current = -1;
    if (query.empty()) return;
    if (options.regex) {
        std::wregex expression;
        try {
            expression = compile(query, options);
        } catch (const std::regex_error&) {
            searchError = L"Invalid regular expression";
            return;
        }
        for (std::size_t line = 0; line < doc.lineCount() && matches.size() < matchLimit; ++line) {
            const auto& text = doc.line(line);
            for (std::wsregex_iterator it(text.begin(), text.end(), expression), end; it != end; ++it) {
                const auto start = static_cast<std::size_t>(it->position(0));
                const auto length = static_cast<std::size_t>(it->length(0));
                if (length == 0) continue;
                if (options.wholeWord && !wholeWordAt(text, start, start + length)) continue;
                matches.push_back({{line, start}, {line, start + length}});
                if (matches.size() >= matchLimit) break;
            }
        }
    } else {
        const auto needle = options.matchCase ? query : lower(query);
        for (std::size_t line = 0; line < doc.lineCount() && matches.size() < matchLimit; ++line) {
            const auto& raw = doc.line(line);
            if (raw.size() < needle.size()) continue;
            const auto text = options.matchCase ? raw : lower(raw);
            for (auto at = text.find(needle); at != std::wstring::npos; at = text.find(needle, at + 1)) {
                if (options.wholeWord && !wholeWordAt(text, at, at + needle.size())) continue;
                matches.push_back({{line, at}, {line, at + needle.size()}});
                at += needle.size() - 1;
                if (matches.size() >= matchLimit) break;
            }
        }
    }
    const auto caret = selections[std::min(primary, selections.size() - 1)].start();
    for (std::size_t i = 0; i < matches.size(); ++i) {
        if (caret <= matches[i].start) {
            current = static_cast<int>(i);
            break;
        }
    }
    if (current < 0 && !matches.empty()) current = 0;
}

void CodeEditorImpl::selectMatch(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= matches.size()) return;
    current = index;
    const auto& m = matches[static_cast<std::size_t>(index)];
    selections = {code::Selection{m.start, m.end}};
    primary = 0;
    reveal(m.end);
    reveal(m.start);
    cursorMoved();
}

bool CodeEditorImpl::findNext(bool backwards) {
    refreshMatches();
    if (matches.empty()) return false;
    const auto& s = selections[std::min(primary, selections.size() - 1)];
    int next = -1;
    if (!backwards) {
        for (std::size_t i = 0; i < matches.size(); ++i) {
            if (s.end() <= matches[i].start) {
                next = static_cast<int>(i);
                break;
            }
        }
        if (next < 0) next = 0;
    } else {
        for (std::size_t i = matches.size(); i-- > 0;) {
            if (matches[i].end <= s.start() && !(s.start() == matches[i].start && s.end() == matches[i].end)) {
                next = static_cast<int>(i);
                break;
            }
        }
        if (next < 0) next = static_cast<int>(matches.size()) - 1;
    }
    selectMatch(next);
    return true;
}

bool CodeEditorImpl::replaceCurrent(const std::wstring& replacement) {
    if (readOnly) return false;
    refreshMatches();
    if (matches.empty()) return false;
    const auto& s = selections[std::min(primary, selections.size() - 1)];
    const auto found = std::find_if(matches.begin(), matches.end(),
                                    [&](const code::Match& m) { return m.start == s.start() && m.end == s.end(); });
    if (found == matches.end()) return findNext(false);
    std::wstring text = replacement;
    if (options.regex) {
        try {
            const auto expression = compile(query, options);
            text = std::regex_replace(doc.text(found->start, found->end), expression, replacement,
                                      std::regex_constants::format_first_only);
        } catch (const std::regex_error&) {
            return false;
        }
    }
    selections = {code::Selection{found->start, found->end}};
    primary = 0;
    replaceSelections(text, code::EditKind::Other);
    refreshMatches();
    findNext(false);
    return true;
}

std::size_t CodeEditorImpl::replaceAll(const std::wstring& replacement) {
    if (readOnly) return 0;
    refreshMatches();
    if (matches.empty()) return 0;
    std::wregex expression;
    if (options.regex) {
        try {
            expression = compile(query, options);
        } catch (const std::regex_error&) {
            return 0;
        }
    }
    std::vector<code::Selection> ranges;
    std::vector<std::wstring> texts;
    for (const auto& m : matches) {
        ranges.push_back({m.start, m.end});
        texts.push_back(options.regex ? std::regex_replace(doc.text(m.start, m.end), expression, replacement,
                                                           std::regex_constants::format_first_only)
                                      : replacement);
    }
    const auto count = matches.size();
    selections = std::move(ranges);
    primary = 0;
    // One undo step for the whole replacement.
    replaceSelections(texts, code::EditKind::Other);
    selections = {selections.front()};
    primary = 0;
    refreshMatches();
    return count;
}

} // namespace oneui
