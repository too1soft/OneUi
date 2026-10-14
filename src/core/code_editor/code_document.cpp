#include "code_document.h"

#include "internal/unicode.h"

#include <algorithm>

namespace oneui::code {

std::wstring normalizeNewlines(std::wstring_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            result.push_back(L'\n');
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
        } else {
            result.push_back(text[i]);
        }
    }
    return result;
}

std::size_t utf8Length(std::wstring_view text) {
    std::size_t bytes = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<std::uint32_t>(text[i]);
        if (c < 0x80) bytes += 1;
        else if (c < 0x800) bytes += 2;
        else if (c >= 0xd800 && c <= 0xdbff && i + 1 < text.size() &&
                 text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff) {
            bytes += 4;
            ++i;
        } else bytes += 3;
    }
    return bytes;
}

Document::Document() { setText({}); }

void Document::setText(std::wstring_view text) {
    lines_.clear();
    const auto normalized = normalizeNewlines(text);
    std::size_t start = 0;
    while (true) {
        const auto end = normalized.find(L'\n', start);
        if (end == std::wstring::npos) {
            lines_.emplace_back(normalized.substr(start));
            break;
        }
        lines_.emplace_back(normalized.substr(start, end - start));
        start = end + 1;
    }
    lineBytes_.assign(lines_.size(), 0);
    bytesDirty_.assign(lines_.size(), true);
    lineStart_.assign(lines_.size() + 1, 0);
    prefixValid_ = 0;
}

std::wstring Document::text() const {
    std::size_t size = lines_.size() - 1;
    for (const auto& line : lines_) size += line.size();
    std::wstring result;
    result.reserve(size);
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (i) result.push_back(L'\n');
        result += lines_[i];
    }
    return result;
}

std::wstring Document::text(Position from, Position to) const {
    from = clamp(from);
    to = clamp(to);
    if (to < from) std::swap(from, to);
    if (from.line == to.line) return lines_[from.line].substr(from.column, to.column - from.column);
    std::wstring result = lines_[from.line].substr(from.column);
    for (auto line = from.line + 1; line < to.line; ++line) {
        result.push_back(L'\n');
        result += lines_[line];
    }
    result.push_back(L'\n');
    result += lines_[to.line].substr(0, to.column);
    return result;
}

Position Document::clamp(Position position) const {
    position.line = std::min(position.line, lines_.size() - 1);
    position.column = std::min(position.column, lines_[position.line].size());
    // Never split a surrogate pair.
    const auto& line = lines_[position.line];
    if (position.column > 0 && position.column < line.size() &&
        line[position.column] >= 0xdc00 && line[position.column] <= 0xdfff &&
        line[position.column - 1] >= 0xd800 && line[position.column - 1] <= 0xdbff)
        --position.column;
    return position;
}

Position Document::advance(Position at, std::wstring_view text) {
    const auto last = text.rfind(L'\n');
    if (last == std::wstring_view::npos) return {at.line, at.column + text.size()};
    return {at.line + static_cast<std::size_t>(std::count(text.begin(), text.end(), L'\n')), text.size() - last - 1};
}

void Document::touched(std::size_t line) {
    prefixValid_ = std::min(prefixValid_, line);
}

