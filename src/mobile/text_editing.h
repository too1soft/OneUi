#pragma once
#include "oneui/mobile/text_input.h"
#include <algorithm>
#include <climits>

namespace oneui::mobile::editing {
inline bool high(char16_t c) { return c >= 0xd800 && c <= 0xdbff; }
inline bool low(char16_t c) { return c >= 0xdc00 && c <= 0xdfff; }
inline bool boundary(const std::u16string &s, int p) {
    return p >= 0 && size_t(p) <= s.size() &&
           !(p > 0 && size_t(p) < s.size() && high(s[p - 1]) && low(s[p]));
}
inline bool decode(const std::u16string &s, std::wstring &out) {
    out.clear();
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t cp = s[i];
        if (high(s[i])) {
            if (++i == s.size() || !low(s[i]))
                return false;
            cp = 0x10000 + ((cp - 0xd800) << 10) + s[i] - 0xdc00;
        } else if (low(s[i]))
            return false;
#if WCHAR_MAX <= 0xffff
        if (cp > 0xffff) {
            out += wchar_t(0xd800 + ((cp - 0x10000) >> 10));
            out += wchar_t(0xdc00 + ((cp - 0x10000) & 0x3ff));
        } else
#endif
            out += wchar_t(cp);
    }
    return true;
}
inline std::u16string encode(const std::wstring &s) {
    std::u16string out;
    for (auto c : s) {
        const auto cp = uint32_t(c);
        if (cp > 0xffff) {
            out += char16_t(0xd800 + ((cp - 0x10000) >> 10));
            out += char16_t(0xdc00 + ((cp - 0x10000) & 0x3ff));
        } else
            out += char16_t(cp);
    }
    return out;
}
inline size_t wideOffset(const std::u16string &s, int p) {
    std::wstring result;
    decode(s.substr(0, size_t(p)), result);
    return result.size();
}

inline int step(const std::u16string &text, int from, int direction, int count) {
    while (count-- > 0 && (direction < 0 ? from > 0 : size_t(from) < text.size())) {
        from += direction;
        if (!boundary(text, from))
            from += direction;
    }
    return from;
}
// Commands operate on the private UTF-16 buffer. The Android password
// connection has only length/selection metadata, so it never diffs bullets.
inline bool edit(EditingState &s, SensitiveEdit command, const std::u16string &text, int first,
                 int second) {
    const int length = int(s.text.size());
    int from = std::min(s.anchor, s.caret), to = std::max(s.anchor, s.caret);
    switch (command) {
    case SensitiveEdit::Commit:
    case SensitiveEdit::Compose: {
        if (text.size() > 1024 * 1024)
            return false;
        std::wstring decoded;
        if (!decode(text, decoded))
            return false;
        if (s.composingStart >= 0) {
            from = s.composingStart;
            to = s.composingEnd;
        }
        if (s.text.size() - (to - from) + text.size() > 1024 * 1024)
            return false;
        s.text.replace(from, to - from, text);
        const int64_t cursor =
            int64_t(from) + (first > 0 ? int64_t(text.size()) + first - 1 : first);
        s.anchor = s.caret = int(std::clamp<int64_t>(cursor, 0, s.text.size()));
        s.composingStart = command == SensitiveEdit::Compose && !text.empty() ? from : -1;
        s.composingEnd = s.composingStart >= 0 ? from + int(text.size()) : -1;
        return boundary(s.text, s.caret);
    }
    case SensitiveEdit::Select:
        if (!boundary(s.text, first) || !boundary(s.text, second))
            return false;
        s.anchor = first;
        s.caret = second;
        return true;
    case SensitiveEdit::Region:
        from = std::clamp(std::min(first, second), 0, length);
        to = std::clamp(std::max(first, second), 0, length);
        if (!boundary(s.text, from) || !boundary(s.text, to))
            return false;
        s.composingStart = from == to ? -1 : from;
        s.composingEnd = from == to ? -1 : to;
        return true;
    case SensitiveEdit::Finish:
        s.composingStart = s.composingEnd = -1;
        return true;
    case SensitiveEdit::DeleteUnits:
    case SensitiveEdit::DeleteCodePoints: {
        if (first < 0 || second < 0)
            return false;
        if (s.composingStart >= 0) {
            from = std::min(from, s.composingStart);
            to = std::max(to, s.composingEnd);
        }
        const int start = command == SensitiveEdit::DeleteUnits ? from - std::min(first, from)
                                                                : step(s.text, from, -1, first);
        const int end = command == SensitiveEdit::DeleteUnits ? to + std::min(second, length - to)
                                                              : step(s.text, to, 1, second);
        if (!boundary(s.text, start) || !boundary(s.text, end))
            return false;
        s.text.erase(to, end - to);
        s.text.erase(start, from - start);
        s.anchor -= from - start;
        s.caret -= from - start;
        if (s.composingStart >= 0) {
            s.composingStart -= from - start;
            s.composingEnd -= from - start;
        }
        return true;
    }
    case SensitiveEdit::Move:
        if (first != -1 && first != 1)
            return false;
        s.caret =
            second == 0 && from != to ? (first < 0 ? from : to) : step(s.text, s.caret, first, 1);
        if (!second)
            s.anchor = s.caret;
        return true;
    }
    return false;
}
} // namespace oneui::mobile::editing
