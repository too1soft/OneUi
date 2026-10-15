#include "code_editor_impl.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

namespace oneui {
namespace {

constexpr float topPadding = 4.0f;
constexpr float scrollbarSize = 10.0f;

bool wideCharacter(wchar_t c) {
    return (c >= 0x1100 && c <= 0x115f) || (c >= 0x2e80 && c <= 0xa4cf) || (c >= 0xac00 && c <= 0xd7a3) ||
           (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe30 && c <= 0xfe4f) || (c >= 0xff00 && c <= 0xff60) ||
           (c >= 0xffe0 && c <= 0xffe6) || (c >= 0xd800 && c <= 0xdbff);
}

bool lowSurrogate(wchar_t c) { return c >= 0xdc00 && c <= 0xdfff; }

} // namespace

namespace code {

double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

Position shiftThrough(Position position, const Edit& edit) {
    if (position < edit.start) return position;
    const Position removedEnd = Document::advance(edit.start, edit.removed);
    const Position insertedEnd = Document::advance(edit.start, edit.inserted);
    if (position < removedEnd) return insertedEnd;
    if (position.line == removedEnd.line) return {insertedEnd.line, insertedEnd.column + (position.column - removedEnd.column)};
    return {position.line - (removedEnd.line - edit.start.line) + (insertedEnd.line - edit.start.line), position.column};
}

} // namespace code

CodeEditorImpl::CodeEditorImpl(CodeEditor& editor) : owner(editor) {
    lexer.reset(doc);
}

float CodeEditorImpl::advance(wchar_t c) const {
    if (lowSurrogate(c)) return 0.0f;
    const float base = ascii[static_cast<std::size_t>('M')] > 0.0f ? ascii[static_cast<std::size_t>('M')] : fontSize * 0.6f;
    if (c < 128) {
        const float value = ascii[static_cast<std::size_t>(c)];
        return value > 0.0f ? value : base;
    }
    const auto found = wide.find(c);
    if (found != wide.end()) return found->second;
    return wideCharacter(c) ? base * 2.0f : base;
}

void CodeEditorImpl::measure(Canvas& canvas) const {
    if (!measured) {
        for (int c = 32; c < 127; ++c)
            ascii[static_cast<std::size_t>(c)] =
                canvas.measureTextWidthWithFont(std::wstring(1, static_cast<wchar_t>(c)), fontSize, TextFontFamily::Monospace);
        ascii[static_cast<std::size_t>('\t')] = ascii[static_cast<std::size_t>(' ')];
        wide.clear();
        measured = true;
        rowsValid = false;
    }
}

float CodeEditorImpl::gutterWidth() const {
    if (!lineNumbers) return 8.0f;
    std::size_t digits = 1;
    for (auto n = doc.lineCount(); n >= 10; n /= 10) ++digits;
    return static_cast<float>(std::max<std::size_t>(digits, 3)) * advance(L'0') + 22.0f;
}

Rect CodeEditorImpl::textRect() const {
    const Rect f = owner.frame();
    const float gutter = gutterWidth();
    const float bottom = wrap ? 0.0f : scrollbarSize;
    return Rect{f.x + gutter + 6.0f, f.y, std::max(0.0f, f.width - gutter - 6.0f - scrollbarSize),
                std::max(0.0f, f.height - bottom)};
}

float CodeEditorImpl::columnX(std::size_t line, std::size_t segmentStart, std::size_t column) const {
    const auto& text = doc.line(line);
    const float tab = std::max(1, tabSize) * spaceWidth();
    float x = 0.0f;
    column = std::min(column, text.size());
    for (auto i = segmentStart; i < column; ++i) {
        if (text[i] == L'\t') x += tab - std::fmod(x, tab);
        else x += advance(text[i]);
    }
    return x;
}

std::vector<std::size_t> CodeEditorImpl::segments(std::size_t line) const {
    std::vector<std::size_t> result{0};
    if (!wrap) return result;
    const float width = textRect().width - 4.0f;
    if (width < advance(L'M') * 4.0f) return result;
    const auto& text = doc.line(line);
    const float tab = std::max(1, tabSize) * spaceWidth();
    float x = 0.0f;
    std::size_t start = 0, lastSpace = std::wstring::npos;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const float w = text[i] == L'\t' ? tab - std::fmod(x, tab) : advance(text[i]);
        if (x + w > width && i > start && !lowSurrogate(text[i])) {
            const std::size_t at = lastSpace != std::wstring::npos && lastSpace > start ? lastSpace + 1 : i;
            result.push_back(at);
            start = at;
            lastSpace = std::wstring::npos;
            x = columnX(line, start, i);
        }
        x += w;
        if (text[i] == L' ' || text[i] == L'\t') lastSpace = i;
    }
    return result;
}

void CodeEditorImpl::ensureRows() const {
    if (!wrap) return;
    const float width = textRect().width;
    if (rowsValid && rowsWidth == width && rowStart.size() == doc.lineCount() + 1) return;
    rowStart.assign(doc.lineCount() + 1, 0);
    for (std::size_t line = 0; line < doc.lineCount(); ++line) rowStart[line + 1] = rowStart[line] + segments(line).size();
    rowsWidth = width;
    rowsValid = true;
}

std::size_t CodeEditorImpl::rowCount() const {
    if (!wrap) return doc.lineCount();
    ensureRows();
    return rowStart.back();
}