Position Document::insert(Position at, std::wstring_view raw, LineChange* change) {
    at = clamp(at);
    const auto text = normalizeNewlines(raw);
    auto& current = lines_[at.line];
    const auto firstBreak = text.find(L'\n');
    if (firstBreak == std::wstring::npos) {
        current.insert(at.column, text);
        bytesDirty_[at.line] = true;
        touched(at.line);
        if (change) *change = {at.line, 1, 1};
        return {at.line, at.column + text.size()};
    }
    std::wstring tail = current.substr(at.column);
    current.erase(at.column);
    current += text.substr(0, firstBreak);
    std::vector<std::wstring> added;
    std::size_t start = firstBreak + 1;
    while (true) {
        const auto end = text.find(L'\n', start);
        if (end == std::wstring::npos) {
            added.emplace_back(text.substr(start));
            break;
        }
        added.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    const Position after{at.line + added.size(), added.back().size()};
    added.back() += tail;
    const auto insertAt = static_cast<std::ptrdiff_t>(at.line + 1);
    lines_.insert(lines_.begin() + insertAt, std::make_move_iterator(added.begin()), std::make_move_iterator(added.end()));
    lineBytes_.insert(lineBytes_.begin() + insertAt, added.size(), 0);
    bytesDirty_.insert(bytesDirty_.begin() + insertAt, added.size(), true);
    bytesDirty_[at.line] = true;
    lineStart_.resize(lines_.size() + 1);
    touched(at.line);
    if (change) *change = {at.line, 1, 1 + added.size()};
    return after;
}

void Document::erase(Position from, Position to, LineChange* change) {
    from = clamp(from);
    to = clamp(to);
    if (to < from) std::swap(from, to);
    if (from == to) {
        if (change) *change = {from.line, 1, 1};
        return;
    }
    if (from.line == to.line) {
        lines_[from.line].erase(from.column, to.column - from.column);
    } else {
        lines_[from.line].erase(from.column);
        lines_[from.line] += lines_[to.line].substr(to.column);
        const auto first = static_cast<std::ptrdiff_t>(from.line + 1), last = static_cast<std::ptrdiff_t>(to.line + 1);
        lines_.erase(lines_.begin() + first, lines_.begin() + last);
        lineBytes_.erase(lineBytes_.begin() + first, lineBytes_.begin() + last);
        bytesDirty_.erase(bytesDirty_.begin() + first, bytesDirty_.begin() + last);
        lineStart_.resize(lines_.size() + 1);
    }
    bytesDirty_[from.line] = true;
    touched(from.line);
    if (change) *change = {from.line, to.line - from.line + 1, 1};
}

void Document::ensurePrefix(std::size_t line) const {
    if (line < prefixValid_) return;
    std::size_t offset = prefixValid_ == 0 ? 0 : lineStart_[prefixValid_ - 1] + lineBytes_[prefixValid_ - 1] + 1;
    for (auto i = prefixValid_; i <= line && i < lines_.size(); ++i) {
        if (bytesDirty_[i]) {
            lineBytes_[i] = static_cast<std::uint32_t>(utf8Length(lines_[i]));
            bytesDirty_[i] = false;
        }
        lineStart_[i] = offset;
        offset += lineBytes_[i] + 1;
        prefixValid_ = i + 1;
    }
}

std::size_t Document::byteOffset(Position position) const {
    position = clamp(position);
    ensurePrefix(position.line);
    return lineStart_[position.line] + utf8Length(std::wstring_view(lines_[position.line]).substr(0, position.column));
}

Position Document::fromByteOffset(std::size_t offset) const {
    ensurePrefix(lines_.size() - 1);
    const auto begin = lineStart_.begin();
    const auto endIt = begin + static_cast<std::ptrdiff_t>(lines_.size());
    auto found = std::upper_bound(begin, endIt, offset);
    const auto line = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, (found - begin) - 1));
    std::size_t remaining = offset - lineStart_[line];
    const auto& text = lines_[line];
    std::size_t column = 0;
    while (column < text.size() && remaining > 0) {
        const auto c = static_cast<std::uint32_t>(text[column]);
        std::size_t bytes = c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
        std::size_t units = 1;
        if (c >= 0xd800 && c <= 0xdbff && column + 1 < text.size()) {
            bytes = 4;
            units = 2;
        }
        if (bytes > remaining) break;
        remaining -= bytes;
        column += units;
    }
    return {line, column};
}

std::size_t Document::byteLength() const {
    ensurePrefix(lines_.size() - 1);
    return lineStart_[lines_.size() - 1] + lineBytes_[lines_.size() - 1];
}

std::string Document::utf8(Position from, Position to) const {
    return unicode::toUtf8(text(from, to));
}

} // namespace oneui::code
