#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace oneui::code {

// Line/column in UTF-16 code units.
struct Position {
    std::size_t line = 0;
    std::size_t column = 0;
    friend bool operator==(Position a, Position b) { return a.line == b.line && a.column == b.column; }
    friend bool operator!=(Position a, Position b) { return !(a == b); }
    friend bool operator<(Position a, Position b) { return a.line < b.line || (a.line == b.line && a.column < b.column); }
    friend bool operator<=(Position a, Position b) { return !(b < a); }
};

// Lines [first, first + removed) were replaced by [first, first + inserted).
struct LineChange {
    std::size_t first = 0;
    std::size_t removed = 0;
    std::size_t inserted = 0;
};

// Line-array storage. Each line is a separate buffer, so an edit costs the
// length of the touched lines plus a pointer shift when lines are inserted;
// 100,000-line scripts stay well inside a frame. UTF-8 offsets, used at the
// product boundary, come from cached per-line byte lengths and prefix sums
// that are rebuilt lazily from the first changed line.
class Document {
public:
    Document();

    void setText(std::wstring_view text);
    std::wstring text() const;
    std::wstring text(Position from, Position to) const;
    const std::wstring& line(std::size_t index) const { return lines_[index]; }
    std::size_t lineCount() const { return lines_.size(); }
    Position end() const { return {lines_.size() - 1, lines_.back().size()}; }
    Position clamp(Position position) const;

    // Returns the position after the inserted text.
    Position insert(Position at, std::wstring_view text, LineChange* change = nullptr);
    void erase(Position from, Position to, LineChange* change = nullptr);

    std::size_t byteOffset(Position position) const;
    Position fromByteOffset(std::size_t offset) const;
    std::size_t byteLength() const;
    std::string utf8(Position from, Position to) const;

    // Position after inserting `text` at `at`, without touching the document.
    static Position advance(Position at, std::wstring_view text);

private:
    void touched(std::size_t line);
    void ensurePrefix(std::size_t line) const;

    std::vector<std::wstring> lines_;
    mutable std::vector<std::uint32_t> lineBytes_; // UTF-8 bytes, excluding '\n'
    mutable std::vector<std::size_t> lineStart_;   // prefix sums, valid below prefixValid_
    mutable std::size_t prefixValid_ = 0;
    mutable std::vector<bool> bytesDirty_;
};

// Normalizes CRLF and CR to LF.
std::wstring normalizeNewlines(std::wstring_view text);
std::size_t utf8Length(std::wstring_view text);

} // namespace oneui::code