std::size_t CodeEditorImpl::rowOfLine(std::size_t line) const {
    if (!wrap) return line;
    ensureRows();
    return rowStart[std::min(line, doc.lineCount())];
}

std::pair<std::size_t, std::size_t> CodeEditorImpl::lineOfRow(std::size_t row) const {
    if (!wrap) return {std::min(row, doc.lineCount() - 1), 0};
    ensureRows();
    row = std::min(row, rowStart.back() - 1);
    const auto found = std::upper_bound(rowStart.begin(), rowStart.end(), row);
    const auto line = static_cast<std::size_t>(found - rowStart.begin()) - 1;
    return {line, row - rowStart[line]};
}

std::size_t CodeEditorImpl::rowOf(code::Position position) const {
    if (!wrap) return position.line;
    const auto segs = segments(position.line);
    std::size_t index = 0;
    while (index + 1 < segs.size() && segs[index + 1] <= position.column) ++index;
    return rowOfLine(position.line) + index;
}

float CodeEditorImpl::contentHeight() const {
    return static_cast<float>(rowCount()) * lineHeight + topPadding * 2.0f;
}

float CodeEditorImpl::contentWidth() const {
    return static_cast<float>(longestLine) * advance(L'M') + 40.0f;
}

float CodeEditorImpl::maxScroll() const {
    // Allow scrolling the last line to the top half of the view.
    return std::max(0.0f, contentHeight() - textRect().height * 0.5f - lineHeight);
}

float CodeEditorImpl::maxHorizontal() const {
    return wrap ? 0.0f : std::max(0.0f, contentWidth() - textRect().width);
}

Rect CodeEditorImpl::caretRect(code::Position position) const {
    position = doc.clamp(position);
    const Rect text = textRect();
    const auto row = rowOf(position);
    std::size_t segmentStart = 0;
    if (wrap) {
        const auto segs = segments(position.line);
        segmentStart = segs[row - rowOfLine(position.line)];
    }
    const float x = text.x + columnX(position.line, segmentStart, position.column) - horizontal;
    const float y = text.y + topPadding + static_cast<float>(row) * lineHeight - scroll;
    return Rect{x, y, 2.0f, lineHeight};
}

code::Position CodeEditorImpl::hit(Point point) const {
    const Rect text = textRect();
    const float rowY = (point.y - text.y - topPadding + scroll) / lineHeight;
    const auto rows = rowCount();
    const auto row = static_cast<std::size_t>(std::clamp(std::floor(rowY), 0.0f, static_cast<float>(rows - 1)));
    const auto [line, segment] = lineOfRow(row);
    const auto& content = doc.line(line);
    std::size_t start = 0, end = content.size();
    if (wrap) {
        const auto segs = segments(line);
        start = segs[segment];
        if (segment + 1 < segs.size()) end = segs[segment + 1];
    }
    const float target = point.x - text.x + horizontal;
    const float tab = std::max(1, tabSize) * spaceWidth();
    float x = 0.0f;
    for (auto i = start; i < end; ++i) {
        if (lowSurrogate(content[i])) continue;
        float w = content[i] == L'\t' ? tab - std::fmod(x, tab) : advance(content[i]);
        if (i + 1 < content.size() && lowSurrogate(content[i + 1])) w += 0.0f;
        if (x + w * 0.5f > target) return {line, i};
        x += w;
    }
    return {line, end};
}

void CodeEditorImpl::setScroll(float value) {
    const float next = std::clamp(value, 0.0f, maxScroll());
    if (next == scroll) return;
    scroll = next;
    owner.invalidate();
}

void CodeEditorImpl::setHorizontal(float value) {
    const float next = std::clamp(value, 0.0f, maxHorizontal());
    if (next == horizontal) return;
    horizontal = next;
    owner.invalidate();
}

void CodeEditorImpl::reveal(code::Position position) {
    const Rect text = textRect();
    const auto row = rowOf(doc.clamp(position));
    const float top = static_cast<float>(row) * lineHeight;
    if (top < scroll) setScroll(top);
    else if (top + lineHeight + topPadding * 2.0f > scroll + text.height)
        setScroll(top + lineHeight + topPadding * 2.0f - text.height);
    if (!wrap) {
        const Rect caret = caretRect(position);
        const float left = caret.x - text.x;
        if (left < 0.0f) setHorizontal(horizontal + left - 20.0f);
        else if (left > text.width - 20.0f) setHorizontal(horizontal + left - text.width + 40.0f);
    }
}

Rect CodeEditorImpl::verticalThumb() const {
    const Rect f = owner.frame();
    const Rect text = textRect();
    const float content = contentHeight() + text.height * 0.5f;
    if (maxScroll() <= 0.0f) return {};
    const float height = std::max(24.0f, text.height * text.height / content);
    const float y = text.y + (text.height - height) * (scroll / maxScroll());
    return Rect{f.x + f.width - scrollbarSize + 2.0f, y, scrollbarSize - 4.0f, height};
}

Rect CodeEditorImpl::horizontalThumb() const {
    if (wrap || maxHorizontal() <= 0.0f) return {};
    const Rect f = owner.frame();
    const Rect text = textRect();
    const float width = std::max(24.0f, text.width * text.width / contentWidth());
    const float x = text.x + (text.width - width) * (horizontal / maxHorizontal());
    return Rect{x, f.y + f.height - scrollbarSize + 2.0f, width, scrollbarSize - 4.0f};
}

} // namespace oneui
